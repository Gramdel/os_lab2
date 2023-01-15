#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "kmod.h"

#define BUF_SIZE 1024

void print_usage_err(const char* filename) {
    fprintf(stderr, "Usage: %s [-pd] <PID>\n", filename);
    exit(-1);
}

int main(int argc, char* argv[]) {
    printf("### GramDEL's OS LAB2 ###\n");
    int flags = 0;

    // parsing options:
    int opt;
    while ((opt = getopt(argc, argv, ":pd")) != -1) {
        switch (opt) {
            case 'p':
                flags |= PRINT_PAGE_INFO;
                break;
            case 'd':
                flags |= PRINT_DENTRY_INFO;
                break;
            case '?':
            default:
                print_usage_err(argv[0]);
        }
    }

    // check if number of args is right:
    if (optind == 1 || optind != argc - 1) {
        print_usage_err(argv[0]);
    }

    // try to parse PID:
    int pid = (int) strtol(argv[optind], NULL, 10);
    if (errno == ERANGE || !pid) {
        fprintf(stderr, "PID must be a positive int number!\n");
        exit(-1);
    }

    // opening file in debugfs:
    printf("Opening \"" DEBUGFS_FILE_PATH "\"...\n");
    FILE* file = fopen(DEBUGFS_FILE_PATH, "r+");
    if (!file) {
        fprintf(stderr, "Couldn't open the file!\n");
        exit(-1);
    }

    // writing flags and pid:
    printf("Sending PID=%d to kernel module...\n", pid);
    if (!fwrite(&pid, sizeof(int), 1, file) || !fwrite(&flags, sizeof(int), 1, file)) {
        fprintf(stderr, "Couldn't write to file in debugfs!\n");
        exit(-1);
    }
    fflush(file);

    // checking if everything is fine:
    if (errno == EINVAL) {
        fprintf(stderr, "Invalid argument! Does process with such PID exist?\n");
        exit(-1);
    } else if (errno == EFAULT) {
        fprintf(stderr, "Internal kernel error occurred!\n");
        exit(-1);
    }

    // reading info:
    char buffer[BUF_SIZE];
    char* line;
    do {
        line = fgets(buffer, BUF_SIZE, file);
        if (line) {
            printf("%s", line);
        }
    } while (line);
    fclose(file);

    return 0;
}