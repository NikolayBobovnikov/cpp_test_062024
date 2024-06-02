#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <shared_mutex>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

#include <boost/asio.hpp>
#include <yaml-cpp/yaml.h>

#include "helpers.h"

using boost::asio::ip::tcp;

class Server
{
public:
	Server(const std::string& configFile)
		: m_updatePending(false)
		, m_getRequestCount(0)
		, m_setRequestCount(0)
		, m_recentGetRequestCount(0)
		, m_recentSetRequestCount(0)
	{
		_loadServerConfig(configFile);
		_loadKeyValues();

		m_acceptor = std::make_unique<tcp::acceptor>(m_ioContext, tcp::endpoint(tcp::v4(), m_port));
	}

	void start()
	{
		m_fileWriter = std::thread(&Server::_writeFile, this);
		m_statsLogger = std::thread(&Server::_logStatistics, this);
		m_setRequestWorker = std::thread(&Server::_processSetRequests, this);

		_acceptClients();

		m_ioContext.run();

		for(auto& thread : m_clientThreads)
		{
			if(thread.joinable())
			{
				thread.join();
			}
		}

		m_fileWriter.join();
		m_statsLogger.join();
		m_setRequestWorker.join();
	}

private:
	std::string m_host;
	int m_port;
	int m_statsTimeout;
	std::string m_keyValuesFile;

	std::unordered_map<std::string, std::string> m_config;
	std::unordered_map<std::string, std::pair<int, int>> m_keyStats;
	std::shared_mutex m_configMutex;
	std::condition_variable_any m_cv;
	bool m_updatePending;
	std::atomic<int> m_getRequestCount;
	std::atomic<int> m_setRequestCount;
	std::atomic<int> m_recentGetRequestCount;
	std::atomic<int> m_recentSetRequestCount;
	std::vector<std::thread> m_clientThreads;
	std::thread m_fileWriter;
	std::thread m_statsLogger;
	std::thread m_setRequestWorker;
	boost::asio::io_context m_ioContext;
	std::unique_ptr<tcp::acceptor> m_acceptor;

	ThreadSafeQueue<std::pair<std::string, std::string>> m_setRequestQueue;

	void _loadServerConfig(const std::string& configFile)
	{
		try
		{
			YAML::Node config = YAML::LoadFile(configFile);
			m_host = config["server"]["host"].as<std::string>();
			m_port = config["server"]["port"].as<int>();

			// default to 5 seconds if not specified
			m_statsTimeout = config["server"]["stats_timeout"].as<int>(5);

			// key values file is relative to the current path in "conf" subdir
			auto keyValuesFilePath = std::filesystem::current_path() /=
				config["server"]["key_values_file"].as<std::string>();

			if(!std::filesystem::exists(keyValuesFilePath))
			{
				throw std::invalid_argument(string_format("File %s not found", keyValuesFilePath.c_str()));
			}

			m_keyValuesFile = keyValuesFilePath.string();
		}
		catch(const YAML::Exception& e)
		{
			std::cerr << "Error loading server configuration: " << e.what() << std::endl;
			throw;
		}
	}

	void _loadKeyValues()
	{
		std::ifstream file(m_keyValuesFile);
		std::string line;
		while(std::getline(file, line))
		{
			auto pos = line.find('=');
			if(pos != std::string::npos)
			{
				std::string key = line.substr(0, pos);
				std::string value = line.substr(pos + 1);
				m_config[key] = value;
			}
		}
	}

	void _writeFile()
	{
		while(true)
		{
			std::unique_lock<std::shared_mutex> lock(m_configMutex);
			m_cv.wait(lock, [this] { return m_updatePending; });

			std::string tempFileName = m_keyValuesFile + ".tmp";
			std::ofstream tempFile(tempFileName);
			if(!tempFile)
			{
				std::cerr << "Failed to open temporary file for writing: " << tempFileName << std::endl;
				continue;
			}

			ScopeGuard deleteTempFile([&tempFileName] { std::filesystem::remove(tempFileName); });

			try
			{
				for(const auto& kv : m_config)
				{
					tempFile << kv.first << "=" << kv.second << "\n";
				}
				tempFile.close();

				// Replace the original file with the temporary file
				std::filesystem::rename(tempFileName, m_keyValuesFile);

				// If no exception, dismiss the scope guard to avoid deletion of temp file
				deleteTempFile.dismiss();

				m_updatePending = false;
			}
			catch(const std::exception& e)
			{
				std::cerr << "Exception occurred while writing to file: " << e.what() << std::endl;
			}
		}
	}

	void _processSetRequests()
	{
		while(true)
		{
			auto request = m_setRequestQueue.pop();
			{
				std::unique_lock<std::shared_mutex> lock(m_configMutex);
				m_config[request.first] = request.second;
				m_keyStats[request.first].second++;
				m_updatePending = true;
				m_cv.notify_one();
			}
		}
	}

	void _handleClient(tcp::socket socket)
	{
		try
		{
			char buffer[1024];
			constexpr int commandLength = 4;
			while(true)
			{
				boost::system::error_code error;
				size_t length = socket.read_some(boost::asio::buffer(buffer), error);

				if(error == boost::asio::error::eof || error == boost::asio::error::connection_reset)
				{
					break; // Connection closed by client
				}
				else if(error)
				{
					throw boost::system::system_error(error);
				}

				std::string command(buffer, length);
				std::string response;

				if(command.substr(0, commandLength) == "get ")
				{
					std::string key = command.substr(commandLength);
					{
						std::shared_lock<std::shared_mutex> lock(m_configMutex);
						if(m_config.find(key) != m_config.end())
						{
							response = key + "=" + m_config[key];
							m_keyStats[key].first++;
						}
						else
						{
							response = "Key not found";
						}
						m_getRequestCount++;
						m_recentGetRequestCount++;
					}
				}
				else if(command.substr(0, commandLength) == "set ")
				{
					auto pos = command.find('=');
					if(pos != std::string::npos)
					{
						std::string key = command.substr(commandLength, pos - commandLength);
						std::string value = command.substr(pos + 1);
						m_setRequestQueue.push({key, value});
						response = "Set request queued";
						m_setRequestCount++;
						m_recentSetRequestCount++;
					}
					else
					{
						response = "Invalid command format";
					}
				}
				else
				{
					response = "Unknown command";
				}

				boost::asio::write(socket, boost::asio::buffer(response), error);
				if(error)
				{
					throw boost::system::system_error(error);
				}
			}
		}
		catch(std::exception& e)
		{
			std::cerr << "Exception in client handling: " << e.what() << "\n";
		}
	}

	void _acceptClients()
	{
		auto socket = std::make_shared<tcp::socket>(m_ioContext);
		m_acceptor->async_accept(*socket, [this, socket](boost::system::error_code ec) {
			if(!ec)
			{
				m_clientThreads.emplace_back(std::thread(&Server::_handleClient, this, std::move(*socket)));
			}
			_acceptClients();
		});
	}

	void _logStatistics()
	{
		while(true)
		{
			std::this_thread::sleep_for(std::chrono::seconds(m_statsTimeout));
			int totalGetRequests = m_getRequestCount.load();
			int totalSetRequests = m_setRequestCount.load();
			int recentGetRequests = m_recentGetRequestCount.exchange(0);
			int recentSetRequests = m_recentSetRequestCount.exchange(0);

			std::cout << "==================== Request statistics ====================\n";
			std::cout << "Total Get Requests: " << totalGetRequests << "\n";
			std::cout << "Total Set Requests: " << totalSetRequests << "\n";
			std::cout << "Get Requests in last " << m_statsTimeout << " seconds: " << recentGetRequests << "\n";
			std::cout << "Set Requests in last " << m_statsTimeout << " seconds: " << recentSetRequests << "\n";
			_printStatisticsTable();
		}
	}

	void _printStatisticsTable()
	{
		std::ostringstream oss;
		oss << std::setw(10) << "Request" << " |";
		for(const auto& kv : m_keyStats)
		{
			oss << std::setw(7) << kv.first << " |";
		}
		oss << "\n";

		oss << std::setw(10) << "Get" << " |";
		for(const auto& kv : m_keyStats)
		{
			oss << std::setw(7) << kv.second.first << " |";
		}
		oss << "\n";

		oss << std::setw(10) << "Set" << " |";
		for(const auto& kv : m_keyStats)
		{
			oss << std::setw(7) << kv.second.second << " |";
		}
		oss << "\n";

		std::cout << oss.str();
	}
};
