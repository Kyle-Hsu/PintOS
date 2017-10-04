#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/thread.h"
#include <kernel/list.h>
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "vm/page.h"
#include "vm/swap.h"

struct list frame_table;
struct lock frame_table_lock;

struct frame_table_entry {
	uint32_t* frame;
	struct thread* owner;
	struct sup_page_entry* sup_entry;
	struct list_elem frame_elem;

};



void frame_init(void);
void* frame_palloc(enum palloc_flags flags, struct spt_entry *s);
void frame_free (void *frame);
void evict_frame(void);


#endif /* vm/frame.h */
