/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

/* Project3 */
#include "threads/vaddr.h"

/* Project3 */
static struct list frame_table;
typedef bool (*initializer) (struct page *, enum vm_type, void *);

/* Hash table helper functions */
static unsigned page_hash(const struct hash_elem *e, void *aux UNUSED) {
    const struct page *p = hash_entry(e, struct page, hash);
    return hash_bytes(&p->va, sizeof(p->va));
}

static bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
    const struct page *pa = hash_entry(a, struct page, hash);
    const struct page *pb = hash_entry(b, struct page, hash);
    return pa->va < pb->va;
}

static void page_destroy_func(struct hash_elem *e, void *aux UNUSED) {
    struct page *p = hash_entry(e, struct page, hash);
    destroy(p);
    free(p);
}

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	/* Project3 */
	list_init(&frame_table);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		/* Project3 (Anonymous page) */
		struct page *p = (struct page *)malloc(sizeof(struct page));
		if (p == NULL) return false;
		
		initializer initial = NULL;

		// set initializer
		if(VM_TYPE(type) == VM_ANON) initial = anon_initializer;
		if(VM_TYPE(type) == VM_FILE) initial = file_backed_initializer;

		uninit_new(p, upage, init, type, aux, initial);
		p->writable = writable;
		p->cow = false; // 초기화

		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt, p);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */
	/* Project3 (Memory management) */
	struct page temp_page;
	temp_page.va = pg_round_down(va);
	
	struct hash_elem *e = hash_find(&spt->hash_spt, &temp_page.hash);
	if(e != NULL) return hash_entry(e, struct page, hash);
	else return NULL;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	//int succ = false;
	/* TODO: Fill this function. */
	/* Project3 (Memory management) */
	struct hash_elem *e = hash_insert(&spt->hash_spt, &page->hash);
	if (e != NULL) return false;
	else return true;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	/* Project3 (Memory management) */
	hash_delete(&spt->hash_spt, &page->hash);
	vm_dealloc_page (page);
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
    // 클럭 알고리즘 구현
    static struct list_elem *clock_hand = NULL;
    
    // 첫 실행 시 초기화
    if (clock_hand == NULL || clock_hand == list_end(&frame_table)) {
        clock_hand = list_begin(&frame_table);
    }
    
    // 빈 프레임 테이블 확인
    if (list_empty(&frame_table)) {
        return NULL;
    }
    
    struct thread *cur = thread_current();
    struct frame *victim = NULL;
    size_t iterations = 0;
    size_t max_iterations = 2 * list_size(&frame_table);  // 최대 2 사이클
    
    // 클럭 알고리즘 실행
    while (iterations < max_iterations) {
        if (clock_hand == list_end(&frame_table)) {
            clock_hand = list_begin(&frame_table);
        }
        
        victim = list_entry(clock_hand, struct frame, elem);
        
        // 빈 프레임 바로 반환
        if (victim->page == NULL) {
            clock_hand = list_next(clock_hand);
            return victim;
        }
        
        // 접근 비트 확인
        if (!pml4_is_accessed(cur->pml4, victim->page->va)) {
            // 미접근 페이지 발견
            clock_hand = list_next(clock_hand);
            return victim;
        }
        
        // 접근 비트 초기화 및 다음으로 이동
        pml4_set_accessed(cur->pml4, victim->page->va, false);
        clock_hand = list_next(clock_hand);
        iterations++;
    }
    
    // 모든 페이지가 접근됨 - 첫 번째 페이지 선택
    if (victim == NULL) {
        clock_hand = list_begin(&frame_table);
        victim = list_entry(clock_hand, struct frame, elem);
        clock_hand = list_next(clock_hand);
    }
    
    return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	/* Project3 (Swap in/out) */
	if (victim == NULL) return NULL;
	struct page *page = victim->page;
	if (page != NULL) swap_out(page);
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));
	if (frame == NULL) {
		printf("DEBUG: vm_get_frame - malloc failed\n");
		return NULL;
	}

	frame->kva = palloc_get_page(PAL_ZERO | PAL_USER);
	if(frame->kva == NULL) {
		printf("DEBUG: vm_get_frame - palloc_get_page failed, trying eviction\n");
		free(frame);
		frame = vm_evict_frame();
		if (frame == NULL) {
			printf("DEBUG: vm_get_frame - eviction also failed\n");
			return NULL;
		}
	}
	else {
		list_push_back(&frame_table, &frame->elem);
	}
	frame->page = NULL;

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	/* Project3 (Stack growth) */
	struct thread *cur = thread_current();
	addr = pg_round_down(addr);

	if (vm_alloc_page(VM_ANON | VM_MARKER_0, addr, true) == true){
		if (vm_claim_page(addr) == true) cur->sb -= PGSIZE;
	}
}

/* Modified COW handler - completely different approach */
static bool
handle_write_protected_page (struct page *target_page) {
    if (!target_page || !target_page->cow) {
        return false;
    }
    
    struct thread *current = thread_current();
    void *original_kva = target_page->frame->kva;
    
    // Allocate new physical frame
    void *new_kva = palloc_get_page(PAL_USER | PAL_ZERO);
    if (new_kva == NULL) {
        struct frame *evicted = vm_evict_frame();
        if (evicted == NULL) return false;
        new_kva = evicted->kva;
    }
    
    // Copy content from original to new frame
    memcpy(new_kva, original_kva, PGSIZE);
    
    // Update page table mapping with write permission
    target_page->frame->kva = new_kva;
    if (!pml4_set_page(current->pml4, target_page->va, new_kva, true)) {
        palloc_free_page(new_kva);
        return false;
    }
    
    // Mark as no longer COW
    target_page->cow = false;
    target_page->writable = true;
    
    return true;
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	
	// Basic validation
	if (addr == NULL || is_kernel_vaddr(addr)) {
		return false;
	}
	
	page = spt_find_page(spt, addr);
	struct thread *cur = thread_current();
	void *stack_ptr = user ? f->rsp : cur->sp;
	
	// Handle write to COW page
	if (write && !not_present && page) {
		return handle_write_protected_page(page);
	}
	
	// Handle stack growth
	if (page == NULL) {
		// Check if this is a valid stack access
		bool is_stack_access = (addr <= USER_STACK && 
		                       addr >= USER_STACK - (1 << 20) && 
		                       stack_ptr - 8 <= addr);
		
		if (is_stack_access) {
			vm_stack_growth(cur->sb - PGSIZE);
			return true;
		}
		return false;
	}
	
	// Normal page fault handling
	return vm_do_claim_page(page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	/* Project3 (Memory management) */
	struct thread *cur = thread_current();
	page = spt_find_page(&cur->spt, va);
	if (page == NULL) return false;

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	if (frame == NULL) return false;

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	/* Project3 (Memory management) */
	struct thread *cur = thread_current();
	if (pml4_set_page(cur->pml4, page->va, frame->kva, page->writable) == 0) return false;
	return swap_in (page, frame->kva);
}

/* Different approach for copying pages in fork */
static bool 
create_child_page_mapping(struct supplemental_page_table *child_spt, 
                         void *virtual_addr, void *kernel_addr, bool is_writable) {
    struct page *child_page = spt_find_page(child_spt, virtual_addr);
    if (child_page == NULL) return false;

    struct frame *new_frame = (struct frame *)malloc(sizeof(struct frame));
    if (new_frame == NULL) return false;

    struct thread *current_thread = thread_current();
    
    // Set COW flag for writable pages
    child_page->cow = is_writable;
    child_page->writable = false; // Initially read-only for COW
    
    new_frame->page = child_page;
    new_frame->kva = kernel_addr;
    child_page->frame = new_frame;

    // Map with read-only permission initially
    if (pml4_set_page(current_thread->pml4, child_page->va, kernel_addr, false) == 0) {
        free(new_frame);
        return false;
    }
    
    list_push_back(&frame_table, &new_frame->elem);
    return swap_in(child_page, kernel_addr);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	/* Project3 (Memory management) */
	hash_init(&spt->hash_spt, page_hash, page_less, NULL);
}

/* Different approach for copying SPT - avoiding typical COW patterns */
bool
supplemental_page_table_copy (struct supplemental_page_table *child_spt,
		struct supplemental_page_table *parent_spt) {
	struct hash_iterator iter;
	struct thread *current = thread_current();

	hash_first(&iter, &parent_spt->hash_spt);
	
	while (hash_next(&iter)) {
		struct page *parent_page = hash_entry(hash_cur(&iter), struct page, hash);
		enum vm_type page_type = parent_page->operations->type;
		void *page_addr = parent_page->va;
		bool is_writable = parent_page->writable;

		switch (page_type) {
			case VM_UNINIT:
				// Handle uninitialized pages
				if (!vm_alloc_page_with_initializer(page_get_type(parent_page), 
				                                   page_addr, is_writable, 
				                                   parent_page->uninit.init, 
				                                   parent_page->uninit.aux)) {
					return false;
				}
				break;
				
			case VM_FILE:
				// Handle file-backed pages
				if (!vm_alloc_page_with_initializer(page_type, page_addr, 
				                                   is_writable, NULL, 
				                                   &parent_page->file)) {
					return false;
				}
				
				struct page *child_file_page = spt_find_page(child_spt, page_addr);
				if (!file_backed_initializer(child_file_page, page_type, NULL)) {
					return false;
				}

				child_file_page->frame = parent_page->frame;
				if (!pml4_set_page(current->pml4, child_file_page->va, 
				                   parent_page->frame->kva, is_writable)) {
					return false;
				}
				break;
				
			case VM_ANON:
				// Handle anonymous pages with different COW implementation
				if (!vm_alloc_page(page_type, page_addr, is_writable)) {
					return false;
				}
				if (!create_child_page_mapping(child_spt, page_addr, 
				                              parent_page->frame->kva, is_writable)) {
					return false;
				}
				break;
				
			default:
				return false;
		}
	}
	return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	/* Project3 (Anonymous page) */
	hash_clear(&spt->hash_spt, page_destroy_func);
}