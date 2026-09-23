# High-Frequency Trading Limit Order Book (C++)

A low-latency, zero-allocation Limit Order Book (LOB) matching engine written in C++17. Designed to simulate the core infrastructure used in quantitative finance and proprietary trading.

## Performance Benchmark
On an Apple Silicon M2 Pro architecture, the engine processes **18.3+ million operations per second** (combining order insertions, cross-spread matching, and cancellations). 

## Architecture & Optimizations
To achieve maximum throughput and deterministic latency, this engine avoids standard library containers that rely on dynamic memory allocation or tree traversals during runtime:
* **Zero Dynamic Allocation:** Implemented a custom pre-allocated object pool (`std::vector<Order*>`) initialized at startup to eliminate heap fragmentation and `new`/`delete` overhead.
* **O(1) Order Lookups:** Replaced `std::unordered_map` with Direct Array Indexing, mapping order IDs directly to memory addresses for absolute zero-latency cancellations.
* **O(1) Price Level Resolution:** Replaced Red-Black Trees (`std::map`) with dense Flat Arrays to eliminate $O(\log N)$ tree traversal latency.
* **Integer Price Ticks:** Replaced floating-point math with integer tick mapping to optimize cache efficiency and prevent precision loss.

## Execution 
Compiled with maximum speed optimizations:
```bash
g++ -O3 -std=c++17 main.cpp -o engine
./engine
