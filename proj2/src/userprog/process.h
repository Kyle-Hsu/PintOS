#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include <kernel/list.h>
#include "threads/synch.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct open_file {
	struct list_elem file_elem;		/* To be put in to a list of open files*/
	struct file* file_pointer;		/* Pointer to the file */
	int fd;							/* File descriptor */
};

struct child_status{
	struct list_elem cs_elem;
	int exit_status; 
	int pid;
	bool has_waited;
	char* name;
	struct semaphore sema;
	struct thread* curr_thread;
};


#endif /* userprog/process.h */
