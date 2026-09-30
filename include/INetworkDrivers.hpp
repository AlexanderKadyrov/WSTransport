#ifndef INETWORK_DRIVERS_HPP
#define INETWORK_DRIVERS_HPP

#include "NetworkTransport.h"
#include <string>
#include <vector>

class IServerDriver {
public:
    virtual ~IServerDriver() {};
    virtual void configure(NetworkTransportCallback* callback) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
};

class IClientDriver {
public:
    virtual ~IClientDriver() {};
    virtual void configure(NetworkTransportCallback* callback) = 0;
    virtual void connect() = 0;
    virtual void run() = 0;
    virtual void stop() = 0;
    virtual void send(const std::vector<NetworkByte>& data) = 0;
};

#endif // INETWORK_DRIVERS_HPP
