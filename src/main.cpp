#include "NetworkServer.hpp"
#include "NetworkClient.hpp"
#include "BoostBeastDrivers.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>

class EchoCallback : public NetworkTransportCallback {
    std::string name_;
public:
    explicit EchoCallback(const std::string& name) : name_(name) {}

    void onConnect(NetworkTransport& transport) override {
        std::cout << "[" << name_ << "] Сессия WSS открыта!\n";
        if (name_ == "Client") {
            std::string hello = "Привет от Клиента из onConnect";
            transport.sendData(std::vector<NetworkByte>(hello.begin(), hello.end()));
        }
    }

    void onReceive(NetworkTransport& transport, const std::vector<NetworkByte>& data) override {
        std::string msg(data.begin(), data.end());
        std::cout << "[" << name_ << "] Получено и дешифровано: " << msg << "\n";
        if (name_ == "Server") {
            transport.sendData(data); // Передача ссылки без лишней аллокации памяти
        }
    }

    void onError(NetworkTransport& transport, const std::string& error) override {
        (void)transport;
        std::cerr << "[" << name_ << "] Ошибка сети: " << error << "\n";
    }

    void onDisconnect(NetworkTransport& transport) override {
        (void)transport;
        std::cout << "[" << name_ << "] Сессия WSS закрыта.\n";
    }
};

int main() {
    try {
        EchoCallback server_cb("Server");
        EchoCallback client_cb("Client");

        // 1. Создаем ОБЩИЙ контекст исполнения на 4 потока ОС
        auto net_context = create_network_context(4);

        // 2. Внедряем net_context в конструкторы драйверов сервера и клиента
        auto server_driver = std::unique_ptr<BoostServerDriver>(
            new BoostServerDriver("127.0.0.1", 8080, "res/certs/server.crt", "res/certs/server.key", net_context)
        );
        auto client_driver = std::unique_ptr<BoostClientDriver>(
            new BoostClientDriver("127.0.0.1", "8080", net_context)
        );

        NetworkServer server(std::move(server_driver));
        NetworkClient client(std::move(client_driver));

        // Инициализация фасадов
        server.init(&server_cb);
        client.init(&client_cb);

        // 3. Выделяем физические вычислительные потоки на уровне контекста
        std::cout << "[Main] Запуск единого пула потоков сетевого контекста...\n";
        net_context->start(); 

        // Запуск логических модулей (внутри общего контекста это no-op)
        server.start(); 
        
        std::cout << "[Main] Клиент пытается установить соединение...\n";
        client.connect();
        client.start();

        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Ожидание хендшейков

        // Отправка тестового сообщения
        std::string custom_msg = "Кастомный запрос из функции main на общем io_context";
        std::vector<NetworkByte> data(custom_msg.begin(), custom_msg.end());
        std::cout << "[Main] Отправка ручного сообщения через фасад...\n";
        client.send(data);

        std::this_thread::sleep_for(std::chrono::seconds(3)); // Время на сетевой обмен

        // 4. Корректная и безопасная остановка
        std::cout << "[Main] Инициировано завершение работы...\n";
        client.stop();
        server.stop();
        net_context->stop(); // Только здесь потоки ОС завершают работу
        std::cout << "[Main] Работа успешно завершена.\n";

    } catch (const std::exception& e) {
        std::cerr << "[Main] Критическое исключение: " << e.what() << "\n";
        return 1;
    }
    return 0;
}