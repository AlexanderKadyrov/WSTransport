#ifndef NETWORK_TRANSPORT_H
#define NETWORK_TRANSPORT_H

#include <vector>
#include <string>

typedef unsigned char NetworkByte;

class NetworkTransport {
public:
    virtual ~NetworkTransport() {};
    virtual void sendData(const std::vector<NetworkByte>& data) = 0;
};

class NetworkTransportCallback {
public:
    virtual ~NetworkTransportCallback() {};
    virtual void onConnect(NetworkTransport& transport) = 0;
    virtual void onReceive(NetworkTransport& transport, const std::vector<NetworkByte>& data) = 0;
    virtual void onError(NetworkTransport& transport, const std::string& error) = 0;
    virtual void onDisconnect(NetworkTransport& transport) = 0;
};

#endif // NETWORK_TRANSPORT_H
