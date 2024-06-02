#include "server.h"
#include <SDKDDKVer.h>

int main()
{
    try
    {
        auto pathToConfig = std::filesystem::current_path() /= "conf/server.yaml";
        Server server(pathToConfig.string());
        server.start();
    }
    catch(std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}