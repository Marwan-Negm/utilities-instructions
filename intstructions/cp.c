#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define COPY_BUFFER_SIZE 65536

static void print_error(const char *action, const char *path)
{
    fprintf(stderr, "cp: cannot %s '%s': %s\n", action, path, strerror(errno));
}

static int copy_data(int source_fd, int destination_fd)
{
    char buffer[COPY_BUFFER_SIZE];

    for (;;) {
        ssize_t bytes_read = read(source_fd, buffer, sizeof(buffer));

        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (bytes_read == 0) {
            return 0;
        }

        ssize_t total_written = 0;
        while (total_written < bytes_read) {
            ssize_t bytes_written = write(destination_fd,
                                          buffer + total_written,
                                          (size_t)(bytes_read - total_written));

            if (bytes_written < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return -1;
            }
            if (bytes_written == 0) {
                errno = EIO;
                return -1;
            }
            total_written += bytes_written;
        }
    }
}

int main(int argc, char *argv[])
{
    int source_fd;
    int destination_fd;
    struct stat source_status;
    struct stat destination_status;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s SOURCE DESTINATION\n", argv[0]);
        return EXIT_FAILURE;
    }

    source_fd = open(argv[1], O_RDONLY);
    if (source_fd < 0) {
        print_error("open", argv[1]);
        return EXIT_FAILURE;
    }

    if (fstat(source_fd, &source_status) < 0) {
        print_error("inspect", argv[1]);
        if (close(source_fd) < 0) {
            print_error("close", argv[1]);
        }
        return EXIT_FAILURE;
    }
    if (!S_ISREG(source_status.st_mode)) {
        fprintf(stderr, "cp: '%s' is not a regular file\n", argv[1]);
        if (close(source_fd) < 0) {
            print_error("close", argv[1]);
        }
        return EXIT_FAILURE;
    }

    destination_fd = open(argv[2], O_WRONLY | O_CREAT,
                          source_status.st_mode & 0777);
    if (destination_fd < 0) {
        print_error("open", argv[2]);
        if (close(source_fd) < 0) {
            print_error("close", argv[1]);
        }
        return EXIT_FAILURE;
    }

    if (fstat(destination_fd, &destination_status) < 0) {
        print_error("inspect", argv[2]);
        (void)close(destination_fd);
        (void)close(source_fd);
        return EXIT_FAILURE;
    }
    if (!S_ISREG(destination_status.st_mode)) {
        fprintf(stderr, "cp: '%s' is not a regular file\n", argv[2]);
        (void)close(destination_fd);
        (void)close(source_fd);
        return EXIT_FAILURE;
    }
    if (source_status.st_dev == destination_status.st_dev &&
        source_status.st_ino == destination_status.st_ino) {
        fprintf(stderr, "cp: '%s' and '%s' are the same file\n",
                argv[1], argv[2]);
        (void)close(destination_fd);
        (void)close(source_fd);
        return EXIT_FAILURE;
    }

    if (ftruncate(destination_fd, 0) < 0) {
        print_error("truncate", argv[2]);
        (void)close(destination_fd);
        (void)close(source_fd);
        return EXIT_FAILURE;
    }

    if (copy_data(source_fd, destination_fd) < 0) {
        int saved_errno = errno;

        (void)close(destination_fd);
        (void)close(source_fd);
        errno = saved_errno;
        print_error("copy", argv[2]);
        return EXIT_FAILURE;
    }

    if (close(destination_fd) < 0) {
        print_error("close", argv[2]);
        (void)close(source_fd);
        return EXIT_FAILURE;
    }
    if (close(source_fd) < 0) {
        print_error("close", argv[1]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
