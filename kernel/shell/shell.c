#include "lib/kcall.h"
#include "lib/string.h"
#include "shell_main/cmd.h"


void main(){
    char line[128];
    
   volatile  int pid = fork();

    if (pid < 0) {
        printf("fork failed\n");
        return;
    } else if (pid == 0) {
        printf("child\n");
        while (1) { }
    } else {
        printf("parent pid=%d\n", pid);
        while (1) { }
    }
    
    while(1) {
        getline(line, sizeof(line));
        parse_and_execute(line);
   
    }

    return 0;
}