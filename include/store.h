#pragma once

#include <string>
#include <unordered_map>
#include <list>
#include <chrono>
#include <mutex>
#include <thread>
#include <functional>   // std::hash
#include <atomic>

static const int SHARD_COUNT = 16;   // number of shards

struct Entry {
    std::string value;
    std::chrono::steady_clock::time_point expireAt;
    bool hasExpiry = false;
    std::list<std::string>::iterator listIt;   // position in this shard's usageList
};

class KVStore {
public:
    KVStore(size_t capacity = 100);
    ~KVStore();

    void set(const std::string& key, const std::string& value);
    bool get(const std::string& key, std::string& valueOut);
    bool del(const std::string& key);
    bool expire(const std::string& key, int seconds);

private:
    void cleanupLoop();

    // ===== PURANA (single map + single mutex) — commented =====
    // void touch(const std::string& key);
    // void evictIfNeeded();
    // std::unordered_map<std::string, Entry> data_;
    // std::list<std::string> usageList_;
    // std::mutex mutex_;

    // ===== NAYA (sharded) =====
    struct Shard {
        std::unordered_map<std::string, Entry> data;   // this shard's map
        std::list<std::string> usageList;              // this shard's LRU list
        std::mutex mutex;                              // this shard's own lock
    };

    Shard shards_[SHARD_COUNT];                        // array of 16 shards

    size_t getShardIndex(const std::string& key);      // key -> shard number
    void touchInShard(Shard& shard, const std::string& key);        // per-shard touch
    void evictIfNeededInShard(Shard& shard);                        // per-shard evict

    size_t capacity_;            // per-shard capacity
    std::thread cleaner_;
    std::atomic<bool> running_;
};