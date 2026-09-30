#include "NetworkServer.hpp"

NetworkServer::NetworkServer(std::unique_ptr<IServerDriver> driver)
    : driver_(std::move(driver)) {}

void NetworkServer::init(const std::string& address, unsigned short port,
                         const std::string& cert_file, const std::string& key_file,
                         NetworkTransportCallback* callback) {
    driver_->configure(address, port, cert_file, key_file, callback);
}

void NetworkServer::start(int thread_count) { driver_->start(thread_count); }
void NetworkServer::stop() { driver_->stop(); }
