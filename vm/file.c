/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"

/* Project3 (Memory mapped files) */
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "threads/vaddr.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;

	/* Project3 (Memory mapped files) */
	struct aux *a = (struct aux *)page->uninit.aux;
	file_page->file = a->file;
	file_page->page_read_bytes = a->page_read_bytes;
	file_page->offset = a->offset;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
	/* Project3 (Swap in/out) */
	lock_acquire(&filesys_lock);
	bool res = lazy_load_segment(page, file_page);
    lock_release(&filesys_lock);
	return res;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page = &page->file;
	struct thread *cur = thread_current();

	if(pml4_is_dirty(cur->pml4, page->va)) {
		file_write_at(file_page->file, page->frame->kva, file_page->page_read_bytes, file_page->offset);
		pml4_set_dirty(cur->pml4, page->va, false);
	}
	page->frame->page = NULL;
	page->frame = NULL;
	pml4_clear_page(cur->pml4, page->va);
	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	/* Project3 (Memory mapped files) */
	struct thread *cur = thread_current();

	if(pml4_is_dirty(cur->pml4, page->va) == 1){
		file_write_at(file_page->file, page->va, file_page->page_read_bytes, file_page->offset);
		pml4_set_dirty(cur->pml4, page->va, false);
	}
	if (page->frame != NULL) {
		list_remove(&page->frame->elem);
		page->frame->page = NULL;
		free(page->frame);
		page->frame = NULL;

	}
    pml4_clear_page(cur->pml4, page->va);
}

/* Do the mmap */
/* Project3 (Memory mapped files) */
void *
do_mmap (void *addr, size_t length, int writable,
        struct file *file, off_t offset) {
    if (addr == NULL || file == NULL || length == 0) 
        return NULL;
        
    struct file *mm_file = file_reopen(file);
    if (mm_file == NULL) 
        return NULL;
        
    off_t len_file = file_length(mm_file);
    if (len_file == 0) {
        file_close(mm_file);
        return NULL;
    }
    
    size_t read_bytes = len_file < length ? len_file : length;
    size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;
    if (zero_bytes == PGSIZE)
        zero_bytes = 0;

    ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT(pg_ofs(addr) == 0);
    ASSERT(offset % PGSIZE == 0);

    void *start_addr = addr;
    size_t remaining_read_bytes = read_bytes;
    size_t remaining_zero_bytes = zero_bytes;
    off_t current_offset = offset;

    lock_acquire(&filesys_lock);
    
    while (remaining_read_bytes > 0 || remaining_zero_bytes > 0) {
        size_t page_read_bytes = remaining_read_bytes < PGSIZE ? remaining_read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        struct aux *a = (struct aux *)malloc(sizeof(struct aux));
        if (a == NULL) {
            file_close(mm_file);
            lock_release(&filesys_lock);
            return NULL;
        }
        
        a->file = mm_file;
        a->page_read_bytes = page_read_bytes;
        a->offset = current_offset;

        if (!vm_alloc_page_with_initializer(VM_FILE, addr,
                    writable, lazy_load_segment, a)) {
            free(a);
            file_close(mm_file);
            lock_release(&filesys_lock);
            return NULL;
        }

        remaining_read_bytes -= page_read_bytes;
        remaining_zero_bytes -= page_zero_bytes;
        addr += PGSIZE;
        current_offset += page_read_bytes;
    }
    
    lock_release(&filesys_lock);
    return start_addr;
}

/* Do the munmap */
/* Project3 (Memory mapped files) */
void
do_munmap (void *addr) {
	struct thread *cur = thread_current();
	struct page *page;

	lock_acquire(&filesys_lock);
	while (page = spt_find_page(&cur->spt, addr)) {
        if (page != NULL) destroy(page);
        addr += PGSIZE;
    }
    lock_release(&filesys_lock);
}