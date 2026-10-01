#include "BoostBeastDrivers.hpp"
#include "NetworkTransport.h"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <queue>
#include <functional>
#include <iostream>
#include <cstdint>
#include <thread>

using work_guard_type = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

// ====================================================================
// РЕАЛИЗАЦИЯ ОБЩЕГО ДВИЖКА (СКРЫТА ВНУТРИ .CPP)
// ====================================================================
class BoostNetworkContext : public INetworkContext {
public:
    boost::asio::io_context ioc;
    boost::asio::ssl::context ssl_ctx;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work;
    std::vector<std::thread> thread_pool;
    int threads_to_run;

    explicit BoostNetworkContext(int thread_count) 
        : ioc(),
          ssl_ctx(boost::asio::ssl::context::tlsv12),
          work(new boost::asio::executor_work_guard<boost::asio::io_context::executor_type>(boost::asio::make_work_guard(ioc))),
          threads_to_run(thread_count)
    {
        ssl_ctx.set_options(boost::asio::ssl::context::default_workarounds |
                            boost::asio::ssl::context::no_sslv2 |
                            boost::asio::ssl::context::no_sslv3 |
                            boost::asio::ssl::context::single_dh_use);
    }

    ~BoostNetworkContext() override {
        stop();
    }

    void start() override {
        if (!thread_pool.empty()) return;
        for (int i = 0; i < threads_to_run; ++i) {
            thread_pool.emplace_back([this]() { ioc.run(); });
        }
    }

    void stop() override {
        work.reset();
        for (auto& th : thread_pool) {
            if (th.joinable()) th.join();
        }
        thread_pool.clear();
    }
};

std::shared_ptr<INetworkContext> create_network_context(int thread_count) {
    return std::make_shared<BoostNetworkContext>(thread_count);
}

// ====================================================================
// ВНУТРЕННИЙ КЛАСС СЕССИИ (БЕЗ ИЗМЕНЕНИЙ ЛОГИКИ СЕТИ)
// ====================================================================
class WebSocketSession : public NetworkTransport, public std::enable_shared_from_this<WebSocketSession> {
public:
    explicit WebSocketSession(
                              boost::asio::ip::tcp::socket socket,
                              boost::asio::ssl::context& ctx,
                              NetworkTransportCallback* callback
                              )
    : strand_(static_cast<boost::asio::io_context&>(socket.get_executor().context())),
    ws_(std::move(socket), ctx),
    callback_(callback)
    {}

    void start_server() {
        ws_.set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::server));
        auto self = shared_from_this();
        ws_.next_layer().async_handshake(boost::asio::ssl::stream_base::server,
                                         boost::asio::bind_executor(strand_, std::bind(&WebSocketSession::on_ssl_handshake_server, self, std::placeholders::_1)));
    }
    
    void start_client(const std::string& host) {
        ws_.set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::client));
        auto self = shared_from_this();
        
        // Настройка SNI для TLS (без этого современные сервера сбросят соединение)
        if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host.c_str())) {
            boost::beast::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
            return handle_error("SSL SNI Setup", ec);
        }
        
        ws_.next_layer().async_handshake(boost::asio::ssl::stream_base::client,
                                         boost::asio::bind_executor(strand_, [self, host](boost::beast::error_code ec) {
            if (ec) return self->handle_error("SSL Handshake", ec);
            self->do_ws_handshake(host);
        }));
    }

    void sendData(const std::vector<NetworkByte>& data) override {
        auto shared_data = std::make_shared<std::vector<uint8_t>>(data.begin(), data.end());
        auto self = shared_from_this();
        boost::asio::post(strand_, [self, shared_data]() {
            bool write_in_progress = !self->write_queue_.empty();
            self->write_queue_.push(std::move(*shared_data));
            if (!write_in_progress) self->do_write();
        });
    }

    boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream> >& stream() { return ws_; }
    boost::asio::io_context::strand& strand() { return strand_; }

private:
    void on_ssl_handshake_server(boost::beast::error_code ec) {
        if (ec) return handle_error("SSL Handshake", ec);
        auto self = shared_from_this();
        ws_.async_accept(boost::asio::bind_executor(strand_, std::bind(&WebSocketSession::on_accept_server, self, std::placeholders::_1)));
    }
    
    void on_accept_server(boost::beast::error_code ec) {
        if (ec) return handle_error("Accept", ec);
        if (callback_) callback_->onConnect(*this);
        do_read();
    }
    
    void do_ws_handshake(const std::string& host) {
        auto self = shared_from_this();
        ws_.async_handshake(host, "/", boost::asio::bind_executor(strand_, [self](boost::beast::error_code ec) {
            if (ec) return self->handle_error("WS Handshake", ec);
            if (self->callback_) self->callback_->onConnect(*self);
            self->do_read();
        }));
    }
    
    void do_read() {
        ws_.async_read(read_buffer_, boost::asio::bind_executor(strand_,
                                                                std::bind(&WebSocketSession::on_read, shared_from_this(), std::placeholders::_1, std::placeholders::_2)));
    }

    void on_read(boost::beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec == boost::beast::websocket::error::closed) {
            if (callback_) callback_->onDisconnect(*this);
            return;
        }
        
        // ДОБАВЬТЕ ЭТУ ПРОВЕРКУ:
        // Игнорируем ошибки принудительной остановки Asio и SSL-обрыва при закрытии
        if (ec == boost::asio::error::operation_aborted ||
            ec == boost::asio::ssl::error::stream_truncated) {
            if (callback_) callback_->onDisconnect(*this);
            return;
        }
        
        if (ec) {
            handle_error("Read", ec);
            return;
        }
        
        std::vector<NetworkByte> data(read_buffer_.size());
        boost::asio::buffer_copy(boost::asio::buffer(data), read_buffer_.data());
        read_buffer_.consume(read_buffer_.size());
        
        if (callback_) {
            callback_->onReceive(*this, data);
        }
        
        do_read();
    }

    void do_write() {
        if (write_queue_.empty()) return;
        ws_.binary(true);
        ws_.async_write(boost::asio::buffer(write_queue_.front()), boost::asio::bind_executor(strand_,
            std::bind(&WebSocketSession::on_write, shared_from_this(), std::placeholders::_1, std::placeholders::_2)));
    }

    void on_write(boost::beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec) return handle_error("Write", ec);
        write_queue_.pop();
        do_write();
    }

    void handle_error(const std::string& context, boost::beast::error_code ec) {
        if (callback_) callback_->onError(*this, context + ": " + ec.message());
    }
    
    boost::asio::io_context::strand strand_;
    boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream> > ws_;
    NetworkTransportCallback* const callback_;
    boost::beast::flat_buffer read_buffer_;
    std::queue<std::vector<uint8_t>> write_queue_;
};

// ====================================================================
// REALIZATION: SERVER DRIVER (PIMPL)
// ====================================================================
class BoostServerDriver::Impl {
public:
    std::shared_ptr<BoostNetworkContext> ctx;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor;
    NetworkTransportCallback* callback = nullptr;

    explicit Impl(std::shared_ptr<BoostNetworkContext> shared_ctx) 
        : ctx(shared_ctx) {}

    void do_accept() {
        acceptor->async_accept([this](boost::beast::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                auto session = std::make_shared<WebSocketSession>(std::move(socket), ctx->ssl_ctx, callback);
                session->start_server();
            }
            if (acceptor && acceptor->is_open()) {
                do_accept();
            }
        });
    }
};

BoostServerDriver::BoostServerDriver(
    const std::string& address,
    unsigned short port,
    const std::string& cert_file,
    const std::string& key_file,
    std::shared_ptr<INetworkContext> shared_context
) {
    auto boost_ctx = std::static_pointer_cast<BoostNetworkContext>(shared_context);
    impl_ = std::unique_ptr<Impl>(new Impl(boost_ctx));
    
    impl_->ctx->ssl_ctx.use_certificate_chain_file(cert_file);
    impl_->ctx->ssl_ctx.use_private_key_file(key_file, boost::asio::ssl::context::pem);

    boost::asio::ip::tcp::endpoint ep(boost::asio::ip::make_address(address), port);
    impl_->acceptor = std::unique_ptr<boost::asio::ip::tcp::acceptor>(
        new boost::asio::ip::tcp::acceptor(impl_->ctx->ioc, ep)
    );
}

BoostServerDriver::~BoostServerDriver() { stop(); }

void BoostServerDriver::configure(NetworkTransportCallback* callback) {
    impl_->callback = callback;
    impl_->do_accept(); 
}

void BoostServerDriver::start() {}

void BoostServerDriver::stop() {
    if (impl_->acceptor && impl_->acceptor->is_open()) {
        boost::system::error_code ec;
        impl_->acceptor->close(ec);
    }
}

// ====================================================================
// REALIZATION: CLIENT DRIVER (PIMPL)
// ====================================================================
class BoostClientDriver::Impl {
public:
    std::shared_ptr<BoostNetworkContext> ctx;
    boost::asio::ip::tcp::resolver resolver;
    std::shared_ptr<WebSocketSession> session;
    NetworkTransportCallback* callback = nullptr;
    std::string target_host;
    std::string target_port;

    Impl(std::shared_ptr<BoostNetworkContext> shared_ctx, const std::string& host, const std::string& port)
    : ctx(shared_ctx)
    , resolver(ctx->ioc) // Привязываем ресолвер напрямую к общему ioc
    , target_host(host)
    , target_port(port)
    {}

    void handle_bootstrap_error(const std::string& phase, boost::beast::error_code ec) {
        std::cerr << "[BoostClientDriver] Ошибка: " << phase << " -> " << ec.message() << "\n";
    }
};

BoostClientDriver::BoostClientDriver(
    const std::string& host,
    const std::string& port,
    std::shared_ptr<INetworkContext> shared_context
) {
    auto boost_ctx = std::static_pointer_cast<BoostNetworkContext>(shared_context);
    impl_ = std::unique_ptr<Impl>(new Impl(boost_ctx, host, port));
}

BoostClientDriver::~BoostClientDriver() { stop(); }

void BoostClientDriver::configure(NetworkTransportCallback* callback) {
    impl_->callback = callback;
}

void BoostClientDriver::connect() {
    std::string host = impl_->target_host;
    impl_->resolver.async_resolve(host, impl_->target_port, [this, host](boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type results) {
        if (ec) return impl_->handle_bootstrap_error("Resolve", ec);
        
        boost::asio::ip::tcp::socket socket(impl_->ctx->ioc);
        auto self_session = std::make_shared<WebSocketSession>(std::move(socket), impl_->ctx->ssl_ctx, impl_->callback);
        
        impl_->session = self_session;
        
        boost::beast::get_lowest_layer(self_session->stream()).async_connect(results,
                                                                             boost::asio::bind_executor(self_session->strand(), [this, self_session, host](boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type::endpoint_type) {
            if (ec) {
                impl_->session.reset();
                return impl_->handle_bootstrap_error("Connect", ec);
            }
            self_session->start_client(host);
        }));
    });
}

void BoostClientDriver::start() {}

void BoostClientDriver::stop() {
    if (impl_->session) {
        try {
            boost::beast::get_lowest_layer(impl_->session->stream()).close();
        } catch (...) {}
    }
    impl_->session.reset();
}

void BoostClientDriver::send(const std::vector<NetworkByte>& data) {
    if (impl_->session) {
        impl_->session->sendData(data);
    }
}
