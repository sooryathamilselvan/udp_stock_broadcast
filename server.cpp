
#include <iostream>
#include <string>
#include <iomanip>
#include <random>
#include <thread>
#include <chrono>
#include <ctime>
#include <cmath>
#include <csignal>
#include <sstream>

#include <winsock2.h>
#include <ws2tcpip.h>

using namespace std;

volatile sig_atomic_t running = 1;

void handleInterrupt(int) {
    running = 0;
}

struct Stock {
    string symbol;
    double price;
};

string getTimestamp() {
    time_t now = time(nullptr);
    tm localTime{};
    localtime_s(&localTime, &now);

    char buffer[20];
    strftime(buffer, sizeof(buffer), "%H:%M:%S", &localTime);
    return string(buffer);
}

int main(int argc, char* argv[]) {
    string multicastIP = "239.255.0.1";
    int port = 5000;
    int intervalMs = 1000;

    try {
        if (argc > 4) {
            cerr << "Usage: server.exe [multicast_ip] [port] [interval_ms]\n";
            return 1;
        }

        if (argc > 1) multicastIP = argv[1];
        if (argc > 2) port = stoi(argv[2]);
        if (argc > 3) intervalMs = stoi(argv[3]);
    } catch (...) {
        cerr << "Invalid numeric argument.\n";
        return 1;
    }

    if (port < 1 || port > 65535) {
        cerr << "Port must be between 1 and 65535.\n";
        return 1;
    }

    if (intervalMs < 100 || intervalMs > 60000) {
        cerr << "Interval must be between 100 and 60000 ms.\n";
        return 1;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Winsock initialization failed.\n";
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSocket == INVALID_SOCKET) {
        cerr << "Socket creation failed: " << WSAGetLastError() << '\n';
        WSACleanup();
        return 1;
    }

    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(static_cast<u_short>(port));

    if (inet_pton(AF_INET, multicastIP.c_str(),
                  &destination.sin_addr) != 1 ||
        !IN_MULTICAST(ntohl(destination.sin_addr.s_addr))) {
        cerr << "Invalid IPv4 multicast address. Use an address from 224.0.0.0 to 239.255.255.255.\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    Stock stocks[] = {
        {"SBI", 812.40},
        {"KOTAK", 1945.20},
        {"63MOONS", 684.50},
        {"TATASTEEL", 156.80},
        {"TATACOFFEE", 1124.60}
    };

    random_device rd;
    mt19937 generator(rd());
    uniform_real_distribution<double> change(-2.0, 2.0);

    signal(SIGINT, handleInterrupt);

    cout << "=== UDP Stock Price Server ===\n"
         << "Multicast IP: " << multicastIP << '\n'
         << "UDP port: " << port << '\n'
         << "Update interval: " << intervalMs << " ms\n"
         << "Press Ctrl+C to stop.\n\n";

    while (running) {
        for (auto& stock : stocks) {
            stock.price = max(
                1.0,
                round((stock.price + change(generator)) * 100.0) / 100.0
            );
        }

        string timestamp = getTimestamp();
        ostringstream message;

        message << "{\"timestamp\":\"" << timestamp << "\",\"stocks\":{";

        for (int i = 0; i < 5; ++i) {
            if (i > 0) message << ',';
            message << '"' << stocks[i].symbol << "\":"
                    << fixed << setprecision(2) << stocks[i].price;
        }
        message << "}}";

        string payload = message.str();

        int sent = sendto(
            serverSocket, payload.c_str(),
            static_cast<int>(payload.size()), 0,
            reinterpret_cast<sockaddr*>(&destination),
            sizeof(destination)
        );

        if (sent == SOCKET_ERROR) {
            cerr << "UDP send failed: " << WSAGetLastError() << '\n';
        } else {
            cout << timestamp;
            for (const auto& stock : stocks) {
                cout << " | " << stock.symbol << ": "
                     << fixed << setprecision(2) << stock.price;
            }
            cout << '\n';
        }

        // Check the shutdown flag every 100 ms.
        int elapsed = 0;
        while (running && elapsed < intervalMs) {
            this_thread::sleep_for(chrono::milliseconds(100));
            elapsed += 100;
        }
    }

    cout << "\nStopping server...\n";
    closesocket(serverSocket);
    WSACleanup();
    cout << "Server stopped successfully.\n";

    return 0;
}
