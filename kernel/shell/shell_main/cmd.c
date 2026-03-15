#include "cmd.h"
#include "../lib/string.h"



// Built-in functions
static void cmd_echo(char** args) {
    int i = 1;
    while(args[i]) {
        printf("%s ", args[i]);
        i++;
    }
    printf("\n");
}

static void cmd_help(char** args) {
    (void)args;
    printf("Built-in commands:\n");
    printf("  echo - print arguments\n");
    printf("  help - show this message\n");
}

static void cmd_ls(char** args) {
    const char* path = args[1] ? args[1] : "/";

    int r = list(path);
    if (r < 0) {
        printf("ls failed\n");
    }
}

static void cmd_cat(char** args) {
    if (!args[1]) {
        write(1, "usage: cat /PATH\n", 17);
        return;
    }

    int fd = open(args[1], 0);
    if (fd < 0) {
        write(1, "cat: open failed\n", 17);
        return;
    }

    char buf[64];
    int n;
    int count = 0;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        write(1, buf, n);
        count++;
        if (count > 16) { // safety to prevent infinite loop on large files
            write(1, "\ncat: file too large\n", 22);
            break;
        }
    }

    close(fd);
    write(1, "\n", 1);
}

// Simple command dispatcher
void parse_and_execute(char* line) {
    if(!line) return;

    // tokenize input
    char* args[16] = {0};
    int argc = 0;
    char* token = strtok(line, " \t\n");
    while(token && argc < 15) {
        args[argc++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[argc] = NULL;

    if(argc == 0) return; // empty line

    // dispatch built-ins
    if (strcmp(args[0], "echo") == 0) {
        cmd_echo(args);
    } else if (strcmp(args[0], "help") == 0) {
        cmd_help(args);
    } else if (strcmp(args[0], "ls") == 0) {
        cmd_ls(args);
    } else if (strcmp(args[0], "cat") == 0) {
        cmd_cat(args);
    } else {
        printf("Unknown command: %s\n", args[0]);
    }
}