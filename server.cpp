#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <winsock2.h>
#include <mysql/jdbc.h>

#pragma comment(lib, "ws2_32.lib")

const std::string DB_HOST = "tcp://127.0.0.1:3306";
const std::string DB_USER = "root";
const std::string DB_PASS = "1234";
const std::string DB_NAME = "LOGIN_SERVER_CLIENT";

void handleClient(SOCKET clientSocket) {
    bool loggedIn = false;
    int userId = -1;
    std::vector<char> buffer(1024);

    while (true) {
        int recvLen = recv(clientSocket, buffer.data(), buffer.size(), 0);
        if (recvLen <= 0) break;

        std::string msg(buffer.data(), recvLen);
        std::cout << "[RECV] " << msg << std::endl;

        try {
            sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
            std::unique_ptr<sql::Connection> conn(
                driver->connect(DB_HOST, DB_USER, DB_PASS));
            conn->setSchema(DB_NAME);

            if (msg.rfind("REGISTER:", 0) == 0) {
                size_t pos1 = msg.find(":", 9);
                size_t pos2 = msg.find(":", pos1 + 1);
                if (pos1 == std::string::npos || pos2 == std::string::npos) {
                    send(clientSocket, "Invalid REGISTER Format\n", 25, 0);
                    continue;
                }
                std::string username = msg.substr(9, pos1 - 9);
                std::string password = msg.substr(pos1 + 1, pos2 - pos1 - 1);

                std::unique_ptr<sql::PreparedStatement> check(
                    conn->prepareStatement("SELECT * FROM users WHERE username = ?"));
                check->setString(1, username);
                std::unique_ptr<sql::ResultSet> res(check->executeQuery());

                if (res->next()) {
                    send(clientSocket, "ID already exists\n", 18, 0);
                }
                else {
                    std::unique_ptr<sql::PreparedStatement> insert(
                        conn->prepareStatement("INSERT INTO users(username, password) VALUES(?, ?)"));
                    insert->setString(1, username);
                    insert->setString(2, password);
                    insert->executeUpdate();
                    send(clientSocket, "Register Success\n", 17, 0);
                }

            }
            else if (msg.rfind("LOGIN:", 0) == 0) {
                size_t pos1 = msg.find(":", 6);
                size_t pos2 = msg.find(":", pos1 + 1);
                if (pos1 == std::string::npos || pos2 == std::string::npos) {
                    send(clientSocket, "Invalid LOGIN Format\n", 21, 0);
                    continue;
                }
                std::string username = msg.substr(6, pos1 - 6);
                std::string password = msg.substr(pos1 + 1, pos2 - pos1 - 1);

                std::unique_ptr<sql::PreparedStatement> login(
                    conn->prepareStatement("SELECT * FROM users WHERE username = ? AND password = ?"));
                login->setString(1, username);
                login->setString(2, password);
                std::unique_ptr<sql::ResultSet> res(login->executeQuery());

                if (res->next()) {
                    loggedIn = true;
                    userId = res->getInt("user_id");

                    std::unique_ptr<sql::PreparedStatement> sess(
                        conn->prepareStatement("INSERT INTO user_sessions(user_id, ip_address) VALUES (?, '127.0.0.1')"));
                    sess->setInt(1, userId);
                    sess->executeUpdate();

                    send(clientSocket, "Login Success\n", 14, 0);
                }
                else {
                    send(clientSocket, "Login Failed\n", 13, 0);
                }

            }
            else if (msg.rfind("CHAT:", 0) == 0 && loggedIn) {
                std::string content = msg.substr(5);
                std::unique_ptr<sql::PreparedStatement> chatStmt(
                    conn->prepareStatement("INSERT INTO message_log(sender_id, content) VALUES(?, ?)"));
                chatStmt->setInt(1, userId);
                chatStmt->setString(2, content);
                chatStmt->executeUpdate();

                std::string echo = "Echo: " + content + "\n";
                send(clientSocket, echo.c_str(), echo.length(), 0);

            }
            else if (msg == "exit") {
                if (loggedIn && userId != -1) {
                    std::unique_ptr<sql::PreparedStatement> logout(
                        conn->prepareStatement("UPDATE user_sessions SET logout_time = NOW() WHERE user_id = ? AND logout_time IS NULL"));
                    logout->setInt(1, userId);
                    logout->executeUpdate();
                }
                send(clientSocket, "Logged out.\n", 12, 0);
                break;

            }
            else {
                send(clientSocket, "Unknown Command\n", 16, 0);
            }
        }
        catch (sql::SQLException& e) {
            std::cerr << "MySQL Error: " << e.what() << std::endl;
            send(clientSocket, "DB Error\n", 9, 0);
        }
    }

    closesocket(clientSocket);
    std::cout << "Client disconnected.\n";
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8888);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, SOMAXCONN);
    std::cout << "Server listening on port 8888...\n";

    while (true) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket != INVALID_SOCKET) {
            std::thread(handleClient, clientSocket).detach();
        }
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}