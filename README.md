# Nasdaq-TotalView-ITCH-Parser
### TLDR;
Calculates the hourly VWAP (Volume Weighted Average Price) for each security.
* Inputs:
    * Path to file holding binary TotalViewITCH data and a path to a directory
      where files holding the hourly VWAP will be stored.
    * Ex: `/Users/tilak/Downloads/01302019.NASDAQ_ITCH50`
* Outputs
    * Files representing the VWAP for each hour where a security was traded
    * Example: `hour_8.txt`:
        ```
        SCO     : 20.8500
        UGLD    : 100.9900
        AAPL    : 161.8413
        ALGN    : 207.5610
        EEM     : 42.1460
        LABD    : 28.4055
        SVXY    : 46.4517
        ...
        ```
* Example invocation: `./a /Users/tilak/Downloads/01302019.NASDAQ_ITCH50 /Users/tilak/out/`

Read more about TotalView-ITCH here: https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf

## How VWAP is calculated
3 data structures are used:
1. `m_vwap`: an array which maps the stock ID to an `Equation` struct holding
    the "numerator" and "denominator" which will be used to calculated VWAP.
2. `m_symbols`: an array which maps the stock ID to the stock symbol
3. `m_order_map`: an unordered map which maps the order id to an order struct 
    which holds order price and quantity.

The algorithm only keeps track of Bid side orders. When a bid order is received,
it is added to the map. When an order is executed, the associated security's 
"numerator" in the `m_vwap` struct is updated with the (price * quantity of shares) 
and the "denominator" is updated with the number of shares traded.

Each message has a timestamp, which is used to keep track of each hour of trading.
At the time we cross the hour boundary, the algorithm goes through each entry of
`m_vwap` and `m_symbols` and calculates the VWAP and outputs it to a file which 
holds VWAP for each security traded during that hour.

## Compiling:
```
Apple clang version 14.0.3 (clang-1403.0.22.14.1)

c++ -std=c++20 -I ../boost_1_82_0 -I . parser.cpp  main.cpp   -o a ~/code/boost_1_82_0/stage/lib/libboost_iostreams.a
```


