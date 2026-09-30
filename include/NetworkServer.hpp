#ifndef NETWORK_SERVER_HPP
#define NETWORK_SERVER_HPP

#include "INetworkDrivers.hpp"
#include <memory>
#include <string>

class NetworkServer {
    std::unique_ptr<IServerDriver> driver_;
public:
    explicit NetworkServer(std::unique_ptr<IServerDriver> driver);
    
    void init(const std::string& address, unsigned short port,
              const std::string& cert_file, const std::string& key_file,
              NetworkTransportCallback* callback);
              
    void start(int thread_count = 4);
    void stop();
};

#endif // NETWORK_SERVER_HPP
