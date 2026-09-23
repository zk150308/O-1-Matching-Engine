# High-Frequency Trading Limit Order Book (C++)

A low-latency, zero-allocation Limit Order Book (LOB) matching engine written in C++17. This project explores the architectural tradeoffs between theoretical algorithmic complexity and hardware-level cache efficiency in quantitative finance systems.

## The Engineering Tradeoff: Complexity vs. Cache Efficiency

This repository contains two iterations of the matching engine, demonstrating a classic hardware tradeoff:

**Version 1: Red-Black Tree Indexing (`engine_v1_hash.cpp`)**
* **Throughput:** ~18.3 million ops/sec (Apple Silicon M3).
* **Architecture:** Utilizes a custom pre-allocated object pool (`std::vector<Order*>`) and direct array indexing for order IDs. 
* **The Catch:** It relies on `std::map` (a Red-Black Tree) for price levels, meaning price resolution is theoretically $O(\log N)$. However, because the memory footprint is extremely small, it fits entirely within the CPU's ultra-fast L1/L2 cache, resulting in massive real-world throughput.

**Version 2: Dense Flat Array (`engine_v2_flat_array.cpp`)**
* **Throughput:** ~8.4 million ops/sec.
* **Architecture:** Eliminates the $O(\log N)$ tree traversal entirely by replacing `std::map` with a statically sized Flat Array for 200,000 integer price ticks. Every single operation (add, cancel, match) is strictly $O(1)$.
* **The Catch:** Guaranteeing $O(1)$ complexity increased the engine's memory footprint to ~12.8MB. This exceeds the L2 cache size of the processor, resulting in frequent cache misses as the CPU fetches data from slower main RAM. 

## Memory Management & Safety

To achieve deterministic latency, this engine strictly avoids standard library containers that rely on dynamic memory allocation during runtime:
* **Zero Runtime Allocation:** Bypasses heap fragmentation and `new`/`delete` overhead by pre-allocating an object pool for 2,000,000 simultaneous orders at startup.
* **Heap vs. Stack Mitigation:** The $O(1)$ flat array architecture initially caused a macOS stack-overflow segmentation fault due to the OS's 8MB stack limit. This was mitigated by migrating the engine initialization to the heap using C++ smart pointers (`std::make_unique`).
* **Floating-Point Elimination:** Replaced floating-point math with integer tick mapping to prevent precision loss and further optimize hardware execution.

## Compilation & Execution 

Compiled with maximum speed optimizations for Apple Silicon:
```bash
# Compile Version 1 or Version 2
g++ -O3 -std=c++17 engine_v2_flat_array.cpp -o engine
./engine
```

## About
Currently a first-year Computer Science with Artificial Intelligence (G704) student at the University of Leeds. Actively exploring low-level systems programming, data structures, and deterministic low-latency software architecture.
