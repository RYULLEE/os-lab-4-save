/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

/* Project3 */
#include <bitmap.h>
#include "threads/mmu.h"

/* Project3 (Swap in/out) */
#define SLOTSIZE PGSIZE / DISK_SECTOR_SIZE

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* Project3 (Swap in/out) */
struct bitmap *swap_info;

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */

static struct bitmap *swap_table;
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	/* Project3 (Swap in/out) */
	swap_disk = disk_get(1,1);
	if (swap_disk != NULL) {
		swap_info = bitmap_create(disk_size(swap_disk) / SLOTSIZE);
	}
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	/* Project3 (Swap in/out) */
	memset(&page->uninit, 0, sizeof(struct uninit_page));
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	anon_page->slot = BITMAP_ERROR;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	/* Project3 (Swap in/out) */
	size_t slot = anon_page->slot;
	size_t sector = slot * SLOTSIZE;
	size_t i = 0;

	if(slot == BITMAP_ERROR) return false;
	if (bitmap_test(swap_info, slot) == false) return false;

	while(i < SLOTSIZE){
		disk_read(swap_disk, sector + i, kva + DISK_SECTOR_SIZE * i);
		i += 1;
	}
	bitmap_set(swap_info, slot, false);
	anon_page->slot = BITMAP_ERROR;
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	/* Project3 (Swap in/out) */
	if (swap_disk == NULL || swap_info == NULL) {
		return false;
	}
	
	size_t free_slot = bitmap_scan_and_flip(swap_info, 0, 1, false);
	if(free_slot == BITMAP_ERROR) return false;  // PANIC 제거하고 false 반환
	
	size_t sector = free_slot * SLOTSIZE;
	size_t i = 0;
	struct thread *cur = thread_current();

	while(i < SLOTSIZE){
		disk_write(swap_disk, sector + i, page->frame->kva + DISK_SECTOR_SIZE * i);
		i += 1;
	}

	anon_page->slot = free_slot;
	page->frame->page = NULL;
	page->frame = NULL;
	pml4_clear_page(cur->pml4, page->va);
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	
	/* Project3 (Anonymous page) */
	struct thread *cur = thread_current();
	if(anon_page->slot != BITMAP_ERROR){
		bitmap_reset(swap_info, anon_page->slot);
	}
	if(page->frame != NULL){
		list_remove(&page->frame->elem);
		page->frame->page = NULL;
		free(page->frame);
		page->frame = NULL;
	}
	pml4_clear_page(cur->pml4, page->va);
}