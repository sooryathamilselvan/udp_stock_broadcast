
#include <iostream>
#include <string>
#include <iomanip>
#include <sstream>
#include <vector>
#include <csignal>
#include <stdexcept>

#include <winsock2.h>
#include <ws2tcpip.h>

using namespace std;

volatile sig_atomic_t running = 1;

void handleInterrupt(int) {
    running = 0;
}

int main(int argc, char* argv[]) {
    string multicastIP = "239.255.0.1";
    int port = 5000;

    try {
        if (argc > 3) {
            cerr << "Usage: client.exe [multicast_ip] [port]\n";
            return 1;
        }

        if (argc > 1) multicastIP = argv[1];
        if (argc > 2) port = stoi(argv[2]);
    } catch (...) {
        cerr << "Invalid port argument.\n";
        return 1;
    }

    if (port < 1 || port > 65535) {
        cerr << "Port must be between 1 and 65535.\n";
        return 1;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Winsock initialization failed.\n";
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "Socket creation failed: " << WSAGetLastError() << '\n';
        WSACleanup();
        return 1;
    }

    BOOL reuse = TRUE;
    if (setsockopt(clientSocket, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&reuse),
                   sizeof(reuse)) == SOCKET_ERROR) {
        cerr << "Socket option failed: " << WSAGetLastError() << '\n';
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    DWORD timeoutMs = 500;
    if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeoutMs),
                   sizeof(timeoutMs)) == SOCKET_ERROR) {
        cerr << "Receive timeout setup failed: "
             << WSAGetLastError() << '\n';
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    sockaddr_in localAddress{};
    localAddress.sin_family = AF_INET;
    localAddress.sin_port = htons(static_cast<u_short>(port));
    localAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(clientSocket,
             reinterpret_cast<sockaddr*>(&localAddress),
             sizeof(localAddress)) == SOCKET_ERROR) {
        cerr << "Bind failed: " << WSAGetLastError() << '\n';
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    ip_mreq multicastRequest{};

    if (inet_pton(AF_INET, multicastIP.c_str(),
                  &multicastRequest.imr_multiaddr) != 1 ||
        !IN_MULTICAST(ntohl(multicastRequest.imr_multiaddr.s_addr))) {
        cerr << "Invalid IPv4 multicast address.\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    multicastRequest.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(clientSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   reinterpret_cast<const char*>(&multicastRequest),
                   sizeof(multicastRequest)) == SOCKET_ERROR) {
        cerr << "Joining multicast group failed: "
             << WSAGetLastError() << '\n';
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    signal(SIGINT, handleInterrupt);

    cout << "=== UDP Stock Price Client ===\n"
         << "Multicast IP: " << multicastIP << '\n'
         << "UDP port: " << port << '\n'
         << "Waiting for stock updates...\n"
         << "Press Ctrl+C to stop.\n";

    char buffer[2048];

    while (running) {
        sockaddr_in senderAddress{};
        int senderLength = sizeof(senderAddress);

        int received = recvfrom(
            clientSocket, buffer, sizeof(buffer) - 1, 0,
            reinterpret_cast<sockaddr*>(&senderAddress),
            &senderLength
        );

        if (received == SOCKET_ERROR) {
            int error = WSAGetLastError();

            if (error == WSAETIMEDOUT ||
                error == WSAEWOULDBLOCK) {
                continue;
            }

            if (running) {
                cerr << "Receive failed: " << error << '\n';
            }
            break;
        }

        buffer[received] = '\0';
        string message(buffer);

        // Extract timestamp from the server's JSON message.
        const string timestampKey = "\"timestamp\":\"";
        size_t tsStart = message.find(timestampKey);

        if (tsStart == string::npos) {
            cerr << "Invalid message: timestamp missing.\n";
            continue;
        }

        tsStart += timestampKey.size();
        size_t tsEnd = message.find('"', tsStart);

        if (tsEnd == string::npos) {
            cerr << "Invalid timestamp.\n";
            continue;
        }

        string timestamp = message.substr(tsStart, tsEnd - tsStart);

        // Extract the stock object.
        const string stocksKey = "\"stocks\":{";
        size_t dataStart = message.find(stocksKey);

        if (dataStart == string::npos) {
            cerr << "Invalid message: stocks missing.\n";
            continue;
        }

        dataStart += stocksKey.size();
        size_t dataEnd = message.find('}', dataStart);

        if (dataEnd == string::npos) {
            cerr << "Invalid stock data.\n";
            continue;
        }

        string stockData = message.substr(
            dataStart, dataEnd - dataStart
        );

        vector<pair<string, double>> stocks;
        stringstream stream(stockData);
        string item;
        bool valid = true;

        while (getline(stream, item, ',')) {
            size_t colon = item.find(':');
            size_t nameStart = item.find('"');

            if (colon == string::npos ||
                nameStart == string::npos) {
                valid = false;
                break;
            }

            size_t nameEnd = item.find('"', nameStart + 1);

            if (nameEnd == string::npos || nameEnd > colon) {
                valid = false;
                break;
            }

            string symbol = item.substr(
                nameStart + 1, nameEnd - nameStart - 1
            );

            try {
                string priceText = item.substr(colon + 1);
                size_t parsedLength = 0;
                double price = stod(priceText, &parsedLength);

                if (parsedLength != priceText.size() ||
                    price < 0) {
                    valid = false;
                    break;
                }

                stocks.push_back({symbol, price});
            } catch (...) {
                valid = false;
                break;
            }
        }

        // Ensure all five expected symbols appear exactly once.
        const vector<string> expected = {
            "SBI", "KOTAK", "63MOONS",
            "TATASTEEL", "TATACOFFEE"
        };

        if (!valid || stocks.size() != expected.size()) {
            cerr << "Invalid or incomplete stock update.\n";
            continue;
        }

        for (const string& symbol : expected) {
            int count = 0;

            for (const auto& stock : stocks) {
                if (stock.first == symbol) {
                    ++count;
                }
            }

            if (count != 1) {
                valid = false;
                break;
            }
        }

        if (!valid) {
            cerr << "Stock symbols are missing or duplicated.\n";
            continue;
        }

        cout << "\nLast received update: " << timestamp << '\n';
        cout << left << setw(15) << "Stock"
             << right << setw(15) << "LTP (INR)" << '\n';
        cout << string(30, '-') << '\n';

        for (const auto& stock : stocks) {
            cout << left << setw(15) << stock.first
                 << right << setw(15) << fixed
                 << setprecision(2) << stock.second << '\n';
        }
    }

    if (setsockopt(clientSocket, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                   reinterpret_cast<const char*>(&multicastRequest),
                   sizeof(multicastRequest)) == SOCKET_ERROR) {
        cerr << "Warning: failed to leave multicast group.\n";
    }

    closesocket(clientSocket);
    WSACleanup();

    cout << "\nClient stopped successfully.\n";
    return 0;
}
