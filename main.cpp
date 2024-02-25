#include <iostream>
#include <boost/iostreams/device/mapped_file.hpp>

#include "parser.h"

namespace io = boost::iostreams;
using header_t = uint16_t;

int main(int argc, char *argv[]) {

    if(argc != 3) {
        std::cerr << "Please provide 2 arguments: " << std::endl;
        std::cerr << "1) Provide path to NASDAQ data." << std::endl;
        std::cerr << "2) Provide path to directory where hourly VWAP data will be created" << std::endl;
        return 1;
    }
    // open binary file
    boost::iostreams::mapped_file_source file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Failed to open file!" << std::endl;
        return 1;
    }

    Parser p(argv[2]);

    // access file content through the memory-mapped region
    const char* s = file.data();      // will use as iterator
    const char* e = s + file.size();  // pointer to end of file

    // Process SoupBinTCP packets:
    // data appears to be using SoupBinTCP format:
    // http://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/soupbintcp.pdf
    // data is sent in packets
    // - 2 byte header: contains packet length
    // - X byte packet: contains the packet
    while(s < e) {
        header_t packet_length = native_order<header_t>(s); // read 2 byte header
        s += sizeof(header_t); // increment pointer to packet data
        
        // process the packet - discern message type
        switch(*s) {
            case 'R':
                p.stockDirectory(s);
            break;
            case 'A':
            case 'F': // intentional fall through
                p.add(s);
            break;
            case 'X': 
                p.cancel(s);
            break;
            case 'D':
                p.del(s);
            break;
            case 'U':
                p.replace(s);
            break;
            case 'E':
                // order executed without price
                p.execute<false>(s);
            break;
            case 'C':
                // order executed with price
                p.execute<true>(s);
            break;
            case 'P':
                p.nonCrossTrade(s);
            break;
            case 'Q':
                p.crossTrade(s);
            break;
            case 'B':
                p.brokenTrade(s);
            break;
        };
        s += packet_length; // move on to next packet
    }

    p.printSizeOfRemainingOrders();
    return 0;
}
