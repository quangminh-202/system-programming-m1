#include <fcntl.h>
#include <unistd.h>

int main() {
    int data[] = {42, 1, 2, 42, 3, 42, 4, 42, 5, 6, 42};
    int fd = open("../data.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    write(fd, data, sizeof(data));
    close(fd);
    return 0;
}