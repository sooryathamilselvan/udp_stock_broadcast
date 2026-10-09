
#include <iostream>
#include <string>
#include <iomanip>
#include <sstream>
#include <vector>
#include <csignal>

#include <winsock2.h>
#include <ws2tcpip.h>

using namespace std;

// Configuration
const char* MULTICAST_IP = "239.255.0.1";
const int PORT = 5000;

volatile sig_atomic_t running = 1;

void handleInterrupt(int signal) {
    running = 0;
}

int main() {
    signal(SIGINT, handleInterrupt);

    // Initialize Winsock
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Winsock initialization failed.\n";
        return 1;
    }

    // Create UDP socket
    SOCKET clientSocket = socket(
        AF_INET, SOCK_DGRAM, IPPROTO_UDP
    );

    if (clientSocket == INVALID_SOCKET) {
        cerr << "Socket creation failed: "
             << WSAGetLastError() << '\n';
        WSACleanup();
        return 1;
    }

    // Allow multiple clients to share the UDP port
    BOOL reuse = TRUE;

    if (setsockopt(
            clientSocket, SOL_SOCKET, SO_REUSEADDR,
            reinterpret_cast<const char*>(&reuse),
            sizeof(reuse)) == SOCKET_ERROR) {
        cerr << "Socket option failed: "
             << WSAGetLastError() << '\n';
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    // Set receive timeout to 500 milliseconds
    DWORD timeoutMs = 500;

    if (setsockopt(
            clientSocket, SOL_SOCKET, SO_RCVTIMEO,
            reinterpret_cast<const char*>(&timeoutMs),
            sizeof(timeoutMs)) == SOCKET_ERROR) {
        cerr << "Receive timeout setup failed: "
             << WSAGetLastError() << '\n';
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    // Bind to the UDP port
    sockaddr_in localAddress{};

    localAddress.sin_family = AF_INET;
    localAddress.sin_port = htons(PORT);
    localAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(
            clientSocket,
            reinterpret_cast<sockaddr*>(&localAddress),
            sizeof(localAddress)) == SOCKET_ERROR) {
        cerr << "Bind failed: "
             << WSAGetLastError() << '\n';
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    // Join multicast group
    ip_mreq multicastRequest{};

    if (inet_pton(
            AF_INET, MULTICAST_IP,
            &multicastRequest.imr_multiaddr) != 1) {
        cerr << "Invalid multicast IP address.\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    multicastRequest.imr_interface.s_addr =
        htonl(INADDR_ANY);

    if (setsockopt(
            clientSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
            reinterpret_cast<const char*>(&multicastRequest),
            sizeof(multicastRequest)) == SOCKET_ERROR) {
        cerr << "Joining multicast group failed: "
             << WSAGetLastError() << '\n';
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    cout << "=== UDP Stock Price Client ===\n";
    cout << "Multicast group: " << MULTICAST_IP << '\n';
    cout << "UDP port: " << PORT << '\n';
    cout << "Waiting for stock updates...\n";
    cout << "Press Ctrl+C to stop.\n";

    char buffer[2048];

    while (running) {
        sockaddr_in senderAddress{};
        int senderLength = sizeof(senderAddress);

        int bytesReceived = recvfrom(
            clientSocket,
            buffer,
            sizeof(buffer) - 1,
            0,
            reinterpret_cast<sockaddr*>(&senderAddress),
            &senderLength
        );

        if (bytesReceived == SOCKET_ERROR) {
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

        buffer[bytesReceived] = '\0';
        string message(buffer);

        // Extract timestamp
        size_t timestampStart =
            message.find("\"timestamp\":\"");

        if (timestampStart == string::npos) {
            cerr << "Invalid message: timestamp missing.\n";
            continue;
        }

        timestampStart += string("\"timestamp\":\"").length();

        size_t timestampEnd =
            message.find('"', timestampStart);

        if (timestampEnd == string::npos) {
            cerr << "Invalid timestamp format.\n";
            continue;
        }

        string timestamp = message.substr(
            timestampStart,
            timestampEnd - timestampStart
        );

        // Extract stock section
        size_t stocksStart = message.find("\"stocks\":{");

        if (stocksStart == string::npos) {
            cerr << "Invalid message: stocks missing.\n";
            continue;
        }

        stocksStart += string("\"stocks\":{").length();

        size_t stocksEnd = message.find('}', stocksStart);

        if (stocksEnd == string::npos) {
            cerr << "Invalid stock data format.\n";
            continue;
        }

        string stockData = message.substr(
            stocksStart, stocksEnd - stocksStart
        );

        vector<pair<string, double>> stocks;
        stringstream dataStream(stockData);
        string item;

        while (getline(dataStream, item, ',')) {
            size_t colon = item.find(':');
            size_t nameStart = item.find('"');

            if (colon == string::npos ||
                nameStart == string::npos) {
                continue;
            }

            size_t nameEnd = item.find('"', nameStart + 1);

            if (nameEnd == string::npos || nameEnd > colon) {
                continue;
            }

            string symbol = item.substr(
                nameStart + 1,
                nameEnd - nameStart - 1
            );

            try {
                size_t parsedLength = 0;
                string priceText = item.substr(colon + 1);

                double price = stod(priceText, &parsedLength);

                if (parsedLength != priceText.size()) {
                    continue;
                }

                stocks.push_back({symbol, price});
            } catch (const exception&) {
                cerr << "Invalid price received.\n";
            }
        }

        if (stocks.size() != 5) {
            cerr << "Incomplete stock update received.\n";
            continue;
        }

        // Display received update
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

    // Leave multicast group
    if (setsockopt(
            clientSocket, IPPROTO_IP, IP_DROP_MEMBERSHIP,
            reinterpret_cast<const char*>(&multicastRequest),
            sizeof(multicastRequest)) == SOCKET_ERROR) {
        cerr << "Warning: failed to leave multicast group.\n";
    }

    // Release resources
    closesocket(clientSocket);
    WSACleanup();

    cout << "\nClient stopped successfully.\n";

    return 0;
}
