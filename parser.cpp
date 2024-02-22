#include <array>
#include <boost/endian/conversion.hpp>
#include <cstdint>  // for uint32_t
#include <cstring>  // for std::memcpy
#include <fstream>  // for file io
#include <iostream>
#include <string>   // for string manipulation
#include <unordered_map>

#include "parser.h"

// Handle the stock directory message
// p_start is a pointer to the start of the message
void Parser::stockDirectory(const char *const p_start) {
    updateTime(p_start+5);
    uint16_t stock_locate = getStockLocate(p_start);

    // populate stock symbol
    std::memcpy(&m_symbols[stock_locate], p_start+11, 8);
}

// Handle adding orders
// NOTE: only considering buy orders. Sell orders are matched w/ buy orders,
// thus, to keep track of VWAP, only need to track one side to prevent double counting
void Parser::add(const char *const p) {
    updateTime(p+5);
    
    // only add buy orders to our tracking:
    if(p[19] != 'B') {
        return;
    }
    order_id_t order_id = getOrderId(p);
    uint32_t quantity = native_order<uint32_t>(p+20);
    uint32_t price = native_order<uint32_t>(p+32);

    m_order_map.emplace(order_id, Order(price, quantity)); 
}

void Parser::cancel(const char *const p) {
    updateTime(p+5);
    order_id_t order_id = getOrderId(p);
    uint32_t quantity_cancelled = native_order<uint32_t>(p+19);

    // update order quantity only if this order exists in our books
    auto pair = m_order_map.find(order_id);
    if(pair == m_order_map.end()) {
        // order is not found in our books, thus ignore
        return;
    }
    pair->second.quantity -= quantity_cancelled;
}

void Parser::del(const char *const p) {
    updateTime(p+5);
    order_id_t order_id = getOrderId(p);
    
    // if the order w order ID exists in our books, erase it
    auto it = m_order_map.find(order_id);
    if (it != m_order_map.end()) {
        m_order_map.erase(it);
    }
}

void Parser::replace(const char *const p) {
    del(p);

    // Don't bother swapping endianness - order ID will be unique regardless
    order_id_t new_order_id = *reinterpret_cast<const order_id_t*>(p+19);
    
    uint32_t quantity = native_order<uint32_t>(p+27);
    uint32_t price = native_order<uint32_t>(p+31);
    m_order_map.emplace(new_order_id, Order(price, quantity)); 
}


// returns the stock locate identifier
// p_msg_begin - pointer to the beginning of the message
// note: stock locate is located at an offset of 1 and is a 2 byte unsigned integer
uint16_t Parser::getStockLocate(const char* const p_msg_begin) {
    // note: we don't bother to reverse endianness here since each stock 
    // locate ID will be unique regardless of endianness
    return *reinterpret_cast<const uint16_t*>(p_msg_begin+1);
}

// returns the order identifier
// p_msg_begin - pointer to the beginning of the message
// note: order ID is located at an offset of 1 and is a 2 byte unsigned integer
order_id_t Parser::getOrderId(const char* const p_msg_begin) {
    // note: we don't bother to reverse endianness here since each order 
    // ID will be unique regardless of endianness
    return *reinterpret_cast<const order_id_t*>(p_msg_begin + 11);
}

// returns the ns since midnight
// p_ts - pointer to the area in memory which holds the timestamp
uint64_t Parser::getTimeStamp(const char* const p_ts) {
    uint64_t ns;
    std::memcpy(&ns, p_ts, 6);
    ns = ns << 16;
    return boost::endian::big_to_native(ns); // must reverse endianess
}

// called at the start of each message to process
// calculates VWAP at the end of each hour
// p_ts - pointer to the part of the message which holds the timestamp
void Parser::updateTime(const char *const p_ts) {
    uint64_t next_ns = getTimeStamp(p_ts); // timestamp from current message being processed
    uint32_t next_hour = (next_ns / kNsInHour); // the hour when the message being processed was sent

    // if the message being processed was received in the hour that is not 
    // the same as our current hour, process VWAP for each hour until we reach 
    // the same hour as the message being processed
    for(; m_hour < next_hour; m_hour++) {
        // process all vwaps and store/display
        processVwap();
    }
    m_ns = next_ns; // update current timestamp
}

// processes the VWAP for hour # m_hour
// write VWAP to a file labelled hour_<m_hour>.txt
// only writes VWAP for securities which traded in that hour
// if no securities traded in the hour, the file will be empty
void Parser::processVwap() {
    // open file for writing
    std::ofstream file(std::string(m_output_dir) + "hour_" + std::to_string(m_hour) + ".txt");
    if (!file.is_open()) {
        std::cerr << "failed to open the file." << std::endl;
        throw;
    }
    file << std::fixed << std::setprecision(4);

    char security_symbol[9];
    security_symbol[8] = '\0'; // add null terminator
    // iterate through all possible locate codes
    for(uint16_t stock_locate = 0; stock_locate < UINT16_MAX; stock_locate++) {
        if(m_vwap[stock_locate].denominator != 0) {
            double vwap = double(m_vwap[stock_locate].numerator) / double(m_vwap[stock_locate].denominator);
            vwap /= 10000;
            std::memcpy(&security_symbol[0], &m_symbols[stock_locate], 8);
            // write to file
            file << security_symbol << ": " << vwap << std::endl;
        }
        // reset vwap
        m_vwap[stock_locate].numerator = 0;
        m_vwap[stock_locate].denominator = 0;
    }

    file.close();
}

