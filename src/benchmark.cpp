//
// Created by shiva on 28-06-2026.
//
#include "store.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <algorithm>
#include <atomic>
using namespace std;
using Clock = chrono::steady_clock;

int main() {
    const int NUM_THREADS = 8;          // kitne parallel clients
    const int OPS_PER_THREAD = 100000;  // har thread kitne ops
    const int TOTAL_OPS = NUM_THREADS * OPS_PER_THREAD;

    KVStore store(100000);              // bada capacity (eviction na ho beech mein)

    // har thread apne latencies yahan rakhega (nanoseconds)
    vector<vector<long long>> latencies(NUM_THREADS);

    // ek thread ka kaam
    auto worker = [&](int threadId) {
        vector<long long>& myLat = latencies[threadId];
        myLat.reserve(OPS_PER_THREAD);

        for (int i = 0; i < OPS_PER_THREAD; i++) {
            string key = "key" + to_string(threadId) + "_" + to_string(i % 1000);
            string value = "val" + to_string(i);

            auto opStart = Clock::now();

            // aadha SET, aadha GET
            if (i % 2 == 0) {
                store.set(key, value);
            } else {
                string out;
                store.get(key, out);
            }

            auto opEnd = Clock::now();
            long long ns = chrono::duration_cast<chrono::nanoseconds>(opEnd - opStart).count();
            myLat.push_back(ns);
        }
    };

    // ===== BENCHMARK START =====
    auto start = Clock::now();

    vector<thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(worker, t);     // thread banao + chalao
    }
    for (auto& th : threads) {
        th.join();                            // sab khatam hone do
    }

    auto end = Clock::now();
    // ===== BENCHMARK END =====

    // total time
    double totalSec = chrono::duration_cast<chrono::milliseconds>(end - start).count() / 1000.0;

    // throughput
    double opsPerSec = TOTAL_OPS / totalSec;

    // saare latencies ek vector mein daalo (P50/P99 ke liye)
    vector<long long> allLat;
    allLat.reserve(TOTAL_OPS);
    for (auto& v : latencies)
        for (auto ns : v)
            allLat.push_back(ns);

    sort(allLat.begin(), allLat.end());

    long long p50 = allLat[allLat.size() * 50 / 100];
    long long p99 = allLat[allLat.size() * 99 / 100];

    // ===== REPORT =====
    cout << "========== BENCHMARK ==========\n";
    cout << "Threads        : " << NUM_THREADS << "\n";
    cout << "Ops per thread : " << OPS_PER_THREAD << "\n";
    cout << "Total ops      : " << TOTAL_OPS << "\n";
    cout << "Total time     : " << totalSec << " sec\n";
    cout << "Throughput     : " << (long long)opsPerSec << " ops/sec\n";
    cout << "Latency P50    : " << p50 << " ns\n";
    cout << "Latency P99    : " << p99 << " ns\n";
    cout << "===============================\n";

    return 0;
}

//g++ -std=c++20 -Iinclude -pthread src/benchmark.cpp src/store.cpp -o benchmark