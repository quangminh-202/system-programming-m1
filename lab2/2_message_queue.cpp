#include <iostream>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <random>
#include <mqueue.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "check.hpp"

#define N 10
#define MAX_ROUNDS 3
#define QUEUE_NAME_1 "/number_game_1"
#define QUEUE_NAME_2 "/number_game_2"

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

bool think_number(int round, mqd_t mq_out, mqd_t mq_in, pid_t opponent_pid) {
    std::cout << "====================" << std::endl;
    std::cout << "Round " << round << "\n[Thinking of a number process id: " << getpid() << "]" << std::endl;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, N - 1);
    int answer_number = distrib(gen);

    std::cout << "Thinking of number: " << answer_number << std::endl;

    Message ready_msg = {1, 0};
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 2;

    if (check(mq_timedsend(mq_out, (const char*)&ready_msg, sizeof(Message), 0, &ts)) == -1) {
        if (!check_opponent_alive(opponent_pid)) return false;
        std::cerr << "mq_timedsend failed: " << strerror(errno) << std::endl;
        return false;
    }

    while (true) {
        Message guess_msg;
        if (check(mq_receive(mq_in, (char*)&guess_msg, sizeof(Message), nullptr)) == -1) {
            if (!check_opponent_alive(opponent_pid)) return false;
            std::cerr << "mq_receive failed: " << strerror(errno) << std::endl;
            return false;
        }

        if (guess_msg.type == 2) {
            if (guess_msg.value == answer_number) {
                std::cout << "Opponent guessed correctly!" << std::endl;
                Message response = {3, 1};
                if (check(mq_send(mq_out, (const char*)&response, sizeof(Message), 0)) == -1) {
                    if (!check_opponent_alive(opponent_pid)) return false;
                    std::cerr << "mq_send failed: " << strerror(errno) << std::endl;
                    return false;
                }
                return true;
            } else {
                std::cout << "Incorrect guess: " << guess_msg.value << std::endl;
                Message response = {3, 0};
                if (check(mq_timedsend(mq_out, (const char*)&response, sizeof(Message), 0, &ts)) == -1) {
                    if (!check_opponent_alive(opponent_pid)) return false;
                    std::cerr << "mq_send failed: " << strerror(errno) << std::endl;
                    return false;
                }
            }
        }
    }
}

bool guess_number(int round, mqd_t mq_out, mqd_t mq_in, pid_t opponent_pid) {
    Message ready_msg;
    if (check(mq_receive(mq_in, (char*)&ready_msg, sizeof(Message), nullptr)) == -1) {
        if (!check_opponent_alive(opponent_pid)) return false;
        std::cerr << "mq_receive failed: " << strerror(errno) << std::endl;
        return false;
    }
    if (ready_msg.type != 1) return false;

    std::vector<bool> guessed(N, false);
    int attempts = 0;
    std::cout << "[Guessing process id: " << getpid() << "]" << std::endl;

    clock_t start_time = clock();

    while (true) {
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
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 2;
        if (check(mq_timedsend(mq_out, (const char*)&guess, sizeof(Message), 0, &ts)) == -1) {
            if (!check_opponent_alive(opponent_pid)) return false;
            std::cerr << "mq_send failed: " << strerror(errno) << std::endl;
            return false;
        }

        Message response;
        if (check(mq_receive(mq_in, (char*)&response, sizeof(Message), nullptr)) == -1) {
            if (!check_opponent_alive(opponent_pid)) return false;
            std::cerr << "mq_receive failed: " << strerror(errno) << std::endl;
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

    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 1;
    attr.mq_msgsize = sizeof(Message);
    attr.mq_curmsgs = 0;

    mq_unlink(QUEUE_NAME_1);
    mq_unlink(QUEUE_NAME_2);

    mqd_t mq_parent = check(mq_open(QUEUE_NAME_1, O_CREAT | O_RDWR, 0644, &attr));
    if (mq_parent == (mqd_t)-1) {
        std::cerr << "Parent queue creation failed: " << strerror(errno) << std::endl;
        return 1;
    }

    mqd_t mq_child = check(mq_open(QUEUE_NAME_2, O_CREAT | O_RDWR, 0644, &attr));
    if (mq_child == (mqd_t)-1) {
        std::cerr << "Child queue creation failed: " << strerror(errno) << std::endl;
        check(mq_close(mq_parent));
        check(mq_unlink(QUEUE_NAME_1));
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Fork failed: " << strerror(errno) << std::endl;
        check(mq_close(mq_parent));
        check(mq_close(mq_child));
        check(mq_unlink(QUEUE_NAME_1));
        check(mq_unlink(QUEUE_NAME_2));
        return 1;
    }

    pid_t opponent_pid = (pid > 0) ? pid : getppid();

    if (pid > 0) {  // Parent process
        for (int i = 1; i <= MAX_ROUNDS; i++) {
            if (!think_number(i, mq_parent, mq_child, opponent_pid)) break;
            if (!guess_number(i, mq_parent, mq_child, opponent_pid)) break;
        }
        int status;
        wait(&status);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            std::cout << "Child exited abnormally with status " << WEXITSTATUS(status) << std::endl;
        }
    } else {  // Child process
        for (int i = 1; i <= MAX_ROUNDS; i++) {
            if (!guess_number(i, mq_child, mq_parent, opponent_pid)) break;
            if (!think_number(i, mq_child, mq_parent, opponent_pid)) break;
        }
        exit(0);
    }

    check(mq_close(mq_parent));
    check(mq_close(mq_child));
    if (pid > 0) {  // Only parent should unlink
        check(mq_unlink(QUEUE_NAME_1));
        check(mq_unlink(QUEUE_NAME_2));
    }

    return 0;
}