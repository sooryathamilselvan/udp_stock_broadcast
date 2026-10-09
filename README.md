# UDP-Based Real-Time Stock Price Broadcasting System

## 1. Project Overview

This project implements a simulated real-time stock price distribution system using C++ and UDP sockets on Windows.

A central server generates Last Traded Price (LTP) values for five stocks and transmits all five prices together in a single UDP multicast datagram every second. Multiple clients join the multicast group and independently receive and display the stock prices.

The system demonstrates network communication, socket programming, data serialization, and one-to-many data distribution.

**Note:** The stock prices are simulated for demonstration purposes and do not represent actual market prices.

## 2. Objectives

- Implement UDP-based stock price transmission.
- Generate simulated LTP values for five stocks.
- Transmit all five stock prices in a single logical update.
- Support multiple clients receiving the same updates.
- Display timestamps and formatted stock prices.
- Handle basic socket errors and clean shutdown.
- Demonstrate UDP multicast communication.

## 3. Technologies Used

- Programming language: C++
- Networking: Winsock2
- Transport protocol: UDP
- Distribution method: IP multicast
- Message format: JSON
- Compiler: GCC (MSYS2 UCRT64)
- Operating system: Windows

## 4. System Architecture

The application consists of one server and multiple clients.

1. The server initializes five stock prices.
2. The server generates random price changes every second.
3. It creates a JSON message containing the timestamp and all five stock prices.
4. The server sends the message to multicast group `239.255.0.1` on UDP port `5000`.
5. Each client binds to the same local UDP port and joins the multicast group.
6. Clients receive the datagram and display the stock prices in a formatted table.

The server is the single source of truth for all generated prices. Clients only receive and display the transmitted values.

## 5. Stocks Simulated

| Stock Symbol | Initial LTP (INR) |
|---|---:|
| SBI | 812.40 |
| KOTAK | 1945.20 |
| 63MOONS | 684.50 |
| TATASTEEL | 156.80 |
| TATACOFFEE | 1124.60 |

The server changes each price randomly by approximately −₹2 to +₹2 per update and maintains positive prices.

## 6. Message Format

The server transmits one JSON message per update.

Example:

```json
{
  "timestamp": "16:10:03",
  "stocks": {
    "SBI": 811.90,
    "KOTAK": 1946.10,
    "63MOONS": 685.40,
    "TATASTEEL": 156.60,
    "TATACOFFEE": 1126.30
  }
}
```

The `timestamp` field records the server's local time. The `stocks` object contains all five stock symbols and their LTP values.

Keeping all five prices in one datagram allows clients to process them as one logical market-data update.

## 7. UDP Multicast Explanation

UDP is a connectionless transport protocol that transmits independent datagrams without establishing a TCP connection.

This application uses IP multicast to distribute each stock update to multiple subscribed clients.

The multicast group address is `239.255.0.1`, and the destination UDP port is `5000`.

The server calls `sendto()` once per update. Clients join the multicast group using `IP_ADD_MEMBERSHIP` and listen for incoming datagrams using `recvfrom()`.

Multicast avoids sending a separate application-level message to each client. However, UDP does not guarantee delivery, ordering, or retransmission of lost packets.

## 8. Project Structure

```text
udp-stock-broadcast/
├── server.cpp
├── client.cpp
└── README.md
```

## 9. Compilation Instructions

Open an MSYS2 UCRT64 terminal and navigate to the project directory.

```bash
cd /c/projects/udp-stock-broadcast
```

Compile the server:

```bash
g++ server.cpp -o server.exe -std=c++17 -lws2_32
```

Compile the client:

```bash
g++ client.cpp -o client.exe -std=c++17 -lws2_32
```

## 10. Execution Instructions

### Start the server

In Terminal 1:

```bash
./server.exe
```

The server begins generating and transmitting stock prices every second.

### Start Client 1

In Terminal 2:

```bash
./client.exe
```

### Start Client 2

In Terminal 3:

```bash
./client.exe
```

### Start Client 3

In Terminal 4:

```bash
./client.exe
```

All three clients should display the stock updates received from the same server.

### Stop the applications

Press `Ctrl+C` in each terminal to stop the corresponding application. The programs release their socket resources during normal shutdown.

## 11. Error Handling

The implementation checks for common socket errors, including:

- Winsock initialization failure.
- Socket creation failure.
- Invalid multicast addresses.
- Socket binding failure.
- Failure to join or leave the multicast group.
- UDP send and receive failures.
- Missing or incomplete stock data in a received message.

The client uses a receive timeout to periodically check for a shutdown request.

## 12. Testing and Demonstration

The following tests should be demonstrated:

1. Start the server and verify that stock prices are generated every second.
2. Start three clients and verify that they receive updates.
3. Compare prices with matching timestamps across clients.
4. Verify that all five stocks are present in each complete update.
5. Stop an individual client and confirm that the other clients continue receiving updates.
6. Stop the server using `Ctrl+C` and verify that it releases its resources.

### Screenshots to include

- Screenshot 1: Server terminal displaying timestamped stock prices.
- Screenshot 2: Three client terminals receiving stock updates simultaneously.
- Screenshot 3: Successful server shutdown.
- Screenshot 4: Successful client shutdown, if required.

## 13. Limitations

- Stock prices are simulated and are not obtained from an exchange.
- UDP multicast delivery is not guaranteed.
- The current implementation uses source-code constants for the multicast address, port, and update interval.
- The client parser is designed for the JSON structure generated by this server rather than arbitrary JSON documents.
- Multicast reception on different computers may depend on network and firewall configuration.

## 14. Conclusion

The project demonstrates a UDP-based one-to-many stock price distribution system using C++ and Winsock2. A central server generates simulated LTP values and sends all five prices in a single JSON datagram. Multiple clients independently receive and display the updates.

The implementation illustrates connectionless communication, multicast networking, structured message transmission, and socket resource management.
