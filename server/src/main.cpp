#include "server.h"

#include <string>

namespace
{
std::string findConfig()
{
    namespace fs = std::filesystem;

    fs::path currentPath = fs::current_path();
    const std::string configFileName = "server.yaml";
    const std::string configDirName = "conf";

    while(!currentPath.empty())
    {
        fs::path configDirPath = currentPath / configDirName;
        if(fs::exists(configDirPath) && fs::is_directory(configDirPath))
        {
            fs::path configFilePath = configDirPath / configFileName;
            if(fs::exists(configFilePath))
            {
                return configFilePath.string();
            }
        }
        currentPath = currentPath.parent_path();
    }

    throw std::runtime_error("Configuration file 'server.yaml' not found in any 'conf' directory in parent paths.");
}
} // namespace

int main()
{
    try
    {
        Server server(findConfig());
        server.start();
    }
    catch(std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}