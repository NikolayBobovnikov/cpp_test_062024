#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "server.h"
#include "session.h"

Server::Server(const std::string& configFile)
    : m_updatePending(false)
    , m_getRequestCount(0)
    , m_setRequestCount(0)
    , m_recentGetRequestCount(0)
    , m_recentSetRequestCount(0)
    , m_acceptor(m_ioContext)
{
    _loadServerConfig(configFile);
    _loadKeyValues();

    tcp::endpoint endpoint(tcp::v4(), m_port);
    m_acceptor.open(endpoint.protocol());
    m_acceptor.set_option(tcp::acceptor::reuse_address(true));
    m_acceptor.bind(endpoint);
    m_acceptor.listen();
}

Server::~Server()
{
    m_ioContext.stop();

    if(m_fileWriter.joinable())
    {
        m_fileWriter.join();
    }
    if(m_statsLogger.joinable())
    {
        m_statsLogger.join();
    }
    if(m_setRequestWorker.joinable())
    {
        m_setRequestWorker.join();
    }
}

void Server::start()
{
    m_fileWriter = std::thread(&Server::_writeFile, this);
    m_statsLogger = std::thread(&Server::_logStatistics, this);
    m_setRequestWorker = std::thread(&Server::_processSetRequests, this);
    _acceptClientConnections();
    m_ioContext.run();
}

void Server::handle_request(std::string_view command, std::string& response)
{
    constexpr int commandLength = 4;

    if(command.substr(0, commandLength) == "get "sv)
    {
        std::string_view key = command.substr(commandLength);
        {
            std::shared_lock<std::shared_mutex> lock(m_configMutex);
            auto it = m_config.find(std::string(key));
            if(it != m_config.end())
            {
                // Create a JSON document
                rapidjson::Document doc;
                doc.SetObject();
                rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

                // Add key-value pair
                doc.AddMember(
                    rapidjson::StringRef(key.data(), key.size()), rapidjson::StringRef(it->second.c_str()), allocator);

                // Add statistics
                auto& stats = m_keyStats[std::string(key)];
                stats.first++;
                doc.AddMember("reads", stats.first, allocator);
                doc.AddMember("writes", stats.second, allocator);

                // Convert JSON document to string
                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                doc.Accept(writer);

                response = buffer.GetString();
            }
            else
            {
                response = "Key not found";
            }
            m_getRequestCount++;
            m_recentGetRequestCount++;
        }
    }
    else if(command.substr(0, commandLength) == "set "sv)
    {
        auto pos = command.find('=');
        if(pos != std::string::npos)
        {
            std::string_view key = command.substr(commandLength, pos - commandLength);
            std::string_view value = command.substr(pos + 1);
            auto keystring = std::string(key);
            response = "OK";
            m_keyStats[keystring].second++;
            m_setRequestCount++;
            m_recentSetRequestCount++;
            m_setRequestQueue.push({keystring, std::string(value)});
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

void Server::_loadServerConfig(const std::string& configFile)
{
    try
    {
        YAML::Node config = YAML::LoadFile(configFile);
        m_host = config["server"]["host"].as<std::string>();
        m_port = config["server"]["port"].as<int>();

        // default to 5 seconds if not specified
        m_statsTimeout = config["server"]["stats_timeout"].as<int>(5);
        m_fileWriteTimeout = config["server"]["file_write_timeout"].as<int>(5);

        // key values file is relative to the current path in "conf" subdir
        auto keyValuesFilePath = std::filesystem::path(configFile).parent_path() /=
            config["server"]["key_values_file"].as<std::string>();

        std::filesystem::create_directories(keyValuesFilePath.parent_path());

        if(!std::filesystem::exists(keyValuesFilePath))
        {
            std::ofstream keyValuesFile(keyValuesFilePath.string());
            if(!keyValuesFile)
            {
                throw std::runtime_error("Failed to create file: " + keyValuesFilePath.string());
            }
            keyValuesFile.close();
        }

        m_keyValuesFile = keyValuesFilePath.string();
    }
    catch(const YAML::Exception& e)
    {
        std::cerr << "Error loading server configuration: " << e.what() << std::endl;
        throw;
    }
}

void Server::_loadKeyValues()
{
    std::ifstream file(m_keyValuesFile);
    if(!file.is_open())
    {
        std::cerr << "Failed to open key values file: " << m_keyValuesFile << std::endl;
        return;
    }

    std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    rapidjson::Document doc;

    if(json.empty())
    {
        std::cerr << "Key values file is empty, creating empty JSON." << std::endl;
        doc.SetObject();
        std::ofstream outFile(m_keyValuesFile);
        if(!outFile.is_open())
        {
            std::cerr << "Failed to open key values file for writing: " << m_keyValuesFile << std::endl;
            return;
        }
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);
        outFile << buffer.GetString();
        outFile.close();
    }
    else
    {
        if(doc.Parse(json.c_str()).HasParseError())
        {
            std::cerr << "Error parsing key values file: " << m_keyValuesFile << std::endl;
            return;
        }

        for(auto itr = doc.MemberBegin(); itr != doc.MemberEnd(); ++itr)
        {
            std::string key = itr->name.GetString();
            std::string value = itr->value.GetString();
            m_config[key] = value;
        }
    }
}
void Server::_writeFile()
{
    while(true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(m_fileWriteTimeout));
        std::unique_lock<std::shared_mutex> lock(m_configMutex);
        m_setRequestQueuedCv.wait(lock, [this] { return m_updatePending.load(); });
        m_writeInProgress = true;
        rapidjson::Document doc;
        doc.SetObject();
        rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

        for(const auto& kv : m_config)
        {
            doc.AddMember(rapidjson::StringRef(kv.first.c_str()), rapidjson::StringRef(kv.second.c_str()), allocator);
        }

        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        std::ofstream keyValuesFile(m_keyValuesFile);
        if(!keyValuesFile.is_open())
        {
            std::cerr << "Failed to open temporary file for writing: " << m_keyValuesFile << std::endl;
            continue;
        }

        keyValuesFile << buffer.GetString();
        keyValuesFile.close();

        if(keyValuesFile.fail())
        {
            std::cerr << "Error while closing the temporary file: " << m_keyValuesFile << std::endl;
            continue;
        }

        m_updatePending = false;
        m_writeInProgress = false;

        // Notify set request processing thread that the file write is completed
        m_writeCompletedCv.notify_all();
    }
}

void Server::_processSetRequests()
{
    while(true)
    {
        auto request = m_setRequestQueue.pop();
        {
            std::unique_lock<std::shared_mutex> lock(m_configMutex);
            m_writeCompletedCv.wait(lock, [this] { return !m_writeInProgress; });
            m_config[request.first] = request.second;
            m_updatePending = true;
            m_setRequestQueuedCv.notify_one();
        }
    }
}

void Server::_acceptClientConnections()
{
    m_acceptor.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
        if(!ec)
        {
            std::make_shared<Session>(std::move(socket), *this)->start();
        }
        _acceptClientConnections();
    });
}

void Server::_logStatistics()
{
    while(true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(m_statsTimeout));
        int totalGetRequests = m_getRequestCount.load();
        int totalSetRequests = m_setRequestCount.load();
        int recentGetRequests = m_recentGetRequestCount.exchange(0);
        int recentSetRequests = m_recentSetRequestCount.exchange(0);

        std::cout << "==================== Request statistics ====================\n";
        std::cout << "Get Requests (in last " << m_statsTimeout << " seconds): " << recentGetRequests << "\n";
        std::cout << "Set Requests (in last " << m_statsTimeout << " seconds): " << recentSetRequests << "\n";
        std::cout << "Get Requests (total): " << totalGetRequests << "\n";
        std::cout << "Set Requests (total): " << totalSetRequests << "\n";
        _printStatisticsTable();
    }
}

void Server::_printStatisticsTable()
{
    static const int width = 10;
    std::ostringstream oss;
    oss << "|" << std::setw(width) << "Request" << " |";
    for(const auto& kv : m_keyStats)
    {
        oss << std::setw(width) << kv.first << " |";
    }
    oss << "\n";

    oss << "|" << std::setw(width) << "Get" << " |";
    for(const auto& kv : m_keyStats)
    {
        oss << std::setw(width) << kv.second.first << " |";
    }
    oss << "\n";

    oss << "|" << std::setw(width) << "Set" << " |";
    for(const auto& kv : m_keyStats)
    {
        oss << std::setw(width) << kv.second.second << " |";
    }
    oss << "\n";

    std::cout << oss.str();
}
