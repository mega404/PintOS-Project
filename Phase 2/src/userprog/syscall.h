#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdio.h>

void syscall_init(void);
bool valid(void*);
void halt();
int wait(int);
int write(int, void*, int);
int create(char*, unsigned);
int open(char*);
void exit(int);
bool remove(char*);
int exec(char*);
int close(int);
int read(int, void*, unsigned);
int size(int);
int seek(int, unsigned);
int tell(int);

#endif /* userprog/syscall.h */
