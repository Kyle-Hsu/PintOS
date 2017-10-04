#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filesys/off_t.h"
#include "threads/thread.h"
#include <kernel/list.h>
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include <kernel/hash.h>


struct spt_entry {
	//file = 1, stack = 2
	uint8_t type;

	bool lazyload;

	//used for lazy load
	struct file* file;
	off_t ofs;
	uint8_t *upage;
	uint32_t read_bytes;
	uint32_t zero_bytes;
	bool writable;

	//swap table index
	size_t swap_index;

	bool pinned;

	bool dirty;
	bool accessed;

	struct list_elem spt_elem;

	struct hash_elem hash_elem;
};

unsigned page_hash (const struct hash_elem *elem, void *aux UNUSED);
bool page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

//function for lazy load
bool add_to_sup_table(struct file *file, off_t ofs, uint8_t *upage,
	uint32_t read_bytes, uint32_t zero_bytes, bool writable);

bool load_lazy(const void* addr);
struct spt_entry* look_for_entry(const void *addr);

bool grow_stack(const void * addr);
#endif /* vm/page.h */
