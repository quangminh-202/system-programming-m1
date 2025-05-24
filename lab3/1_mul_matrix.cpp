#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <pthread.h>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring> // dùng cho strerror()
#include "check.hpp"

using namespace std;

using Matrix = vector<double>; //flatten: 1 chieu

int n = 0; // matrix size
Matrix A, B, C_seq, C_par;

struct ThreadArg {
    int start_row;
    int end_row;
};

Matrix read_matrix(const string& filename) {
    int fd = check(open(filename.c_str(), O_RDONLY));
    if (fd < 0) {
        throw runtime_error("Cannot open file: " + filename + " - " + strerror(errno));
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        check(close(fd));
        throw runtime_error("Cannot stat file: " + filename);
    }

    size_t filesize = st.st_size;
    int elements = filesize / sizeof(double);
    int dim = sqrt(elements);
    if (dim * dim != elements) {
        check(close(fd));
        throw runtime_error("File size is not a square matrix of doubles");
    }

    n = dim; // cập nhật biến toàn cục

    Matrix mat(elements);
    ssize_t bytes_read = read(fd, mat.data(), filesize);
    if (bytes_read != (ssize_t)filesize) {
        close(fd);
        throw runtime_error("Failed to read entire matrix from file: " + filename);
    }

    close(fd);
    return mat;
}
//Matrix read_matrix(const string& filename) {
//    ifstream file(filename, ios::binary | ios::ate); // at end for tellg() = tell get input file stream
//    if (!file) throw runtime_error("Cannot open file: " + filename);
//
//    streamsize size = file.tellg();
//    file.seekg(0, ios::beg); //seek get begin: tim vi tri va lay
//
//    int elements = size / sizeof(double);
//    int dim = sqrt(elements);
//    n = dim; // update global
//
//    Matrix mat(elements);
//    file.read(reinterpret_cast<char*>(mat.data()), size); //reinterpret: hieu lai
//    return mat;
//}

void multiply_seq() {
    C_seq.assign(n * n, 0); //assign: giao pho
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            for (int k = 0; k < n; ++k)
                C_seq[i * n + j] += A[i * n + k] * B[k * n + j];
}

void* thread_multiply(void* arg) {
    ThreadArg* t = static_cast<ThreadArg*>(arg);
    for (int i = t->start_row; i < t->end_row; ++i)
        for (int j = 0; j < n; ++j) {
            double sum = 0;
            for (int k = 0; k < n; ++k)
                sum += A[i * n + k] * B[k * n + j];
            C_par[i * n + j] = sum;
        }
    return nullptr;
}

void multiply_parallel(int thread_count = 16) {
    thread_count = std::min(thread_count, n);
    C_par.assign(n * n, 0);

    vector<pthread_t> threads(thread_count); //mang luong
    vector<ThreadArg> args(thread_count); // mang doi so truyen vao luong

    int rows_per_thread = n / thread_count; //
    for (int i = 0; i < thread_count; ++i) {
        args[i].start_row = i * rows_per_thread;
        args[i].end_row = (i == thread_count - 1) ? n : args[i].start_row + rows_per_thread;
        pthread_create(&threads[i], nullptr, thread_multiply, &args[i]);
    }

    for (auto& t : threads) //wait for threads
        pthread_join(t, nullptr);
}

void print_matrix(const Matrix& M) {
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j)
            cout << M[i * n + j] << "\t";
        cout << "\n";
    }
}

int main() {
    try {
        A = read_matrix("../small_matrix1.bin");
        B = read_matrix("../small_matrix2.bin");

        cout << "Matrix size: " << n << "x" << n << endl;

        auto t1 = chrono::high_resolution_clock::now();
        multiply_seq();
        auto t2 = chrono::high_resolution_clock::now();

        cout << "\nSequential result:\n";
        print_matrix(C_seq);

        chrono::duration<double> duration_seq = t2 - t1;
        cout << "Sequential time: " << duration_seq.count() << "s\n";

        t1 = chrono::high_resolution_clock::now();
        multiply_parallel();
        t2 = chrono::high_resolution_clock::now();

        cout << "\nParallel result:\n";
        print_matrix(C_par);

        chrono::duration<double> duration_par = t2 - t1;
        cout << "Parallel time: " << duration_par.count() << "s\n";

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }

    return 0;
}