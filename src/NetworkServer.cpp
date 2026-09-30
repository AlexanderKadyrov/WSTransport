#include "NetworkServer.hpp"

NetworkServer::NetworkServer(std::unique_ptr<IServerDriver> driver)
    : driver_(std::move(driver)) {}

void NetworkServer::init(NetworkTransportCallback* callback) {
    driver_->configure(callback);
}

void NetworkServer::start(int thread_count) { driver_->start(thread_count); }
void NetworkServer::stop() { driver_->stop(); }
