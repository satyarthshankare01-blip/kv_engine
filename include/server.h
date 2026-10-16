//
// Created by shiva on 24-06-2026.
//

#ifndef IN_MEMORY_KEY_VALUE_STORE__REDIS_LITE__SERVER_H
#define IN_MEMORY_KEY_VALUE_STORE__REDIS_LITE__SERVER_H

#endif //IN_MEMORY_KEY_VALUE_STORE__REDIS_LITE__SERVER_H
#pragma once
#include <string>

class KVStore;   // forward declaration

class Server {
public:
    Server(int port);
    void run();

private:
    void handleClient(int client_fd, KVStore& store);   // ← naya

    int port_;
    int server_fd_;
};

// handleClient → ek client ko handle karega (recv/parse/store/send loop)
// KVStore& store → store ka REFERENCE (sab clients SAME store use karein!)
//                  → copy nahi (& zaroori)
//
// class KVStore; → forward declaration
//    "KVStore naam ka class hai, detail store.h mein"
//    → header mein sirf reference use kar rahe, full include nahi chahiye