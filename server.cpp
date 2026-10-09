
#include <iostream>
#include <string>
#include <iomanip>
#include <random>
#include <chrono>
#include <thread>
#include <ctime>
#include <cmath>
#include <csignal>

#include <winsock2.h>
#include <ws2tcpip.h>

using namespace std;

// ---------------- CONFIGURATION ----------------

const char* MULTICAST_IP = "239.255.0.1";
const int PORT = 5000;
const int UPDATE_INTERVAL_MS = 1000;

// Signal-safe flag for graceful shutdown
volatile sig_atomic_t running = 1;

void handleInterrupt(int signal) {
    running = 0;
}

// ---------------- STOCK STRUCTURE ----------------

struct Stock {
    string symbol;
    double price;
};

// ---------------- TIMESTAMP FUNCTION ----------------

string getTimestamp() {
    time_t now = time(nullptr);
    tm localTime{};

    localtime_s(&localTime, &now);

    char buffer[20];

    strftime(
        buffer,
        sizeof(buffer),
        "%H:%M:%S",
        &localTime
    );

    return string(buffer);
}

// ---------------- MAIN FUNCTION ----------------

int main() {

    // Register Ctrl+C handler
    signal(SIGINT, handleInterrupt);

    // 1. Initialize Winsock
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Winsock initialization failed.\n";
        return 1;
    }

    // 2. Create UDP socket
    SOCKET serverSocket = socket(
        AF_INET,
        SOCK_DGRAM,
        IPPROTO_UDP
    );

    if (serverSocket == INVALID_SOCKET) {
        cerr << "Socket creation failed. Error: "
             << WSAGetLastError() << '\n';

        WSACleanup();
        return 1;
    }

    // 3. Configure multicast destination
    sockaddr_in destination{};

    destination.sin_family = AF_INET;
    destination.sin_port = htons(PORT);

    if (inet_pton(
            AF_INET,
            MULTICAST_IP,
            &destination.sin_addr
        ) != 1) {

        cerr << "Invalid multicast IP address.\n";

        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // 4. Initialize five stocks with starting LTP values
    Stock stocks[] = {
        {"SBI", 812.40},
        {"KOTAK", 1945.20},
        {"63MOONS", 684.50},
        {"TATASTEEL", 156.80},
        {"TATACOFFEE", 1124.60}
    };

    const int stockCount = 5;

    // 5. Random number generator for simulated price changes
    random_device rd;
    mt19937 generator(rd());

    uniform_real_distribution<double> priceChange(-2.0, 2.0);

    // 6. Display server information
    cout << "====================================\n";
    cout << "       UDP STOCK PRICE SERVER       \n";
    cout << "====================================\n";

    cout << "Multicast group : " << MULTICAST_IP << '\n';
    cout << "UDP port        : " << PORT << '\n';
    cout << "Update interval : "
         << UPDATE_INTERVAL_MS << " ms\n";

    cout << "Broadcasting stock prices...\n";
    cout << "Press Ctrl+C to stop the server.\n\n";

    cout << left << setw(12) << "Timestamp";

    for (const auto& stock : stocks) {
        cout << right << setw(14) << stock.symbol;
    }

    cout << "\n";

    cout << string(82, '-') << '\n';

    // 7. Generate and multicast stock prices periodically
    while (running) {

        // Update all five prices
        for (int i = 0; i < stockCount; i++) {

            stocks[i].price += priceChange(generator);

            // Ensure the simulated price remains positive
            if (stocks[i].price < 1.0) {
                stocks[i].price = 1.0;
            }

            // Round to two decimal places
            stocks[i].price =
                round(stocks[i].price * 100.0) / 100.0;
        }

        string timestamp = getTimestamp();

        // 8. Create one JSON message containing all five stocks
        string message =
            "{\"timestamp\":\"" + timestamp + "\",\"stocks\":{";

        for (int i = 0; i < stockCount; i++) {

            if (i > 0) {
                message += ",";
            }

            message += "\"" + stocks[i].symbol + "\":";

            // Serialize the price with exactly two decimal places
            ostringstream priceStream;

            priceStream << fixed << setprecision(2)
                        << stocks[i].price;

            message += priceStream.str();
        }

        message += "}}";

        // 9. Send one UDP datagram to the multicast group
        int bytesSent = sendto(
            serverSocket,
            message.c_str(),
            static_cast<int>(message.size()),
            0,
            reinterpret_cast<sockaddr*>(&destination),
            sizeof(destination)
        );

        if (bytesSent == SOCKET_ERROR) {

            cerr << "UDP send failed. Error: "
                 << WSAGetLastError() << '\n';

        } else if (bytesSent != static_cast<int>(message.size())) {

            cerr << "Warning: unexpected UDP send length.\n";

        } else {

            // 10. Display the same generated data on the server
            cout << left << setw(12) << timestamp;

            for (const auto& stock : stocks) {
                cout << right << setw(14)
                     << fixed << setprecision(2)
                     << stock.price;
            }

            cout << '\n';
        }

        // Wait in short intervals so Ctrl+C is noticed promptly
        const int sleepStepMs = 100;
        int elapsedMs = 0;

        while (running && elapsedMs < UPDATE_INTERVAL_MS) {
            this_thread::sleep_for(
                chrono::milliseconds(sleepStepMs)
            );

            elapsedMs += sleepStepMs;
        }
    }

    // 11. Clean shutdown
    cout << "\n\nStopping UDP stock price server...\n";

    if (closesocket(serverSocket) == SOCKET_ERROR) {
        cerr << "Socket close failed. Error: "
             << WSAGetLastError() << '\n';
    }

    WSACleanup();

    cout << "Winsock resources released.\n";
    cout << "Server stopped successfully.\n";

    return 0;
}
