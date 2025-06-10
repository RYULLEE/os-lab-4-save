#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdbool.h>
#include "threads/thread.h"

void syscall_init (void);
struct lock filesys_lock;

/* Project2 (System Call) */    //system call functions
void halt(void);
void exit(int status);
tid_t fork(const char *thread_name, struct intr_frame *f);
int exec(const char *cmd_line);
int wait(tid_t pid);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char* file);
int filesize(int fd);
int read(int fd, void* buffer, unsigned size);
int write(int fd, const void* buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);

#ifndef VM
/* Project2 (Helper function) */
void check_address(void *addr); //check whether the address is valid
#else
/* Project3 (Memory mapped files) */
struct page *check_address(void *addr);
#endif

/* Project3 (Memory mapped files) */
void *mmap(void *addr, size_t length, int writable, int fd, off_t offset);
void munmap(void *addr);
#endif /* userprog/syscall.h */

// project 4 (new system calls)
bool chdir (const char *dir);
bool mkdir (const char *dir);
bool readdir (int fd, char *name);
bool isdir (int fd);
int inumber (int fd);
int symlink (const char *target, const char *linkpath);