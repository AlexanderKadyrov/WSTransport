#ifndef NETWORK_CLIENT_HPP
#define NETWORK_CLIENT_HPP

#include "INetworkDrivers.hpp"
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

class NetworkClient {
    std::unique_ptr<IClientDriver> driver_;
public:
    explicit NetworkClient(std::unique_ptr<IClientDriver> driver);
    
    void init(NetworkTransportCallback* callback);
    void connect();
    void run();
    void stop();
    void send(const std::vector<NetworkByte>& data);
};

#endif // NETWORK_CLIENT_HPP
