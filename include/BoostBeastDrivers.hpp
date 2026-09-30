#ifndef BOOST_BEAST_DRIVERS_HPP
#define BOOST_BEAST_DRIVERS_HPP

#include "INetworkDrivers.hpp"
#include <memory>
#include <string>
#include <vector>

class BoostServerDriver : public IServerDriver {
    class Impl;
    std::unique_ptr<Impl> impl_;
public:
    BoostServerDriver(
        const std::string& address,
        unsigned short port,
        const std::string& cert_file,
        const std::string& key_file,
        int thread_count = 1
    );
    ~BoostServerDriver() override;

    void configure(NetworkTransportCallback* callback) override;
    void start() override;
    void stop() override;
};

class BoostClientDriver : public IClientDriver {
    class Impl;
    std::unique_ptr<Impl> impl_;
public:
    BoostClientDriver(
        const std::string& host,
        const std::string& port
    );
    ~BoostClientDriver() override;

    void configure(NetworkTransportCallback* callback) override;
    void connect() override;
    void start() override;
    void stop() override;
    void send(const std::vector<NetworkByte>& data) override;
};

#endif // BOOST_BEAST_DRIVERS_HPP
