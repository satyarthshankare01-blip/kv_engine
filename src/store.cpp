#include "store.h"
#include <iostream>
using namespace std;
KVStore::KVStore(size_t capacity) : capacity_(capacity), running_(true) {
    cleaner_ = thread(&KVStore::cleanupLoop, this);
}

KVStore::~KVStore() {
    running_ = false;
    if (cleaner_.joinable()) cleaner_.join();
}

/* ===================================================================
   PURANA CODE (single map + single mutex) — Phase 5 tak
   Har function poore data_ pe EK mutex_ lagata tha.
   Problem: alag keys pe kaam karne wale clients bhi ek taale pe wait.
   Sharding ne ise replace kiya (neeche).

void KVStore::touch(const string& key) {
    auto it = data_.find(key);
    usageList_.erase(it->second.listIt);
    usageList_.push_front(key);
    it->second.listIt = usageList_.begin();
}
void KVStore::evictIfNeeded() {
    if (data_.size() <= capacity_) return;
    string oldest = usageList_.back();
    usageList_.pop_back();
    data_.erase(oldest);
}
void KVStore::set(const string& key, const string& value) {
    lock_guard<mutex> lock(mutex_);
    ... data_ / usageList_ ...
}
   (get / del / expire / cleanupLoop bhi same — ek mutex_, ek data_)
   =================================================================== */


// ===== NAYA (SHARDED) =====

// key -> shard number. hash(key) % 16. Same key -> hamesha same shard.
size_t KVStore::getShardIndex(const std::string& key) {
    size_t idx = std::hash<std::string>{}(key) % SHARD_COUNT;
    //std::cout << "[shard] key '" << key << "' -> shard " << idx << "\n";  // temp
    return idx;
}

// touch, but on a GIVEN shard's list (not a global list anymore)
void KVStore::touchInShard(Shard& shard, const string& key) {
    auto it = shard.data.find(key);
    shard.usageList.erase(it->second.listIt);   // O(1) remove via iterator
    shard.usageList.push_front(key);            // re-insert at front
    it->second.listIt = shard.usageList.begin();// save new position
}

// evict, but on a GIVEN shard (each shard has its own capacity)
void KVStore::evictIfNeededInShard(Shard& shard) {
    if (shard.data.size() <= capacity_) return;
    string oldest = shard.usageList.back();   // back = LRU of this shard
    shard.usageList.pop_back();
    shard.data.erase(oldest);
    cout << "[lru] evicted: " << oldest << "\n";
}

// SET: pick shard, lock ONLY that shard, then same logic as before
void KVStore::set(const string& key, const string& value) {
    Shard& shard = shards_[getShardIndex(key)];        // 1. which shard
    lock_guard<mutex> lock(shard.mutex);     // 2. lock only this shard

    auto it = shard.data.find(key);
    if (it != shard.data.end()) {                      // existing key -> update
        it->second.value = value;
        it->second.hasExpiry = false;
        touchInShard(shard, key);
        return;
    }
    shard.usageList.push_front(key);                   // new key -> list front
    Entry e;
    e.value = value;
    e.hasExpiry = false;
    e.listIt = shard.usageList.begin();
    shard.data[key] = e;
    evictIfNeededInShard(shard);                       // evict if this shard full
}

// GET: pick shard, lock it, lazy-expiry check, return value
bool KVStore::get(const string& key, string& valueOut) {
    Shard& shard = shards_[getShardIndex(key)];
    lock_guard<mutex> lock(shard.mutex);

    auto it = shard.data.find(key);
    if (it == shard.data.end()) return false;

    Entry& e = it->second;
    if (e.hasExpiry && chrono::steady_clock::now() > e.expireAt) {
        shard.usageList.erase(e.listIt);               // dead -> remove from BOTH
        shard.data.erase(it);
        return false;
    }
    valueOut = e.value;
    touchInShard(shard, key);                          // read counts as "used"
    return true;
}

// DEL: pick shard, lock it, erase from both map + list
bool KVStore::del(const string& key) {
    Shard& shard = shards_[getShardIndex(key)];
    lock_guard<mutex> lock(shard.mutex);

    auto it = shard.data.find(key);
    if (it == shard.data.end()) return false;

    shard.usageList.erase(it->second.listIt);
    shard.data.erase(it);
    return true;
}

// EXPIRE: pick shard, lock it, set deadline on the key
bool KVStore::expire(const string& key, int seconds) {
    Shard& shard = shards_[getShardIndex(key)];
    lock_guard<mutex> lock(shard.mutex);

    auto it = shard.data.find(key);
    if (it == shard.data.end()) return false;

    it->second.expireAt = chrono::steady_clock::now()
                          + chrono::seconds(seconds);
    it->second.hasExpiry = true;
    return true;
}

// ACTIVE expiry: scan EVERY shard once a second, lock each shard in turn
void KVStore::cleanupLoop() {
    while (running_) {
        this_thread::sleep_for(chrono::seconds(1));
        auto now = chrono::steady_clock::now();

        for (int i = 0; i < SHARD_COUNT; i++) {            // each of 16 shards
            Shard& shard = shards_[i];
            lock_guard<mutex> lock(shard.mutex); // lock THIS shard only

            for (auto it = shard.data.begin(); it != shard.data.end(); ) {
                if (it->second.hasExpiry && now > it->second.expireAt) {
                    cout << "[cleaner] expired key removed: " << it->first << "\n";
                    shard.usageList.erase(it->second.listIt);
                    it = shard.data.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
}