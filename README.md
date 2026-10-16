# 🗄️ Redis-lite — In-Memory Key-Value Store (C++20)

A **concurrent, in-memory key-value store** built from scratch in C++20, inspired by Redis.  
Supports `SET / GET / DEL / EXPIRE`, per-shard LRU eviction, TTL expiry, and 16-way sharded locking.

**Benchmarked at `2.45M ops/sec` — 6.3× throughput and 11× lower P99 vs a single global lock.**

---

## 📐 Architecture Overview

```
Client (nc / redis-cli)
        │  TCP (port 6379)
        ▼
┌─────────────────────────────────────────────────┐
│                    Server                        │
│  accept() ──► thread-per-client                  │
│                    │                             │
│               parse(input)                       │
│                    │                             │
│         ┌──────────▼──────────┐                 │
│         │       KVStore        │                 │
│         │  hash(key) % 16     │                 │
│         │  ┌─────┐ ┌─────┐    │                 │
│         │  │Shard│ │Shard│... │                 │
│         │  │ map │ │ map │    │                 │
│         │  │list │ │list │    │                 │
│         │  │mutex│ │mutex│    │                 │
│         │  └─────┘ └─────┘    │                 │
│         └─────────────────────┘                 │
│              Background TTL cleaner thread       │
└─────────────────────────────────────────────────┘
```

---

## 🚀 Features

| Phase | Feature | Key Technique |
|-------|---------|---------------|
| 1 | TCP Server | POSIX sockets — `socket / bind / listen / accept / recv / send` |
| 2 | Command Parsing | Tokenizer → typed `Command` struct with validation |
| 3 | Storage | `unordered_map` — O(1) average insert / lookup / delete |
| 4 | TTL Expiry | `chrono` deadlines + lazy expiry on read + active background thread |
| 5 | LRU Eviction | `map` + doubly-linked `list` + cached iterator — O(1) touch & evict |
| 6 | Concurrency | Thread-per-client + 16-shard mutex — 6.3× throughput gain |

---

## 📁 Project Structure

```
kvstore/
├── CMakeLists.txt
├── include/
│   ├── command.h       # Command struct + CommandType enum
│   ├── parser.h        # parse() declaration
│   ├── store.h         # KVStore class (16-shard LRU + TTL)
│   └── server.h        # Server class
└── src/
    ├── main.cpp        # entry point (6 lines)
    ├── server.cpp      # TCP socket loop + thread dispatch
    ├── parser.cpp      # tokenize + validate
    └── store.cpp       # set / get / del / expire + LRU + TTL
```

---

## ⚙️ Build & Run

**Requirements:** `g++ ≥ 10`, Linux / WSL (POSIX sockets)

```bash
# Build
g++ -std=c++20 -Iinclude -pthread \
    src/main.cpp src/server.cpp src/parser.cpp src/store.cpp \
    -o server

# Run server (Terminal 1)
./server
# Server ready on port 6379...

# Connect client (Terminal 2)
nc localhost 6379
```

---

## 💬 Supported Commands

```
SET name shivam        → OK
GET name               → shivam
DEL name               → OK
GET name               → (nil)
EXPIRE name 5          → OK   (key auto-deletes after 5 seconds)
```

---

## 🔑 Core Design Decisions

### 1. TTL — Lazy + Active Expiry

Two-pronged expiry, same as real Redis:

- **Lazy:** on every `GET`, check if the key's deadline has passed → delete on the spot. Zero extra overhead for permanent keys (short-circuit on `hasExpiry = false`).
- **Active:** background thread sleeps 1 second, then sweeps all shards and erases dead keys — memory never leaks even for keys nobody reads.

```cpp
// Lazy (inside get())
if (e.hasExpiry && steady_clock::now() > e.expireAt) {
    data_.erase(it);
    return false;   // (nil)
}

// Active (background cleanupLoop)
for (auto it = data_.begin(); it != data_.end(); ) {
    if (expired(it->second))  it = data_.erase(it);
    else                      ++it;
}
```

> `steady_clock` used (not `system_clock`) — immune to wall-clock jumps.

### 2. LRU — O(1) via Cached Iterator

Every map `Entry` stores a `list<string>::iterator` — a direct pointer to its node in the recency list.

```
MAP data_                       LIST usageList_
"a" → { value, listIt ●─────►  [a]  }
"b" → { value, listIt ●─────►  [b]  }
                                front = recent, back = oldest
```

- `touch(key)`: `erase(listIt)` → `push_front` → update `listIt = begin()` — all O(1)
- `evict()`: `back()` → `pop_back` → `map.erase` — O(1)

No scan. No O(n) search. The cached iterator is the trick.

### 3. 16-Shard Locking

```cpp
size_t getShardIndex(const string& key) {
    return std::hash<string>{}(key) % 16;
}
```

Each shard has its own `unordered_map`, `list`, and `mutex`. Unrelated keys hit different shards and run **in parallel**. Same key always maps to the same shard — consistency guaranteed.

| | 1 shard (global lock) | 16 shards |
|---|---|---|
| Throughput | 388,349 ops/sec | **2,453,987 ops/sec** |
| P50 latency | 2,462 ns | **827 ns** |
| P99 latency | 153,237 ns | **13,605 ns** |

Benchmark: 8 threads × 100K ops, 50% SET / 50% GET, in-process (no network overhead).

---

## 🧵 Concurrency Model

```
Main thread                 Cleaner thread
──────────────              ──────────────
accept() loop               while (running_) {
  │                           sleep_for(1s)
  └─► thread(handleClient)    lock shard
        recv → parse          sweep expired
        store.set/get         unlock
        send                }
        close(fd)
```

- `std::mutex` per shard — protects `map` + `list` (composite structure, can't be atomic)
- `std::atomic<bool> running_` — single flag for the cleaner loop (atomic is enough)
- `lock_guard` (RAII) — auto-unlock, no manual `unlock()` to forget

---

## 📊 Benchmark

```
Workload : 8 threads × 100,000 ops (50% SET / 50% GET)
Keys     : random strings, uniform distribution
Capacity : 100 keys (LRU eviction active)
```

```
[benchmark] 16 shards
Total ops   : 800,000
Total time  : 0.33 s
Throughput  : 2,453,987 ops/sec
P50 latency : 827 ns
P99 latency : 13,605 ns

[benchmark] 1 shard (baseline)
Throughput  : 388,349 ops/sec
P99 latency : 153,237 ns
```

---

## 🗺️ Roadmap

- [x] Phase 1 — TCP Server (POSIX sockets, echo)
- [x] Phase 2 — Command Parsing (SET / GET / DEL / EXPIRE)
- [x] Phase 3 — Storage (`unordered_map`)
- [x] Phase 4 — TTL (lazy + active expiry, `chrono`, `mutex`, `thread`, `atomic`)
- [x] Phase 5 — LRU Eviction (O(1) map + list + cached iterator)
- [x] Phase 6 — Concurrency (thread-per-client, 16-shard locking, benchmark)
- [ ] Persistence (append-only log / snapshot)
- [ ] RESP binary protocol
- [ ] `epoll`-based event loop (beyond thread-per-client)
- [ ] Lock-free structures for sub-µs tail latency
- [ ] Consistent hashing for multi-node scale-out

---

## 🛠️ Tech Stack

- **Language:** C++20
- **Networking:** POSIX sockets (`sys/socket.h`, `netinet/in.h`)
- **Concurrency:** `std::thread`, `std::mutex`, `std::atomic`, `std::lock_guard`
- **Time:** `std::chrono::steady_clock`
- **Data structures:** `std::unordered_map`, `std::list`
- **Build:** `g++ -std=c++20 -pthread` / CMake

---

## 👤 Author

**Shivam Kachhadiya**  
M.Tech CSE — VIT Vellore  
[shivamsys.in](https://shivamsys.in) · [github.com/shivamkachhadiya](https://github.com/shivamkachhadiya)
