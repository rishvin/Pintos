#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

void process_init(tid_t ptid);
void process_notify(int status);
tid_t process_execute (const char *file_name);
tid_t process_execute_sync(const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
