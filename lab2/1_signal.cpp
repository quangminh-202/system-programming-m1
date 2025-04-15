#include <iostream>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <random>
#include <ctime>
#include "check.hpp"

#define N 100
#define MAX_ROUNDS 16

volatile sig_atomic_t answer_number = -1;
volatile sig_atomic_t last_guess = -1;
volatile sig_atomic_t last_response = -1;
pid_t opponent = 0;

//sigset_t mask, old_mask;

void signal_handler(int sig, siginfo_t *info, void *context) {
    if (sig == SIGUSR1 || sig == SIGUSR2) {
        last_response = (sig == SIGUSR1) ? 1 : 0;
    } else if (sig == SIGRTMIN) {
        last_guess = info->si_value.sival_int;
    }
}

void wait_for_signal(const sigset_t&mask) {

    sigsuspend(&mask);
}

void think_number(int round) {
    std::cout << "====================" << std::endl;
    std::cout << "Round " << round << "\n[Thinking of a number process id: " << getpid() << "]"<< std::endl;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, N - 1);
    answer_number = distrib(gen);

    std::cout << "Thinking of number: " << answer_number << std::endl;
    check(kill(opponent, SIGUSR1));

    while (true) {
        sigset_t
        wait_for_signal(SIGRTMIN);
        if (last_guess != -1) {
            if (last_guess == answer_number) {
                std::cout << "Opponent guessed correctly!" << std::endl;
                check(sigqueue(opponent, SIGUSR1, { .sival_int = 1 }));
                break;
            } else {
                std::cout << "Incorrect guess: " << last_guess << std::endl;
                check(sigqueue(opponent, SIGUSR2, { .sival_int = 0 })); // gui tin nhan di
            }
            last_guess = -1;
        }
    }
}

void guess_number(int round) {
    wait_for_signal(SIGUSR1);
    std::vector<bool> guessed(N, false);
    int attempts = 0;
    std::cout << "[Process guessing id: " << getpid() << "]" << std::endl;

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
        check(sigqueue(opponent, SIGRTMIN, { .sival_int = num }));

        wait_for_signal(SIGUSR1);

        if (last_response == 1) {
            clock_t end_time = clock();
            double elapsed_time = double(end_time - start_time) / CLOCKS_PER_SEC;

            std::cout << "Guessed correctly in " << attempts << " attempts!\n";
            std::cout << "Time taken: " << elapsed_time << " seconds\n";
            last_response = -1;
            break;
        }
    }
}

int main() {
    struct sigaction sa = {};
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = signal_handler;
    sigaction(SIGUSR1, &sa, nullptr);
    sigaction(SIGUSR2, &sa, nullptr);
    sigaction(SIGRTMIN, &sa, nullptr);

    srand(time(nullptr));
    pid_t ppid = getpid();
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        return 1;
    }

    opponent = (pid > 0) ? pid : ppid;
    if (pid > 0) {
        for (int i = 1; i <= MAX_ROUNDS; i++) {
            think_number(i);
            guess_number(i);
        }
    } else {
        for (int i = 1; i <= MAX_ROUNDS; i++) {
            guess_number(i);
            think_number(i);
        }
    }

    return 0;
}
