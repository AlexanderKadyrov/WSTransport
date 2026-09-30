#ifndef NETWORK_TRANSPORT_H
#define NETWORK_TRANSPORT_H

#include <vector>
#include <string>
#include <cstdint>

class NetworkTransport {
public:
    virtual ~NetworkTransport() = default;
    virtual void sendData(const std::vector<uint8_t> data) = 0;
};

class NetworkTransportCallback {
public:
    virtual ~NetworkTransportCallback() = default;
    virtual void onConnect(NetworkTransport& transport) = 0;
    virtual void onReceive(NetworkTransport& transport, const std::vector<uint8_t> data) = 0;
    virtual void onError(NetworkTransport& transport, const std::string error) = 0;
    virtual void onDisconnect(NetworkTransport& transport) = 0;
};

#endif // NETWORK_TRANSPORT_H
