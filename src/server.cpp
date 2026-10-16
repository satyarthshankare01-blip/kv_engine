#include "server.h"
#include "parser.h"
#include "store.h"
#include <iostream>
#include <cstring>
#include <thread>           // ← naya
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

Server::Server(int port) : port_(port), server_fd_(-1) {}

// ek client ko handle karta — ye pehle run() ke andar tha
void Server::handleClient(int client_fd, KVStore& store) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes <= 0) {
            std::cout << "Client disconnected.\n";
            break;
        }

        std::string input(buffer, bytes);
        while (!input.empty() && (input.back() == '\n' || input.back() == '\r')) {
            input.pop_back();
        }

        Command cmd = parse(input);

        std::string response;
        if (!cmd.valid) {
            response = "ERROR: " + cmd.error + "\n";
        } else {
            switch (cmd.type) {
                case CommandType::SET:
                    store.set(cmd.key, cmd.value);
                    response = "OK\n";
                    break;
                case CommandType::GET: {
                    std::string value;
                    response = store.get(cmd.key, value) ? value + "\n" : "(nil)\n";
                    break;
                }
                case CommandType::DEL:
                    response = store.del(cmd.key) ? "OK\n" : "(nil)\n";
                    break;
                case CommandType::EXPIRE:
                    response = store.expire(cmd.key, cmd.seconds) ? "OK\n" : "(nil)\n";
                    break;
                default:
                    response = "ERROR: unhandled\n";
            }
        }

        send(client_fd, response.c_str(), response.size(), 0);
    }
    close(client_fd);      // is client ki line band
}

void Server::run() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ == -1) { std::cerr << "socket fail\n"; return; }

    sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(server_fd_, (sockaddr*)&address, sizeof(address)) == -1) {
        std::cerr << "bind fail — port busy?\n"; return;
    }
    if (listen(server_fd_, 5) == -1) { std::cerr << "listen fail\n"; return; }

    std::cout << "Server ready on port " << port_ << "...\n";

    KVStore store(100);     // ONE store, saare clients ke liye shared

    while (true) {                                          // ACCEPT LOOP
        int client_fd = accept(server_fd_, nullptr, nullptr);
        if (client_fd == -1) { std::cerr << "accept fail\n"; continue; }

        std::cout << "Client connected!\n";

        // har client ka apna thread
        std::thread(&Server::handleClient, this, client_fd, std::ref(store)).detach();
    }

    close(server_fd_);
}