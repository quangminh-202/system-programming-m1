#include <iostream>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <unistd.h>

using namespace std;

pid_t child_pid;
int number_to_guess;
int last_guess;
int attempts;

void wait_for_signal() {
    do {
        pause();
    } while (true);
}

void send_signal(pid_t pid, int sig, int value) {
    union sigval val;
    val.sival_int = value;
    sigqueue(pid, sig, val);
}

void guess_handler(int sig, siginfo_t *info, void *context) {
    last_guess = info->si_value.sival_int;
    attempts++;
    if (last_guess == number_to_guess) {
        kill(child_pid, SIGUSR1);
    } else {
        kill(child_pid, SIGUSR2);
    }
}

void alrm_handler(int sig) {
    cout << "[Process " << getpid() << "] Timeout! Restarting...\n";
    _exit(0);
}

void correct_guess_handler(int sig) {
    cout << "[Process " << getpid() << "] Correct! The number was " << number_to_guess
         << ". Attempts: " << attempts << "\n";
    _exit(0);
}

void incorrect_guess_handler(int sig) {
    send_signal(getppid(), SIGRTMIN, rand() % 100 + 1);
}

int main() {
    srand(time(nullptr));
    number_to_guess = rand() % 100 + 1; // Giả sử N = 100
    attempts = 0;

    struct sigaction sa_guess, sa_correct, sa_incorrect, sa_alarm;
    sa_guess.sa_flags = SA_SIGINFO;
    sa_guess.sa_sigaction = guess_handler;
    sigaction(SIGRTMIN, &sa_guess, nullptr);

    sa_correct.sa_handler = correct_guess_handler;
    sigaction(SIGUSR1, &sa_correct, nullptr);

    sa_incorrect.sa_handler = incorrect_guess_handler;
    sigaction(SIGUSR2, &sa_incorrect, nullptr);

    sa_alarm.sa_handler = alrm_handler;
    sigaction(SIGALRM, &sa_alarm, nullptr);

    if ((child_pid = fork()) == 0) {
        // Child process (Guesser)
        send_signal(getppid(), SIGRTMIN, rand() % 100 + 1);
        wait_for_signal();
    } else {
        // Parent process (Chooser)
        wait_for_signal();
    }
    return 0;
}
