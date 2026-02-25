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
    if(strcmp(args[0], "echo") == 0) {
        cmd_echo(args);
    } else if(strcmp(args[0], "help") == 0) {
        cmd_help(args);
    } else {
        printf("Unknown command: %s\n", args[0]);
    }
}