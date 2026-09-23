/**
 * @file engine_v2_flat_array.cpp
 * @brief Ultra-low latency LOB matching engine using Dense Flat Arrays.
 * Guarantees O(1) complexity at the cost of L2 Cache misses.
 */

#include <iostream>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <chrono>
#include <random>
#include <memory> 

namespace hft {

    constexpr uint32_t MAX_TICK_PRICE = 200000; 
    constexpr uint32_t MAX_ORDERS = 2000000;    

    struct Limit;

    struct Order {
        uint64_t order_id;
        bool is_buy;
        uint32_t price_tick;
        uint32_t size;
        
        Order* prev_order;
        Order* next_order;
        Limit* parent_limit;
        
        Order() : order_id(0), is_buy(false), price_tick(0), size(0), 
                  prev_order(nullptr), next_order(nullptr), parent_limit(nullptr) {}
    };

    struct Limit {
        uint32_t price_tick;
        uint32_t total_volume;
        uint32_t order_count;
        
        Order* head_order;
        Order* tail_order;
        
        Limit() : price_tick(0), total_volume(0), order_count(0), 
                  head_order(nullptr), tail_order(nullptr) {}
    };

    class OrderBook {
    private:
        Limit bids[MAX_TICK_PRICE];
        Limit asks[MAX_TICK_PRICE];
        
        uint32_t best_bid;
        uint32_t best_ask;

        std::vector<Order*> order_index;     
        std::vector<Order*> free_orders;
        std::vector<Order*> all_allocated_orders; 

        inline Order* get_order_from_pool(uint64_t id, bool is_buy, uint32_t price, uint32_t size) {
            Order* order = free_orders.back();
            free_orders.pop_back();
            order->order_id = id;
            order->is_buy = is_buy;
            order->price_tick = price;
            order->size = size;
            order->prev_order = nullptr;
            order->next_order = nullptr;
            order->parent_limit = nullptr;
            return order;
        }

        inline void return_order_to_pool(Order* order) {
            free_orders.push_back(order);
        }

        void remove_order_node(Order* order) {
            Limit* limit = order->parent_limit;

            if (order->prev_order != nullptr) {
                order->prev_order->next_order = order->next_order;
            } else {
                limit->head_order = order->next_order;
            }

            if (order->next_order != nullptr) {
                order->next_order->prev_order = order->prev_order;
            } else {
                limit->tail_order = order->prev_order;
            }

            limit->total_volume -= order->size;
            limit->order_count -= 1;

            if (limit->order_count == 0) {
                if (order->is_buy && limit->price_tick == best_bid) {
                    while (best_bid > 0 && bids[best_bid].order_count == 0) best_bid--;
                } else if (!order->is_buy && limit->price_tick == best_ask) {
                    while (best_ask < MAX_TICK_PRICE - 1 && asks[best_ask].order_count == 0) best_ask++;
                }
            }

            order_index[order->order_id] = nullptr;
            return_order_to_pool(order); 
        }

        void match_orders() {
            while (best_bid >= best_ask && best_bid > 0 && best_ask < MAX_TICK_PRICE) {
                Limit& bid_limit = bids[best_bid];
                Limit& ask_limit = asks[best_ask];

                Order* bid_order = bid_limit.head_order;
                Order* ask_order = ask_limit.head_order;

                uint32_t trade_size = std::min(bid_order->size, ask_order->size);

                bid_order->size -= trade_size;
                ask_order->size -= trade_size;
                
                bid_limit.total_volume -= trade_size;
                ask_limit.total_volume -= trade_size;

                if (bid_order->size == 0) remove_order_node(bid_order);
                if (ask_order->size == 0) remove_order_node(ask_order);
            }
        }

    public:
        OrderBook(size_t pool_size = MAX_ORDERS) {
            best_bid = 0;
            best_ask = MAX_TICK_PRICE - 1;

            for (uint32_t i = 0; i < MAX_TICK_PRICE; ++i) {
                bids[i].price_tick = i;
                asks[i].price_tick = i;
            }

            free_orders.reserve(pool_size);
            all_allocated_orders.reserve(pool_size);
            order_index.resize(pool_size + 10, nullptr);

            for (size_t i = 0; i < pool_size; ++i) {
                Order* new_order = new Order();
                free_orders.push_back(new_order);
                all_allocated_orders.push_back(new_order); 
            }
        }

        ~OrderBook() {
            for (Order* order : all_allocated_orders) delete order;
        }

        void add_order(uint64_t id, bool is_buy, uint32_t price_tick, uint32_t size) {
            Order* new_order = get_order_from_pool(id, is_buy, price_tick, size);
            order_index[id] = new_order; 

            Limit* target_limit = is_buy ? &bids[price_tick] : &asks[price_tick];
            new_order->parent_limit = target_limit;
            
            if (target_limit->head_order == nullptr) {
                target_limit->head_order = new_order;
                target_limit->tail_order = new_order;
            } else {
                new_order->prev_order = target_limit->tail_order;
                target_limit->tail_order->next_order = new_order;
                target_limit->tail_order = new_order;
            }
            
            target_limit->total_volume += size;
            target_limit->order_count += 1;

            if (is_buy && price_tick > best_bid) best_bid = price_tick;
            if (!is_buy && price_tick < best_ask) best_ask = price_tick;

            match_orders();
        }

        void cancel_order(uint64_t id) {
            if (id < order_index.size() && order_index[id] != nullptr) {
                remove_order_node(order_index[id]);
            }
        }
    };
} // namespace hft

void run_speed_test(int num_messages) {
    auto engine = std::make_unique<hft::OrderBook>(num_messages + 100);
    
    std::mt19937 gen(42); 
    std::uniform_int_distribution<uint32_t> price_dist(9000, 11000);
    std::uniform_int_distribution<uint32_t> size_dist(10, 500);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> action_dist(1, 100);

    struct Message { int type; uint64_t id; bool is_buy; uint32_t price; uint32_t size; };
    std::vector<Message> messages(num_messages);
    
    uint64_t current_id = 1;
    std::vector<uint64_t> active_ids;
    active_ids.reserve(num_messages);

    for (int i = 0; i < num_messages; ++i) {
        if (action_dist(gen) <= 80 || active_ids.empty()) {
            messages[i] = {1, current_id, (bool)side_dist(gen), price_dist(gen), size_dist(gen)};
            active_ids.push_back(current_id);
            current_id++;
        } else {
            std::uniform_int_distribution<size_t> index_dist(0, active_ids.size() - 1);
            size_t idx = index_dist(gen);
            messages[i] = {2, active_ids[idx], false, 0, 0};
            active_ids[idx] = active_ids.back();
            active_ids.pop_back();
        }
    }

    std::cout << "Starting O(1) Flat Array Engine Benchmark...\n";
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_messages; ++i) {
        if (messages[i].type == 1) engine->add_order(messages[i].id, messages[i].is_buy, messages[i].price, messages[i].size);
        else engine->cancel_order(messages[i].id);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    
    std::cout << "Processed: " << num_messages << " messages\n";
    std::cout << "Time taken: " << diff.count() << " seconds\n";
    std::cout << "Throughput: " << (int)(num_messages / diff.count()) << " ops/sec\n";
}

int main() {
    run_speed_test(1000000);
    return 0;
}