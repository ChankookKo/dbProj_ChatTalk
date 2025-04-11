#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <string>

#pragma comment(lib, "ws2_32.lib")
using namespace std;

string loginStatus = "[LOGOUT]";
string loginUser = "";

string receiveLine(SOCKET sock) {
    char buffer[1024];
    int len = recv(sock, buffer, sizeof(buffer), 0);
    if (len <= 0) return "";
    return string(buffer, len);
}

void sendLine(SOCKET sock, const string& msg) {
    send(sock, msg.c_str(), static_cast<int>(msg.length()), 0);
}

void printMenu() {
    cout << "\n======================CHATTALK======================\n";
    if (loginStatus == "[LOGIN]" && !loginUser.empty())
        cout << "[" << loginUser << "], LOGIN\n";
    else if (loginStatus == "[LOGOUT]" && !loginUser.empty())
        cout << "[" << loginUser << "], LOGOUT\n";
    else
        cout << "[LOGOUT]\n";

    cout << "[CHATTALK MENU]\n";
    if (loginStatus == "[LOGIN]") {
        cout << "3. CHAT\n";
        cout << "4. LOGOUT\n";
    }
    else {
        cout << "1. REGISTER\n";
        cout << "2. LOGIN\n";
    }
    cout << "0. EXIT\n";
    cout << "====================================================\n";
}

int main() {
    WSADATA wsaData;
    SOCKET clientSocket;
    SOCKADDR_IN serverAddr;
    vector<char> buffer(1024);

    WSAStartup(MAKEWORD(2, 2), &wsaData);
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8888);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
    connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));

    cout << "Connected to server." << endl;

    while (true) {
        printMenu();
        cout << "Select: ";
        string choice;
        getline(cin, choice);

        if (choice == "1" && loginStatus != "[LOGIN]") {
            string id, pw;
            cout << "ID: "; getline(cin, id);
            cout << "PW: "; getline(cin, pw);
            string msg = "REGISTER:" + id + ":" + pw + ":";
            sendLine(clientSocket, msg);

        }
        else if (choice == "2" && loginStatus != "[LOGIN]") {
            string id, pw;
            cout << "ID: "; getline(cin, id);
            cout << "PW: "; getline(cin, pw);
            string msg = "LOGIN:" + id + ":" + pw + ":";
            sendLine(clientSocket, msg);

            string response = receiveLine(clientSocket);
            cout << "[SERVER] " << response;
            if (response.find("Login Success") != string::npos) {
                loginStatus = "[LOGIN]";
                loginUser = id;
            }
            continue;

        }
        else if (choice == "3" && loginStatus == "[LOGIN]") {
            while (true) {
                cout << "Message (exit to stop): ";
                string chat;
                getline(cin, chat);
                if (chat == "exit") break;

                string msg = "CHAT:" + chat;
                sendLine(clientSocket, msg);

                int len = recv(clientSocket, buffer.data(), buffer.size(), 0);
                if (len > 0) {
                    string resp(buffer.data(), len);
                    cout << "[SERVER] " << resp;
                }
            }
            continue;

        }
        else if (choice == "4" && loginStatus == "[LOGIN]") {
            sendLine(clientSocket, "exit");
            loginStatus = "[LOGOUT]";
            cout << "[" << loginUser << "], LOGOUT\n";
            continue;

        }
        else if (choice == "0") {
            cout << "GOODBYE, CHATTALK\n";
            sendLine(clientSocket, "exit");
            break;

        }
        else {
            cout << "Invalid choice or unavailable in current state.\n";
            continue;
        }

        // 일반 응답 출력
        int recvLen = recv(clientSocket, buffer.data(), buffer.size(), 0);
        if (recvLen > 0) {
            string response(buffer.data(), recvLen);
            cout << "[SERVER] " << response;
        }
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}