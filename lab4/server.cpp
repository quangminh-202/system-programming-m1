#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <fcntl.h>
#include <signal.h>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <random>
#include "check.hpp"

#define PORT 60002
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

void log_sync(const std::string& msg) {
    flock(1, LOCK_EX);
    std::cout << "[LOG] " << msg << std::endl;
    flock(1, LOCK_UN);
}

std::string addr_to_str(const sockaddr_in& addr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    return std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
}

void handle_client(int client_sock, sockaddr_in client_addr) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100);
    int secret = dis(gen);
    std::cout << "secret: " << secret << std::endl;

    std::string addr = addr_to_str(client_addr);

    while (true) {
        int guess;
        int bytes = recv(client_sock, &guess, sizeof(guess), 0);
        if (bytes <= 0) break;
//        buffer[bytes] = '\0';
//        std::string guess_str(buffer);
//        int guess;

        int response;
        if (guess < secret)
            response = -1; //"Too low"; // -1
        else if (guess > secret)
            response = 1; // "Too high"; // 1
        else {
            response = 0; //"Correct!"; // 0
        }

        send(client_sock, &response, sizeof(response), 0);
        log_sync(addr + " guessed " + std::to_string(guess) + " -> " +
                    (response == 0 ? "Correct!" : (response < 0 ? "Too low" : "Too high")));
        if (response == 0)
            break;
    }
}

void zombie_ignore() {
    struct sigaction sa{};
    sa.sa_handler = SIG_IGN;
    sigaction(SIGCHLD, &sa, nullptr);
}

int main() {
    int server_fd = check(socket(AF_INET, SOCK_STREAM, 0));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET; //IPv4 address family internet
    server_addr.sin_port = htons(PORT);  //network byte order
    server_addr.sin_addr.s_addr = INADDR_ANY; //cho phep server nhan ket noi tu moi IP

    check(bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)));
    check(listen(server_fd, MAX_CLIENTS) );

    zombie_ignore();
    log_sync("Server listening on port " + std::to_string(PORT));

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = check(accept(server_fd, (sockaddr*)&client_addr, &client_len));

        std::string client_info = addr_to_str(client_addr);
        log_sync("Connected: " + client_info);

        pid_t pid = fork();
        if (pid == 0) { // child
            close(server_fd);
            handle_client(client_fd, client_addr);
            close(client_fd);
            log_sync("Disconnected: " + client_info);
            exit(0);
        } else if (pid > 0) {
            close(client_fd); // parent đóng socket dùng bởi child
        } else {
            perror("fork");
        }
    }
}

