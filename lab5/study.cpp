#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>

// Luồng phụ - giả sử làm gì đó, ví dụ ghi log
void* logger_thread(void* arg) {
    while (1) {
        printf("Logger thread is running...\n");
        sleep(1);
    }
    return NULL;
}

// Hàm main
int main() {
    pthread_t tid;

    // Tạo một luồng phụ
    pthread_create(&tid, NULL, logger_thread, NULL);

    // Ngủ 2s cho logger chạy thử
    sleep(2);

    pid_t pid = fork();

    if (pid == 0) {
        // Tiến trình con sau fork

        printf(">> Child process: Only this thread exists now.\n");

        // Gọi exec để chạy chương trình khác, ví dụ 'ls'
        execlp("ls", "ls", "-l", NULL);

        // Nếu exec thất bại
        perror("exec failed");
        exit(1);
    } else if (pid > 0) {
        // Tiến trình cha tiếp tục chạy
        printf(">> Parent process continues.\n");
        pthread_join(tid, NULL);  // đợi logger
    } else {
        perror("fork failed");
        return 1;
    }

    return 0;
}