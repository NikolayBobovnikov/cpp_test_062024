#include "server.h"

#include <filesystem>

Server::Server(int port)
    : m_port(port), m_acceptor(m_ioContext, tcp::endpoint(tcp::v4(), port)), m_updatePending(false), m_getRequestCount(0), m_setRequestCount(0)
{
    _loadConfig();
}

void Server::start()
{
    m_fileWriter = std::thread(&Server::_writeFile, this);
    m_statsLogger = std::thread(&Server::_logStatistics, this);

    _acceptClients();

    m_ioContext.run();

    for (auto &thread : m_clientThreads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    m_fileWriter.join();
    m_statsLogger.join();
}

void Server::_loadConfig()
{
    auto pathToConfig = std::filesystem::current_path() /= "conf/values.txt";
    std::ifstream file(pathToConfig.string());
    std::string line;
    while (std::getline(file, line))
    {
        auto pos = line.find('=');
        if (pos != std::string::npos)
        {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            m_config[key] = value;
        }
    }
}

void Server::_writeFile()
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(m_configMutex);
        m_cv.wait(lock, [this]
                  { return m_updatePending; });

        std::ofstream file("config.txt");
        for (const auto &kv : m_config)
        {
            file << kv.first << "=" << kv.second << "\n";
        }
        m_updatePending = false;
    }
}

void Server::_handleClient(tcp::socket socket)
{
    try
    {
        char buffer[1024];
        constexpr int prefixLength = 4; // "get " and "set " prefix
        while (true)
        {
            boost::system::error_code error;
            size_t length = socket.read_some(boost::asio::buffer(buffer), error);

            if (error == boost::asio::error::eof || error == boost::asio::error::connection_reset)
            {
                break; // Connection closed by client
            }
            else if (error)
            {
                throw boost::system::system_error(error);
            }

            std::string command(buffer, length);
            std::string response;

            {
                std::lock_guard<std::mutex> lock(m_configMutex);
                if (command.substr(0, prefixLength) == "get ")
                {
                    std::string key = command.substr(prefixLength);
                    if (m_config.find(key) != m_config.end())
                    {
                        response = key + "=" + m_config[key];
                        m_keyStats[key].first++;
                    }
                    else
                    {
                        response = "Key not found";
                    }
                    m_getRequestCount++;
                }
                else if (command.substr(0, prefixLength) == "set ")
                {
                    auto pos = command.find('=');
                    if (pos != std::string::npos)
                    {
                        std::string key = command.substr(prefixLength, pos - prefixLength);
                        std::string value = command.substr(pos + 1);
                        m_config[key] = value;
                        m_keyStats[key].second++;
                        m_updatePending = true;
                        m_cv.notify_one();
                        response = "Set successful";
                        m_setRequestCount++;
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
            }

            boost::asio::write(socket, boost::asio::buffer(response), error);
            if (error)
            {
                throw boost::system::system_error(error);
            }
        }
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception in client handling: " << e.what() << "\n";
    }
}

void Server::_acceptClients()
{
    auto socket = std::make_shared<tcp::socket>(m_ioContext);
    m_acceptor.async_accept(*socket, [this, socket](boost::system::error_code ec)
                            {
            if (!ec) {
                m_clientThreads.emplace_back(std::thread(&Server::_handleClient, this, std::move(*socket)));
            }
            _acceptClients(); });
}

void Server::_logStatistics()
{
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        int totalGetRequests = m_getRequestCount.load();
        int totalSetRequests = m_setRequestCount.load();

        std::cout << "Total Get Requests: " << totalGetRequests << "\n";
        std::cout << "Total Set Requests: " << totalSetRequests << "\n";
        std::cout << "Access Statistics:\n";
        for (const auto &kv : m_keyStats)
        {
            std::cout << kv.first << " - Reads: " << kv.second.first << ", Writes: " << kv.second.second << "\n";
        }
    }
}
