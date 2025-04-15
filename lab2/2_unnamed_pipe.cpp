#include <iostream>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <random>
#include <errno.h>
#include <string.h>
#include "check.hpp"

#define N 10
#define MAX_ROUNDS 3

struct Message {
    int type;  // 1: ready, 2: guess, 3: response
    int value;
};

bool check_opponent_alive(pid_t pid) {
    if (check(kill(pid, 0)) == -1 && errno == ESRCH) {
        std::cout << "Opponent has exited. Game over!" << std::endl;
        return false;
    }
    return true;
}

bool think_number(int round, int write_fd, int read_fd, pid_t opponent_pid) {
    std::cout << "====================" << std::endl;
    std::cout << "Round " << round << "\n[Thinking of a number process id: " << getpid() << "]" << std::endl;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, N - 1);
    int answer_number = distrib(gen);

    std::cout << "Thinking of number: " << answer_number << std::endl;

    Message ready_msg = {1, 0};
    if (check(write(write_fd, &ready_msg, sizeof(Message))) == -1) {
        if (!check_opponent_alive(opponent_pid)) return false;
        std::cerr << "Write failed: " << strerror(errno) << std::endl;
        return false;
    }

    while (true) {
        if (!check_opponent_alive(opponent_pid)) return false;

        Message guess_msg;
        if (check(read(read_fd, &guess_msg, sizeof(Message))) == -1) {
            if (!check_opponent_alive(opponent_pid)) return false;
            std::cerr << "Read failed: " << strerror(errno) << std::endl;
            return false;
        }

        if (guess_msg.type == 2) {
            if (guess_msg.value == answer_number) {
                std::cout << "Opponent guessed correctly!" << std::endl;
                Message response = {3, 1};
                if (check(write(write_fd, &response, sizeof(Message))) == -1) {
                    if (!check_opponent_alive(opponent_pid)) return false;
                    std::cerr << "Write failed: " << strerror(errno) << std::endl;
                    return false;
                }
                return true;
            } else {
                std::cout << "Incorrect guess: " << guess_msg.value << std::endl;
                Message response = {3, 0};
                if (check(write(write_fd, &response, sizeof(Message))) == -1) {
                    if (!check_opponent_alive(opponent_pid)) return false;
                    std::cerr << "Write failed: " << strerror(errno) << std::endl;
                    return false;
                }
            }
        }
    }
}

bool guess_number(int round, int write_fd, int read_fd, pid_t opponent_pid) {
    Message ready_msg;
    if (check(read(read_fd, &ready_msg, sizeof(Message))) == -1) {
        if (!check_opponent_alive(opponent_pid)) return false;
        std::cerr << "Read failed: " << strerror(errno) << std::endl;
        return false;
    }
    if (ready_msg.type != 1) return false;

    std::vector<bool> guessed(N, false);
    int attempts = 0;
    std::cout << "[Guessing process id: " << getpid() << "]" << std::endl;

    clock_t start_time = clock();

    while (true) {
        if (!check_opponent_alive(opponent_pid)) return false;
        // check if all true
        bool all_guessed = true;
        for (int i = 0; i < N; i++) {
            if (!guessed[i]) {
                all_guessed = false;
                break;
            }
        }
        if (all_guessed) {
            std::cerr << "Error: All possible numbers have been guessed!" << std::endl;
            break;
        }
        int num;
        do {
            num = rand() % N;
        } while (guessed[num]);

        guessed[num] = true;
        attempts++;

        std::cout << "Guessing: " << num << std::endl;
        Message guess = {2, num};
        if (check(write(write_fd, &guess, sizeof(Message))) == -1) {
            if (!check_opponent_alive(opponent_pid)) return false;
            std::cerr << "Write failed: " << strerror(errno) << std::endl;
            return false;
        }

        Message response;
        if (check(read(read_fd, &response, sizeof(Message))) == -1) {
            if (!check_opponent_alive(opponent_pid)) return false;
            std::cerr << "Read failed: " << strerror(errno) << std::endl;
            return false;
        }

        if (response.type == 3 && response.value == 1) {
            clock_t end_time = clock();
            double elapsed_time = double(end_time - start_time) / CLOCKS_PER_SEC;

            std::cout << "Guessed correctly in " << attempts << " attempts!\n";
            std::cout << "Time taken: " << elapsed_time << " seconds\n";
            return true;
        }
    }
}

int main() {
    srand(time(nullptr));

    int parent_to_child[2];
    int child_to_parent[2];

    if (check(pipe(parent_to_child)) == -1 || check(pipe(child_to_parent)) == -1) {
        std::cerr << "Pipe creation failed: " << strerror(errno) << std::endl;
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Fork failed: " << strerror(errno) << std::endl;
        check(close(parent_to_child[0]));
        check(close(parent_to_child[1]));
        check(close(child_to_parent[0]));
        check(close(child_to_parent[1]));
        return 1;
    }

    if (pid > 0) {  // Parent process
        check(close(parent_to_child[0]));
        check(close(child_to_parent[1]));

        for (int i = 1; i <= MAX_ROUNDS; i++) {
            if (!think_number(i, parent_to_child[1], child_to_parent[0], pid)) break;
            if (!guess_number(i, parent_to_child[1], child_to_parent[0], pid)) break;
        }

        int status;
        wait(&status);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            std::cout << "Child exited abnormally with status " << WEXITSTATUS(status) << std::endl;
        }

        check(close(parent_to_child[1]));
        check(close(child_to_parent[0]));
    } else {  // Child process
        check(close(parent_to_child[1]));
        check(close(child_to_parent[0]));

        for (int i = 1; i <= MAX_ROUNDS; i++) {
            if (!guess_number(i, child_to_parent[1], parent_to_child[0], getppid())) break;
            if (!think_number(i, child_to_parent[1], parent_to_child[0], getppid())) break;
        }

        check(close(parent_to_child[0]));
        check(close(child_to_parent[1]));
        exit(0);
    }

    return 0;
}