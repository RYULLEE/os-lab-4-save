#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

/* Project2 (System Call) */
#include <string.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/mmu.h"
#include "threads/palloc.h"
#include "userprog/process.h"
#include "kernel/stdio.h"

// project 4
#include "filesys/directory.h"
#include "filesys/inode.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	/* Project2 (System Call) */
	lock_init(&filesys_lock);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	//printf ("system call!\n");
	//thread_exit ();

    /* Project3 (Memory mapped files) */
    struct thread *t = thread_current();
    t->sp = f->rsp;

    /* Project2 (System Call) */
    switch (f->R.rax) {
        case SYS_HALT:
            halt();
            break;
        case SYS_EXIT:
            exit(f->R.rdi);
            break;
        case SYS_FORK:
            f->R.rax = fork(f->R.rdi, f);
            break;
        case SYS_EXEC:
            f->R.rax = exec(f->R.rdi);
            break;
        case SYS_WAIT:
            f->R.rax = wait(f->R.rdi);
            break;
        case SYS_CREATE:
            f->R.rax = create(f->R.rdi, f->R.rsi);
            break;
        case SYS_REMOVE:
            f->R.rax = remove(f->R.rdi);
            break;
        case SYS_OPEN:
            f->R.rax = open(f->R.rdi);
            break;
        case SYS_FILESIZE:
            f->R.rax = filesize(f->R.rdi);
            break;
        case SYS_READ:
            f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
            break;
        case SYS_WRITE:
            f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
            break;
        case SYS_SEEK:
            seek(f->R.rdi, f->R.rsi);
            break;
        case SYS_TELL:
            f->R.rax = tell(f->R.rdi);
            break;
        case SYS_CLOSE:
            close(f->R.rdi);
            break;
        /* Project3 (Memory mapped files) */
        case SYS_MMAP:
            f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
            break;
        case SYS_MUNMAP:
            munmap(f->R.rdi);
            break;
        // project 4 (new system calls)
        case SYS_CHDIR:
            f->R.rax = chdir(f->R.rdi);
            break;
        case SYS_MKDIR:
            f->R.rax = mkdir(f->R.rdi);
            break;
        case SYS_READDIR:
            f->R.rax = readdir(f->R.rdi, f->R.rsi);
            break;
        case SYS_ISDIR:
            f->R.rax = isdir(f->R.rdi);
            break;
        case SYS_INUMBER:
            f->R.rax = inumber(f->R.rdi);
            break;
        case SYS_SYMLINK:
            f->R.rax = symlink(f->R.rdi, f->R.rsi);
            break;
        default:
            exit(-1);
            break;
    }
}

/* Project2 (System Call) */
void halt(void){
    power_off();
}

/* Project2 (System Call) */
void exit(int status){
	struct thread *t = thread_current();
    t->exit_status = status;
	// Process Termination Message
    printf("%s: exit(%d)\n", t->name, t->exit_status);
    thread_exit();
}

/* Project2 (System Call) */
tid_t fork(const char *thread_name, struct intr_frame *f){
    check_address(thread_name);
    return process_fork(thread_name, f);
}

/* Project2 (System Call) */
int exec(const char *cmd_line){
    check_address(cmd_line);

    char *cmd_copy = palloc_get_page(0);
    if (cmd_copy == NULL) return -1;

    memcpy(cmd_copy, cmd_line, strlen(cmd_line) + 1);

    char *save;
	strtok_r(cmd_line, " ", &save);
    int result = process_exec(cmd_copy);
	if ( result == -1) exit(-1);
	NOT_REACHED();
}

/* Project2 (System Call) */
int wait(tid_t pid){
	return process_wait(pid);
}

/* Project2 (System Call) */
bool create(const char *file, unsigned initial_size) {
    check_address(file);
    
    // 락 획득 및 생성 시도
    lock_acquire(&filesys_lock);
    bool success = filesys_create(file, initial_size);
    lock_release(&filesys_lock);
    
    return success;
}

/* Project2 (System Call) */
bool remove(const char *file){
	check_address(file);
	return filesys_remove(file);
}

/* Project2 (System Call) */
int open(const char* file) {
    check_address(file);
    
    if(file == NULL) return -1;
    
    // 락 획득
    lock_acquire(&filesys_lock);
    struct file *new_file = filesys_open(file);
    
    if (new_file == NULL) {
        lock_release(&filesys_lock);
        return -1;
    }
    
    int file_descriptor = add_file(new_file);
    // When FDT is full
    if (file_descriptor == -1) {
        file_close(new_file);
    }
    
    lock_release(&filesys_lock);
    return file_descriptor;
}

/* Project2 (System Call) */
int filesize(int fd){
	struct file *file = get_file(fd);

    if (file == NULL) return -1;

    lock_acquire(&filesys_lock);
    int length = file_length(file);
    lock_release(&filesys_lock);

    return length;
}

/* Project2 (System Call) */
int read(int fd, void* buffer, unsigned size){
    int result;
    check_address(buffer);
    /* Project3 (Memory mapped files) */
    check_buffer(buffer, size, true);

    struct file* file = get_file(fd);
    if (file == NULL) return -1;

    if (fd == 1) result = -1;   //read from std output
    else if (fd == 0){          //read from std input
        int i;
        char c;
		char *buf = buffer;
		for (i = 0; i < size; i++) {
			lock_acquire(&filesys_lock);
			c = input_getc();
			lock_release(&filesys_lock);
			*buf++ = c;
			if (c == '\0') break;
		}
		result = i;
    } 
	else {                      //read from file
		lock_acquire(&filesys_lock);
		result = file_read(file, buffer,size);
		lock_release(&filesys_lock);
	}

	return result;
}

/* Project2 (System Call) */
int write(int fd, const void* buffer, unsigned size){
    int result;
    check_address(buffer);
    /* Project3 (Memory mapped files) */
    check_buffer(buffer, size, false);
    
    struct file *file = get_file(fd);
    if (file == NULL) return -1;

    if (fd == 0) result = -1;       //write at std input
    else if(fd == 1){               //write at std output
        lock_acquire(&filesys_lock);
		putbuf(buffer, size);
		lock_release(&filesys_lock);
		result = size;
    }
    else{                           //write at file
        lock_acquire(&filesys_lock);
        result = file_write(file, buffer, size);
		lock_release(&filesys_lock);
    }
    
    return result;
}

/* Project2 (System Call) */
void seek(int fd, unsigned position){
    if(fd <= 1) return;     //ignore seek request from stdin & stdout

    struct file *file = get_file(fd);
    if (file == NULL) return;

    lock_acquire(&filesys_lock);
    file_seek(file, position);
    lock_release(&filesys_lock);
}

/* Project2 (System Call) */
unsigned tell(int fd){
    if(fd <= 1) return;     //ignore tell request from stdin & stdout
    
    struct file *file = get_file(fd);
    if (file == NULL) return;

    lock_acquire(&filesys_lock);
    file_tell(file);
    lock_release(&filesys_lock);
}

/* Project2 (System Call) */
void close(int fd){
    if(fd <= 1) return;     //ignore close request from stdin & stdout

    lock_acquire(&filesys_lock);
    close_file(fd);
    lock_release(&filesys_lock);
}

/* Project2 (System Call) */
#ifndef VM
void check_address (void *addr){
    if (is_kernel_vaddr(addr) || addr == NULL || pml4_get_page(thread_current()->pml4, addr) == NULL){
        exit(-1);
    }
}
#else

/* Project3 (Memory mapped files) */
struct page *check_address(void *addr){
    if(addr == NULL) exit(-1);
    if(is_kernel_vaddr(addr)) exit(-1);
    struct thread *cur = thread_current();
    return spt_find_page(&cur->spt, addr);
}

/* Project3 (Memory mapped files) */
void check_buffer(void *buffer, size_t size, bool writable){
    for(size_t i = 0; i < size; i++){
        struct page *p = check_address(buffer + i);
        if(p == NULL) exit(-1);
        if(writable == true && p->writable == false) exit(-1);
    }
}
#endif

/* Project3 (Memory mapped files) */
void *mmap(void *addr, size_t length, int writable, int fd, off_t offset){
    // address / length / offset are not appropriate
    if (addr == NULL || pg_round_down(addr) != addr || length <= 0) return NULL;
    
    // 커널 주소 영역 체크 - 이 부분이 핵심!
    if (is_kernel_vaddr(addr)) return NULL;
    if (is_kernel_vaddr(addr + length - 1)) return NULL;  // 끝 주소도 체크
    
    if (offset != pg_round_down(offset) || offset % PGSIZE != 0) return NULL;

    // file is not appropriate
    struct file *file = get_file(fd);
    if(file == 0 || file == 1 || file == NULL) return NULL;

    // page already exists for the address
    struct thread *cur = thread_current();
    if (spt_find_page(&cur->spt, addr)) return NULL;

    return do_mmap(addr, length, writable, file, offset);
}

/* Project3 (Memory mapped files) */
void munmap(void *addr){
    do_munmap(addr);
}

// project 4 (new system calls)
bool chdir (const char *dir){
    return filesys_chdir(dir);
}

bool mkdir (const char *dir){
    return filesys_mkdir(dir);
}

bool readdir (int fd, char *name){
    struct file *file = get_file(fd);

    if (file == NULL) return false;
    if (inode_type(file->inode) != 0) return false; // it should be directory type

    return dir_readdir(file, name);
}

bool isdir (int fd){
    struct file *file = get_file(fd);

    if (file == NULL) return false;
    if (inode_type(file->inode) != 0) return false;
    else return true;
}

int inumber (int fd){
    struct file *file = get_file(fd);
    return inode_get_inumber(file->inode);
}

int symlink (const char *target, const char *linkpath){
    if (filesys_symlink(target, linkpath)) return 0;
    else return -1;
}