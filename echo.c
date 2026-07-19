#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int write_all(int fd, const char *text, size_t length)
{
    size_t written = 0;

    while (written < length) {
        ssize_t result = write(fd, text + written, length - written);

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (result == 0) {
            errno = EIO;
            return -1;
        }
        written += (size_t)result;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int first_argument = 1;
    int print_newline = 1;

    while (first_argument < argc &&
           strcmp(argv[first_argument], "-n") == 0) {
        print_newline = 0;
        ++first_argument;
    }

    for (int i = first_argument; i < argc; ++i) {
        if (i > first_argument && write_all(STDOUT_FILENO, " ", 1) < 0) {
            perror("echo: write");
            return EXIT_FAILURE;
        }
        if (write_all(STDOUT_FILENO, argv[i], strlen(argv[i])) < 0) {
            perror("echo: write");
            return EXIT_FAILURE;
        }
    }

    if (print_newline && write_all(STDOUT_FILENO, "\n", 1) < 0) {
        perror("echo: write");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
