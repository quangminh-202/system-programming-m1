#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

void check(int result, const char *message) {
    if (result == -1) {
        perror(message);
        exit(EXIT_FAILURE);
    }
}

void create_directory(const char *path) {
    check(mkdir(path, 0755), "Error creating directory");
    printf("Created directory: %s\n", path);
}

void create_file(const char *path, const char *content, size_t size) {
    int fd = open(path, O_CREAT | O_WRONLY, 0644);
    check(fd, "Error creating file");
    if (content) {
        check(write(fd, content, strlen(content)), "Error writing to file");
    } else {
        check(ftruncate(fd, size), "Error setting file size");
    }
    check(close(fd), "Error closing file");
    printf("Created file: %s\n", path);
}

void create_symlink(const char *target, const char *linkpath) {
    check(symlink(target, linkpath), "Error creating symlink");
    printf("Created symlink: %s -> %s\n", linkpath, target);
}

void create_hardlink(const char *target, const char *linkpath) {
    check(link(target, linkpath), "Error creating hard link");
    printf("Created hard link: %s -> %s\n", linkpath, target);
}

void create_random_binary_file(const char *path, size_t size) {
    int fd = open(path, O_CREAT | O_WRONLY, 0644);
    check(fd, "Error creating binary file");
    for (size_t i = 0; i < size; i++) {
        unsigned char random_byte = rand() % 256;
        check(write(fd, &random_byte, 1), "Error writing random data");
    }
    check(close(fd), "Error closing binary file");
    printf("Created binary file: %s (%zu bytes)\n", path, size);
}

void create_zero_filled_file(const char *path, size_t size) {
    int fd = open(path, O_CREAT | O_WRONLY, 0644);
    check(fd, "Error creating zero-filled file");
    check(ftruncate(fd, size), "Error setting file size");
    check(close(fd), "Error closing file");
    printf("Created zero-filled file: %s (%zu bytes)\n", path, size);
}

void remove_directory(const char *path) {
    struct dirent *entry;
    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir");
        return;
    }
    char filepath[4096];
    struct stat statbuf;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);
        if (stat(filepath, &statbuf) == 0) {
            if (S_ISDIR(statbuf.st_mode)) {
                remove_directory(filepath);
            } else {
                check(unlink(filepath), "Error deleting file");
            }
        }
    }
    closedir(dir);
    check(rmdir(path), "Error removing directory");
}

int main() {
    // Create directories
    create_directory("/home/quangminh/HarryPotter");
    create_directory("/home/quangminh/HarryPotter/Hufflepuff");
    create_directory("/home/quangminh/HarryPotter/Ravenclaw");

    // Create files
    create_random_binary_file("/home/quangminh/HarryPotter/Hufflepuff/file1.txt", 1024);
    create_file("/home/quangminh/HarryPotter/Hufflepuff/file2.txt", "Do what is right, not what is easy", 0);
    create_zero_filled_file("/home/quangminh/HarryPotter/Ravenclaw/file3.txt", 1048576);
    create_file("/home/quangminh/HarryPotter/Ravenclaw/file4.txt", "Wit beyond measure is man's greatest treasure.", 0);

    // Create symlinks
    create_symlink("/home/quangminh/HarryPotter/Hufflepuff/file1.txt", "/home/quangminh/HarryPotter/symlink.txt");

    // Create hard link
    create_hardlink("/home/quangminh/HarryPotter/Ravenclaw/file3.txt", "/home/quangminh/HarryPotter/hardlink.txt");

    return 0;
}