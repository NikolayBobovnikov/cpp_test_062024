#include <SDKDDKVer.h>
#include "server.h"

int main()
{
    try
    {
        Server server(12345);
        server.start();
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
