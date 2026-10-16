//
// Created by shiva on 24-06-2026.
//
// parser.cpp — Phase 2 mein bharenge
#include "parser.h"
#include <sstream>
#include <vector>
// stringstream kya hai?
// ek string ko "stream" jaisa treat karta hai — jaise cin se input lete hain.
//    cin >> x     → keyboard se padho
//    ss >> x      → string se padho
//
// Default: WHITESPACE pe break karta (space, tab, newline)
//    → exactly humein yahi chahiye!
// Step by step:
// input = "SET name shivam"
// ss(input)              → stringstream banaya is string ka
//
// while (ss >> token)    → ek-ek word nikalo
//    iteration 1: token = "SET"     → tokens = ["SET"]
//    iteration 2: token = "name"    → tokens = ["SET", "name"]
//    iteration 3: token = "shivam"  → tokens = ["SET", "name", "shivam"]
//    iteration 4: kuch nahi bacha   → loop khatam
//
// Result: tokens = ["SET", "name", "shivam"]
// std::vector<std::string> kya hai?
// vector = dynamic array — size badh sakta jaise jaise push karte
// strings ka — string ka array
// push_back(x) = end mein x add karo
// Real-life:
// notebook — page badhate jao, jitna chahiye utna
//    array banane jaisa nahi — "fixed 100 size lo"
//    vector smart hai

Command parse(const std::string& input) {
    Command cmd;

    // Step 1: String ko tokens mein todo
    std::stringstream ss(input);
    std::string token;
    std::vector<std::string> tokens;

    while (ss >> token) {
        tokens.push_back(token);
    }

    // Step 2: Khaali input check
    if (tokens.empty()) {
        cmd.error = "empty command";
        return cmd;
    }

    // Step 3: Pehla token = command name
    const std::string& cmdName = tokens[0];

    // Step 4: SET handle karo
    if (cmdName == "SET") {
        if (tokens.size() != 3) {
            cmd.error = "SET needs key and value";
            return cmd;
        }
        cmd.type  = CommandType::SET;
        cmd.key   = tokens[1];
        cmd.value = tokens[2];
        cmd.valid = true;
        return cmd;
    }

    // Step 5: GET handle karo
    if (cmdName == "GET") {
        if (tokens.size() != 2) {
            cmd.error = "GET needs key";
            return cmd;
        }
        cmd.type  = CommandType::GET;
        cmd.key   = tokens[1];
        cmd.valid = true;
        return cmd;
    }

    // Step 6: DEL handle karo
    if (cmdName == "DEL") {
        if (tokens.size() != 2) {
            cmd.error = "DEL needs key";
            return cmd;
        }
        cmd.type  = CommandType::DEL;
        cmd.key   = tokens[1];
        cmd.valid = true;
        return cmd;
    }

    if (cmdName == "EXPIRE") {
        if (tokens.size() != 3) {
            cmd.error = "EXPIRE needs key and seconds";
            return cmd;
        }
        cmd.type = CommandType::EXPIRE;
        cmd.key  = tokens[1];
        cmd.seconds = std::stoi(tokens[2]);   // string "3" → int 3
        cmd.valid = true;
        return cmd;
    }

    // Step 7: Kuch bhi match nahi hua
    cmd.error = "unknown command: " + cmdName;
    return cmd;
}
