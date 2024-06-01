#include <iostream>
#include <fstream>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <vector>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class Server
{
public:
    Server(int port);
    void start();

private:
    int m_port;
    std::unordered_map<std::string, std::string> m_config;
    std::unordered_map<std::string, std::pair<int, int>> m_keyStats;
    std::mutex m_configMutex;
    std::condition_variable m_cv;
    bool m_updatePending;
    std::atomic<int> m_getRequestCount;
    std::atomic<int> m_setRequestCount;
    std::vector<std::thread> m_clientThreads;
    std::thread m_fileWriter;
    std::thread m_statsLogger;
    boost::asio::io_context m_ioContext;
    tcp::acceptor m_acceptor;

    void _loadConfig();

    void _writeFile();

    void _handleClient(tcp::socket socket);

    void _acceptClients();

    void _logStatistics();
};
