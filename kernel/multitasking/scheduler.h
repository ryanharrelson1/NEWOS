#ifndef SCHEDULER_H
#define SCHEDULER_H
#include <stdint.h>
#include <stddef.h>
#include "proccess.h"

extern process_t *current_process;
extern process_t* process_list;


void scheduler_tick(uintptr_t* stack_frame);
void scheduler_start() ;
void add_process(process_t* proc);

#endif // SCHEDULER_H