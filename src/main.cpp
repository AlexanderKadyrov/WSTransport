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

        // 1. Создаем сервер
        NetworkServer server(std::unique_ptr<BoostServerDriver>(
            new BoostServerDriver("127.0.0.1", 8080, "res/certs/server.crt", "res/certs/server.key", 4)
        ));
        server.init(&server_cb);

        // 2. Создаем клиента, передавая адрес в его драйвер
        NetworkClient client(std::unique_ptr<BoostClientDriver>(
            new BoostClientDriver("127.0.0.1", "8080")
        ));
        client.init(&client_cb);

        // 3. Запуск сетевых движков на пулах потоков внутри драйверов
        server.start(); // 4 потока для сервера

        // 4. Инициализация подключения
        std::cout << "[Main] Клиент пытается установить соединение...\n";
        client.connect();
        client.run();    // 1 поток для клиента

        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Ожидание завершения хендшейков

        // 5. Произвольная отправка сообщения из бизнес-кода в любой момент времени
        std::string custom_msg = "Кастомный запрос из функции main";
        std::vector<NetworkByte> data(custom_msg.begin(), custom_msg.end());
        std::cout << "[Main] Отправка ручного сообщения через фасад...\n";
        client.send(data);

        std::this_thread::sleep_for(std::chrono::seconds(3)); // Время на сетевой обмен

        // 6. Корректная остановка модулей
        std::cout << "[Main] Инициировано завершение работы...\n";
        client.stop();
        server.stop();
        std::cout << "[Main] Работа успешно завершена.\n";

    } catch (const std::exception& e) {
        std::cerr << "[Main] Критическое исключение: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
