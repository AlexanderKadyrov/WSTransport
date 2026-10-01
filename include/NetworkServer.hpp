#ifndef NETWORK_SERVER_HPP
#define NETWORK_SERVER_HPP

#include "INetworkDrivers.hpp"
#include <memory>
#include <string>

class NetworkServer {
    std::unique_ptr<IServerDriver> driver_;
public:
    explicit NetworkServer(std::unique_ptr<IServerDriver> driver);
    
    void init(NetworkTransportCallback* callback);
              
    void start();
    void stop();
};

#endif // NETWORK_SERVER_HPP
