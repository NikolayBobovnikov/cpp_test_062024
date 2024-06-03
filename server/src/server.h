#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <sstream>
#include <string_view>
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
    Server(const std::string& configFile);
    ~Server();

    void start();
    void handle_request(std::string_view data, std::string& response);

private:
    std::string m_host;
    int m_port;
    size_t m_statsTimeout;
    size_t m_fileWriteTimeout;
    std::string m_keyValuesFile;
    std::unordered_map<std::string, std::string> m_config;
    // stats per key requests: first - get, second - set
    std::unordered_map<std::string, std::pair<int, int>> m_keyStats;
    std::shared_mutex m_configMutex;
    std::condition_variable_any m_setRequestQueuedCv;
    std::condition_variable_any m_writeCompletedCv;
    std::atomic<bool> m_updatePending{false};
    std::atomic<bool> m_writeInProgress{false};
    std::atomic<size_t> m_getRequestCount;
    std::atomic<size_t> m_setRequestCount;
    std::atomic<size_t> m_recentGetRequestCount;
    std::atomic<size_t> m_recentSetRequestCount;
    std::thread m_fileWriter;
    std::thread m_statsLogger;
    std::thread m_setRequestWorker;
    boost::asio::io_context m_ioContext;
    tcp::acceptor m_acceptor;
    ThreadSafeQueue<std::pair<std::string, std::string>> m_setRequestQueue;

    void _loadServerConfig(const std::string& configFile);
    void _loadKeyValues();
    void _writeFile();
    void _processSetRequests();
    void _acceptClientConnections();
    void _logStatistics();
    void _printStatisticsTable();
};
