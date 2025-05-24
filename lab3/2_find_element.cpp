#include <iostream>
#include <vector>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <chrono>
#include <algorithm>
#include "check.hpp"

const int THREADS = 16;
const int VALUE_TO_FIND = 42;
pthread_barrier_t barrier;

std::vector<int> arr;
std::vector<std::vector<int>> local_results(THREADS);
std::vector<int> parallel_result;
std::vector<int> sequential_result;

struct ThreadArg {
    int id;
    int start;
    int end;
};

// Đọc file nhị phân vào mảng arr
void load_binary_file(const char* filename) {
    int fd = check(open(filename, O_RDONLY));
    if (fd < 0) {
        perror("Cannot open file");
        exit(1);
    }

    struct stat st;
    fstat(fd, &st);
    size_t filesize = st.st_size;
    size_t count = filesize / sizeof(int);

    arr.resize(count);
    ssize_t read_bytes = check(read(fd, arr.data(), filesize));
    if (read_bytes != (ssize_t)filesize) {
        perror("Error reading file");
        exit(1);
    }
    close(fd);
    std::cout << "Data in file (" << count << " integers):\n";
    for (size_t i = 0; i < count; ++i) {
        std::cout << arr[i] << " ";
    }
    std::cout << "\n";
}

// Thread function
void* search_worker(void* arg) {
    ThreadArg* t = (ThreadArg*)arg;
    for (int i = t->start; i < t->end; ++i) {
        if (arr[i] == VALUE_TO_FIND)
            local_results[t->id].push_back(i);
    }

    pthread_barrier_wait(&barrier);

    if (t->id == 0) {
        for (const auto& v : local_results)
            parallel_result.insert(parallel_result.end(), v.begin(), v.end());
        std::sort(parallel_result.rbegin(), parallel_result.rend());
    }

    return nullptr;
}

int main() {
    const char* filename = "../data.bin";
    load_binary_file(filename);

    // Tuần tự
    auto start_seq = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < arr.size(); ++i)
        if (arr[i] == VALUE_TO_FIND)
            sequential_result.push_back(i);
    std::sort(sequential_result.rbegin(), sequential_result.rend());
    auto end_seq = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration_seq = end_seq - start_seq;

    // Song song
    check(pthread_barrier_init(&barrier, nullptr, THREADS));
    pthread_t threads[THREADS];
    ThreadArg args[THREADS];
    int chunk = (arr.size() + THREADS - 1) / THREADS;

    auto start_par = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < THREADS; ++i) {
        args[i].id = i;
        args[i].start = i * chunk;
        args[i].end = std::min((int)arr.size(), args[i].start + chunk);
        pthread_create(&threads[i], nullptr, search_worker, &args[i]);
    }
    for (int i = 0; i < THREADS; ++i)
        pthread_join(threads[i], nullptr);
    auto end_par = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration_par = end_par - start_par;
    pthread_barrier_destroy(&barrier);

    // In kết quả
    std::cout << "Sequential result: ";
    for (int i : sequential_result) std::cout << i << " ";
    std::cout << "\nParallel result:   ";
    for (int i : parallel_result) std::cout << i << " ";
    std::cout << "\n";

    std::cout << "\nTime (sequential): " << duration_seq.count() << " sec\n";
    std::cout << "Time (parallel)  : " << duration_par.count() << " sec\n";

    // So sánh kết quả
    if (sequential_result == parallel_result) {
        std::cout << "Results match.\n";
    } else {
        std::cout << "Results mismatch.\n";
    }

    return 0;
}