#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <string>
#include "check.hpp"

#define PORT 60002

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ./client <server_ip>" << std::endl;
        return 1;
    }

    int sock = check(socket(AF_INET, SOCK_STREAM, 0));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, argv[1], &server_addr.sin_addr);

    check(connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)));

    while (true) {
        std::cout << "Enter your guess (1-100): ";
        std::string input;
        std::getline(std::cin, input);
        if (input.empty()) continue;

        int guess;
        try {
            guess = std::stoi(input); // for int
        } catch (...) {
            std::cerr << "Invalid input. Please enter an integer." << std::endl;
            continue;
        }

        check(send(sock, &guess, sizeof(guess), 0));

        int result;
        int bytes = recv(sock, &result, sizeof(result), 0);
        if (bytes <= 0) break;

        if (result < 0) {
            std::cout << "Server: Too low" << std::endl;
        } else if (result > 0) {
            std::cout << "Server: Too high" << std::endl;
        } else {
            std::cout << "Server: Correct!" << std::endl;
            break;
        }
    }

    close(sock);
    return 0;
}