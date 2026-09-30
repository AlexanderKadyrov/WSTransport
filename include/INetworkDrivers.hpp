#ifndef INETWORK_DRIVERS_HPP
#define INETWORK_DRIVERS_HPP

#include "NetworkTransport.h"
#include <string>
#include <vector>

class INetworkDriver {
public:
    virtual ~INetworkDriver() {}
    virtual void configure(NetworkTransportCallback* callback) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
};

class IServerDriver : public INetworkDriver {
public:
    virtual ~IServerDriver() {}
};

class IClientDriver : public INetworkDriver {
public:
    virtual ~IClientDriver() {}
    virtual void connect() = 0;
    virtual void send(const std::vector<NetworkByte>& data) = 0;
};

#endif // INETWORK_DRIVERS_HPP
