#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

/* Project2 (Argument Passing) */
void argument_stack(char **argv, int argc, struct intr_frame *if_);

/* Project2 (System Call) */
int add_file(struct file *f);
void close_file(int fd);
struct file *get_file(int fd);
struct thread *get_child(tid_t tid);

/* Project3 (Anonymous page) */
// struct containing information for loading binary 
struct aux{
    struct file *file;
    size_t page_read_bytes;
    off_t offset;
};

/* Project3 (Anonymous page) */
bool lazy_load_segment(struct page *page, void *aux);

#endif /* userprog/process.h */