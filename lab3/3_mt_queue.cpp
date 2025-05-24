#include <iostream>
#include <queue>
#include <optional>
#include <pthread.h>
#include <atomic>
#include <sched.h>
#include <string>
#include "check.hpp"

constexpr int M = 3; // số producer compile-time thoi diem bien dich ko thay doi trong runtime
constexpr int N = 2; // số consumer  read
constexpr int ITEMS_PER_PRODUCER = 5; //moi producer tao bao nhieu phan tu
constexpr int QUEUE_CAPACITY = 10;

pthread_mutex_t cout_mutex = PTHREAD_MUTEX_INITIALIZER;

void safe_print(const std::string& msg) {
    check(pthread_mutex_lock(&cout_mutex));
    std::cout << msg << std::endl;
    check(pthread_mutex_unlock(&cout_mutex));
}

template <typename T>
class mt_queue {
    size_t max_size_;
    std::queue<T> queue_;
    mutable pthread_mutex_t mutex_;
    pthread_cond_t not_empty_, not_full_;
public:
    explicit mt_queue(size_t max_size) : max_size_(max_size) {
        pthread_mutex_init(&mutex_, nullptr);
        pthread_cond_init(&not_empty_, nullptr);
        pthread_cond_init(&not_full_, nullptr);
    }

    mt_queue(const mt_queue&) = delete;
    mt_queue(mt_queue&&) = delete;

    ~mt_queue() {
        pthread_mutex_destroy(&mutex_);
        pthread_cond_destroy(&not_empty_);
        pthread_cond_destroy(&not_full_);
    }

    void enqueue(const T& v) {
        check(pthread_mutex_lock(&mutex_));
        while (queue_.size() >= max_size_) {
            check(pthread_cond_wait(&not_full_, &mutex_)); //wait consumer get value
        }
        queue_.push(v);
        check(pthread_cond_signal(&not_empty_));
        check(pthread_mutex_unlock(&mutex_));
    }

    T dequeue() {
        check(pthread_mutex_lock(&mutex_));
        while (queue_.empty()) {
            check(pthread_cond_wait(&not_empty_, &mutex_));
        }
        T val = queue_.front();
        queue_.pop();
        check(pthread_cond_signal(&not_full_)); // bao cho consumer rang co cho trong roi
        check(pthread_mutex_unlock(&mutex_));
        return val;
    }

    std::optional<T> try_dequeue() { //dung de lay
        check(pthread_mutex_lock(&mutex_));
        if (queue_.empty()) {
            check(pthread_mutex_unlock(&mutex_));
            return std::nullopt;
        }
        T val = queue_.front();
        queue_.pop();
        check(pthread_cond_signal(&not_full_));
        check(pthread_mutex_unlock(&mutex_));
        return val;
    }

    bool try_enqueue(const T& v) { //dung de ghi
        check(pthread_mutex_lock(&mutex_));
        if (queue_.size() >= max_size_) {
            check(pthread_mutex_unlock(&mutex_));
            return false;
        }
        queue_.push(v);
        check(pthread_cond_signal(&not_empty_));
        check(pthread_mutex_unlock(&mutex_));
        return true;
    }

    bool full() const {
        pthread_mutex_lock(&mutex_);
        bool result = queue_.size() >= max_size_;
        pthread_mutex_unlock(&mutex_);
        return result;
    }

    bool empty() const {
        check(pthread_mutex_lock(&mutex_));
        bool result = queue_.empty();
        check(pthread_mutex_unlock(&mutex_));
        return result;
    }

    void stop(){

    }
};

mt_queue<int> queue(QUEUE_CAPACITY);
std::atomic<int> finished_producers = 0; //bien dem producer, consumer biet toan bo pro da xong

void* producer(void* arg) { //write
    int id = *(int*)arg;
    for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
        int val = id * 100 + i;
        queue.enqueue(val);
        safe_print("[Producer " + std::to_string(id) + "] Enqueued: " + std::to_string(val));
    }
    finished_producers.fetch_add(1);
    return nullptr;
}

void* consumer(void* arg) { //read
    int id = *(int*)arg;
    while (true) {
        auto val_opt = queue.try_dequeue();

        if (val_opt.has_value()) {
            int val = val_opt.value();
            safe_print("[Consumer " + std::to_string(id) + "] Dequeued: " + std::to_string(val));
        } else {
            // Nếu rỗng và tất cả producer đã xong => dừng
            if (finished_producers == M && queue.empty()) {
                break;
            }
            sched_yield(); //busy-wait
        }
    }
    return nullptr;
}

int main() {
    pthread_t producers[M], consumers[N];
    int prod_ids[M], cons_ids[N];
    for (int i = 0; i < M; ++i) {
        prod_ids[i] = i;
        pthread_create(&producers[i], nullptr, producer, &prod_ids[i]);
    }

    for (int i = 0; i < N; ++i) {
        cons_ids[i] = i;
        pthread_create(&consumers[i], nullptr, consumer, &cons_ids[i]);
    }

    for (int i = 0; i < M; ++i)
        pthread_join(producers[i], nullptr);


    for (int i = 0; i < N; ++i)
        pthread_join(consumers[i], nullptr);

    safe_print("All producers and consumers finished.");
    return 0;
}