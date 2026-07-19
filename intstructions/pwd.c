#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    size_t size = 128;
    char *buffer = NULL;

    for (;;) {
        char *larger_buffer = realloc(buffer, size);

        if (larger_buffer == NULL) {
            perror("pwd: realloc");
            free(buffer);
            return EXIT_FAILURE;
        }
        buffer = larger_buffer;

        if (getcwd(buffer, size) != NULL) {
            break;
        }
        if (errno != ERANGE) {
            perror("pwd: getcwd");
            free(buffer);
            return EXIT_FAILURE;
        }
        if (size > (size_t)-1 / 2) {
            fprintf(stderr, "pwd: working directory path is too long\n");
            free(buffer);
            return EXIT_FAILURE;
        }
        size *= 2;
    }

    if (printf("%s\n", buffer) < 0) {
        perror("pwd: stdout");
        free(buffer);
        return EXIT_FAILURE;
    }
    free(buffer);

    if (fclose(stdout) == EOF) {
        perror("pwd: stdout");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
