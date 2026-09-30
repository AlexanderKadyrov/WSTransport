#include "NetworkServer.hpp"

NetworkServer::NetworkServer(std::unique_ptr<IServerDriver> driver)
    : driver_(std::move(driver)) {}

void NetworkServer::init(NetworkTransportCallback* callback) {
    driver_->configure(callback);
}

void NetworkServer::start() { driver_->start(); }
void NetworkServer::stop() { driver_->stop(); }
