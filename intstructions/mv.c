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
    fprintf(stderr, "mv: cannot %s '%s': %s\n", action, path, strerror(errno));
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

static int copy_across_filesystems(const char *source, const char *destination)
{
    int source_fd = -1;
    int temporary_fd = -1;
    int result = -1;
    struct stat source_status;
    size_t template_length;
    char *temporary_path = NULL;

    source_fd = open(source, O_RDONLY);
    if (source_fd < 0) {
        print_error("open", source);
        goto cleanup;
    }
    if (fstat(source_fd, &source_status) < 0) {
        print_error("inspect", source);
        goto cleanup;
    }
    if (!S_ISREG(source_status.st_mode)) {
        fprintf(stderr, "mv: '%s' is not a regular file\n", source);
        goto cleanup;
    }

    if (strlen(destination) > (size_t)-1 - sizeof(".tmp.XXXXXX")) {
        fprintf(stderr, "mv: destination path is too long\n");
        goto cleanup;
    }
    template_length = strlen(destination) + sizeof(".tmp.XXXXXX");
    temporary_path = malloc(template_length);
    if (temporary_path == NULL) {
        perror("mv: malloc");
        goto cleanup;
    }
    if (snprintf(temporary_path, template_length, "%s.tmp.XXXXXX",
                 destination) < 0) {
        fprintf(stderr, "mv: could not construct temporary path\n");
        goto cleanup;
    }

    temporary_fd = mkstemp(temporary_path);
    if (temporary_fd < 0) {
        print_error("create temporary file for", destination);
        goto cleanup;
    }

    if (copy_data(source_fd, temporary_fd) < 0) {
        print_error("copy to", destination);
        goto cleanup;
    }
    if (fchmod(temporary_fd, source_status.st_mode & 07777) < 0) {
        print_error("set permissions on", destination);
        goto cleanup;
    }
    {
        const struct timespec times[2] = {
            source_status.st_atim,
            source_status.st_mtim
        };

        if (futimens(temporary_fd, times) < 0) {
            print_error("preserve timestamps on", destination);
            goto cleanup;
        }
    }
    if (fsync(temporary_fd) < 0) {
        print_error("sync", destination);
        goto cleanup;
    }
    if (close(temporary_fd) < 0) {
        temporary_fd = -1;
        print_error("close", destination);
        goto cleanup;
    }
    temporary_fd = -1;

    if (close(source_fd) < 0) {
        source_fd = -1;
        print_error("close", source);
        goto cleanup;
    }
    source_fd = -1;

    if (rename(temporary_path, destination) < 0) {
        print_error("replace", destination);
        goto cleanup;
    }

    if (unlink(source) < 0) {
        print_error("remove", source);
        goto cleanup;
    }

    result = 0;

cleanup:
    if (temporary_fd >= 0 && close(temporary_fd) < 0 && result == 0) {
        print_error("close", temporary_path);
        result = -1;
    }
    if (source_fd >= 0 && close(source_fd) < 0 && result == 0) {
        print_error("close", source);
        result = -1;
    }
    if (result != 0 && temporary_path != NULL &&
        unlink(temporary_path) < 0 && errno != ENOENT) {
        print_error("remove temporary file", temporary_path);
    }
    free(temporary_path);
    return result;
}

int main(int argc, char *argv[])
{
    struct stat source_status;
    struct stat destination_status;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s SOURCE DESTINATION\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (lstat(argv[1], &source_status) < 0) {
        print_error("inspect", argv[1]);
        return EXIT_FAILURE;
    }
    if (!S_ISREG(source_status.st_mode)) {
        fprintf(stderr, "mv: '%s' is not a regular file\n", argv[1]);
        return EXIT_FAILURE;
    }

    if (lstat(argv[2], &destination_status) == 0) {
        if (source_status.st_dev == destination_status.st_dev &&
            source_status.st_ino == destination_status.st_ino) {
            fprintf(stderr, "mv: '%s' and '%s' are the same file\n",
                    argv[1], argv[2]);
            return EXIT_FAILURE;
        }
    } else if (errno != ENOENT) {
        print_error("inspect", argv[2]);
        return EXIT_FAILURE;
    }

    if (rename(argv[1], argv[2]) == 0) {
        return EXIT_SUCCESS;
    }
    if (errno != EXDEV) {
        print_error("move to", argv[2]);
        return EXIT_FAILURE;
    }

    if (copy_across_filesystems(argv[1], argv[2]) < 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
