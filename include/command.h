//
// Created by shiva on 24-06-2026.
//

#ifndef IN_MEMORY_KEY_VALUE_STORE__REDIS_LITE__COMMAND_H
#define IN_MEMORY_KEY_VALUE_STORE__REDIS_LITE__COMMAND_H

#endif //IN_MEMORY_KEY_VALUE_STORE__REDIS_LITE__COMMAND_H
#pragma once

#include <string>

enum class CommandType {
    SET,
    GET,
    DEL,
    EXPIRE,      // ← add
    UNKNOWN     // galat command ke liye
};

struct Command {
    CommandType type = CommandType::UNKNOWN;
    std::string key;
    std::string value;
    int seconds = 0;        // ← add (EXPIRE ke liye)
    bool valid = false;
    std::string error;
};


// Client bheja:  "SET name shivam"
//
// Command struct:
//    ┌─────────────────────────┐
//    │ type   : SET                         │
//    │ key    : "name"                     │
//    │ value  : "shivam"                  │
//    │ valid  : true                         │
//    │ error  : ""                            │
//    └─────────────────────────┘