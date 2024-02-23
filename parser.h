#pragma once

#include <array>
#include <boost/endian/conversion.hpp>
#include <cstdint>  // for uint32_t
#include <cstring>  // for std::memcppy
#include <iostream>
#include <string>   // for string manipulation
#include <unordered_map>

// Convert to little endian
template<typename T>
decltype(auto) native_order(const char * const ptr) {
    T network_order = *reinterpret_cast<const T*>(ptr);
    T native_order = boost::endian::big_to_native(network_order);
    return native_order;
}

using order_id_t = uint64_t; // order IDs are stored as 8 byte unsigned integers
class Parser {
public:
    // give the parser a directory of where to store output files
    Parser(char *output_dir) : m_output_dir(output_dir){}

    // structure to hold data used to calculate VWAP
    struct Equation{
        uint64_t numerator{0};   // sum of (# of executed shares * execution price)
        uint64_t denominator{0}; // holds sum of # of executed shares
    };

    struct Order{
        Order(uint32_t p, uint32_t q) : price(p), quantity(q){}
        uint32_t price;
        uint32_t quantity;
    };

    // Handle the stock directory message
    // p_start is a pointer to the start of the message
    void stockDirectory(const char *const p_start);

    // Handle adding orders
    // NOTE: only considering buy orders. Sell orders are matched w/ buy orders,
    // thus, to keep track of VWAP, only need to track one side to prevent double counting
    void add(const char *const p);

    void cancel(const char *const p);

    void del(const char *const p);

    void replace(const char *const p);


    // N - whether the message contains the price at which the order was filled
    // if not, the price is the same as the request price when the order was placed
    template<bool N>
    void execute(const char *const p) {
        updateTime(p+5);
        uint16_t stock_locate = getStockLocate(p);
        order_id_t order_id = getOrderId(p);

        // only proceed to process this message if it is in our books
        auto pair = m_order_map.find(order_id);
        if(pair == m_order_map.end()) {
            return;
        } 

        uint32_t quantity = native_order<uint32_t>(p+19);
        uint32_t price;
        
        if(N) {
            // get price from message
            price = native_order<uint32_t>(p+32);
        } else {
            // get price from ordiginal order
            price = pair->second.price;
        }

        // add to numerator and denominator to calculate VWAP
        m_vwap[stock_locate].numerator += (quantity * price);
        m_vwap[stock_locate].denominator += quantity;
        
        // reduce the number of outstanding shares which have not yet been executed
        pair->second.quantity -= quantity;
        if(pair->second.quantity == 0) {
            m_order_map.erase(pair);
        }
    }

private:
    // returns the stock locate identifier
    // p_msg_begin - pointer to the beginning of the message
    // note: stock locate is located at an offset of 1 and is a 2 byte unsigned integer
    uint16_t getStockLocate(const char* const p_msg_begin);

    // returns the order identifier
    // p_msg_begin - pointer to the beginning of the message
    // note: order ID is located at an offset of 1 and is a 2 byte unsigned integer
    order_id_t getOrderId(const char* const p_msg_begin);

    // returns the ns since midnight
    // p_ts - pointer to the area in memory which holds the timestamp
    uint64_t getTimeStamp(const char* const p_ts);

    // called at the start of each message to process
    // calculates VWAP at the end of each hour
    // p_ts - pointer to the part of the message which holds the timestamp
    void updateTime(const char *const p_ts);

    // processes the VWAP for hour # m_hour
    // write VWAP to a file labelled hour_<m_hour>.txt
    // only writes VWAP for securities which traded in that hour
    // if no securities traded in the hour, the file will be empty
    void processVwap();

    // I've chosen to use arrays here since it should help w/ cache locality
    // Also, being able to index arrays quickly with stock locate code should
    // provide quick lookup
    // Downside is that these arrays have 65,535 elements which may waste some
    // space
    std::array<Equation, UINT16_MAX> m_vwap{};
    std::array<char[8], UINT16_MAX> m_symbols{};

    // Not the best for cache locality
    std::unordered_map<order_id_t, Order> m_order_map;
    char *m_output_dir;
    uint64_t m_ns{0}; // keeps track of nanoseconds since midnight
    uint32_t m_hour{0}; // keeps track of current hour 0-23
    const uint64_t kNsInHour{3600000000000LLU}; // nanoseconds in an hour
};
