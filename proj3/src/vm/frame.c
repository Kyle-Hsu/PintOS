#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "vm/frame.h"
#include "userprog/pagedir.h"


void frame_init(void){
	list_init(&frame_table);
	lock_init(&frame_table_lock);
}

void* frame_palloc(enum palloc_flags flags, struct spt_entry* s){
	void* frame = palloc_get_page(flags);
	if(!frame){
		//printf("no frame to alloc\n");
		return frame;
	}

	struct frame_table_entry *f = malloc(sizeof(struct frame_table_entry));
	f->frame = frame;
	f->owner = thread_current();
	f->sup_entry = s;

	lock_acquire(&frame_table_lock);
	list_push_back(&frame_table, &f->frame_elem);
	lock_release(&frame_table_lock);
	return frame;
}

void evict_frame(void){
	//printf("%s wants to acquire frame table lock\n", thread_current()->name );
	//hex_dump(*esp, *esp, 200, true);
	lock_acquire(&frame_table_lock);
	//printf("size of ft %d\n", list_size(&frame_table));
	struct list_elem* iter = list_begin(&frame_table);
	while(1){
		struct frame_table_entry *f = list_entry(iter, struct frame_table_entry, frame_elem);
		struct thread* t = f->owner;
		struct spt_entry* s = f->sup_entry;

		bool accessed = pagedir_is_accessed(t->pagedir, s->upage);
		if(!s->pinned){
			if(accessed){
				pagedir_set_accessed(t->pagedir, s->upage, false);
			} else {		
				s->swap_index = swap_out(f->frame);
				list_remove(iter);
				pagedir_clear_page(t->pagedir, s->upage);
				lock_release(&frame_table_lock);
				frame_free(f->frame);
				
				free(f);
				
				break;
			}
		}

		if(list_next(iter) == list_end(&frame_table)){
			iter = list_begin(&frame_table);
		} else {
			iter = list_next(iter);
		}
	}

	if(lock_held_by_current_thread(&frame_table_lock)){
		//printf("%s will release frame table lock\n", thread_current()->name );
		lock_release(&frame_table_lock);
	}
}


void frame_free (void *frame){
	lock_acquire(&frame_table_lock);
	for (struct list_elem* iter = list_begin(&frame_table); iter != list_end(&frame_table);
		iter = list_next(iter)){
		struct frame_table_entry *f = list_entry(iter, struct frame_table_entry, frame_elem);
		if (f->frame == frame){
			list_remove(iter);
			free(f);
			break;
		}
	}
	lock_release(&frame_table_lock);
	palloc_free_page(frame);
}
