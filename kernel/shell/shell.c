#include "lib/kcall.h"
#include "lib/string.h"
#include "shell_main/cmd.h"


void main(){
    char line[128];
    

    
    while(1) {
        getline(line, sizeof(line));
        parse_and_execute(line);
   
    }

    return 0;
}