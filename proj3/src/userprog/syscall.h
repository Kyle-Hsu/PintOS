#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

void check_valid_address(const void * addr, void * esp);
void halt(void );
void exit(int status);
int write(int fd, const void* buffer, unsigned size);
void acquire_lock(void);
void release_lock(void);

#endif /* userprog/syscall.h */
