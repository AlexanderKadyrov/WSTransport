#include "NetworkClient.hpp"

NetworkClient::NetworkClient(std::unique_ptr<IClientDriver> driver)
    : driver_(std::move(driver)) {}

void NetworkClient::init(NetworkTransportCallback* callback) { driver_->configure(callback); }
void NetworkClient::connect(const std::string& host, const std::string& port) { driver_->connect(host, port); }
void NetworkClient::run() { driver_->run(); }
void NetworkClient::stop() { driver_->stop(); }
void NetworkClient::send(const std::vector<NetworkByte>& data) { driver_->send(data); }
