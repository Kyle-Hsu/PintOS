#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <round.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"
#include "userprog/process.h"
#include "vm/frame.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/pagedir.h"

unsigned page_hash (const struct hash_elem *elem, void *aux UNUSED){
	const struct spt_entry* spt = hash_entry(elem, struct spt_entry, hash_elem);
	return hash_bytes (&spt->upage, sizeof spt->upage);
}

bool page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
	const struct spt_entry *spta = hash_entry (a, struct spt_entry, hash_elem);
	const struct spt_entry *sptb = hash_entry (b, struct spt_entry, hash_elem);
	return spta->upage < sptb->upage;
}


bool add_to_sup_table(struct file *file, off_t ofs, uint8_t *upage,
	uint32_t read_bytes, uint32_t zero_bytes, bool writable){

	struct spt_entry* s = malloc(sizeof(struct spt_entry));
	
	if(!s){
		return false;
	}

	s->lazyload = false;
	
	//type == file;
	s->type = 1;
	s->file = file;
	s->ofs = ofs;
	s->upage = upage;
	s->read_bytes = read_bytes;
	s->zero_bytes = zero_bytes;
	s->writable = writable;
	s->pinned = false;
	//printf("adding %p, type: %d\n", s->upage, s->type);


	lock_acquire(&thread_current()->spt_lock);
	list_push_back(&thread_current()->sup_page_table, &s->spt_elem);
	hash_insert(&thread_current()->spt_hash, &s->hash_elem);
	//printf("hash table size %d\n", hash_size(&thread_current()->spt_hash));
	//printf("added to spt %p\n", upage);
	lock_release(&thread_current()->spt_lock);

	return true;
}

struct spt_entry* look_for_entry(const void *addr){
	void* page = pg_round_down(addr);
	struct spt_entry spt;
	spt.upage = page;


	struct thread* t = thread_current();
	lock_acquire(&thread_current()->spt_lock);

	struct hash_elem* e = hash_find(&t->spt_hash, &spt.hash_elem);

	lock_release(&thread_current()->spt_lock);
	if(!e){
		return NULL;
	}

	return hash_entry(e, struct spt_entry, hash_elem);
}

bool load_lazy(const void* addr){
	void* page = pg_round_down(addr);
	// Look for addr in sup page table
	struct spt_entry* spt = look_for_entry(page);

	if(spt == NULL){
		printf("%p entry was not in sup page table\n", addr);
		return false;
	} 

	//load file
	if(spt->type == 1){
		uint8_t *kpage = frame_palloc (PAL_USER, spt);
		if(!kpage){
			evict_frame();
			kpage = frame_palloc(PAL_USER, spt);
			// return false;
		}

		// /* Load this page. */
		if (file_read_at(spt->file, kpage, spt->read_bytes, spt->ofs) != (int) spt->read_bytes)
		{
		  frame_free (kpage);
		  return false; 
		}
		memset (kpage + spt->read_bytes, 0, spt->zero_bytes);

		/* Add the page to the process's address space. */
		if (!install_page (spt->upage, kpage, spt->writable)) 
		{
		  frame_free (kpage);
		  return false; 
		}

		spt->lazyload = true;
	} else if(spt->type == 2){
		uint8_t *kpage = frame_palloc (PAL_USER, spt);
		if (!kpage){
			return false;
		}
		if (!install_page (spt->upage, kpage, spt->writable)){
			frame_free (kpage);
			return false; 
		}

		swap_in(spt->swap_index, spt->upage);
		spt->lazyload = true;
	}

	return true;

}

bool grow_stack(const void * addr){

	uint32_t size = (uint32_t) (PHYS_BASE - pg_round_down(addr));

	uint32_t maxstacksize = (1<<23);

	//if requested stack is larger than maxstacksize
	if (size > maxstacksize) {
      	return false;
  	}

	struct spt_entry* spt= malloc(sizeof(struct spt_entry));


	void * page = pg_round_down(addr);

	//type = stack
	spt->type = 2;
	spt->upage = page;
	spt->lazyload = true;
	spt->writable = true;
	spt->pinned = true;


	void * kpage = frame_palloc(PAL_USER, spt);

	//printf("adding %p, type: %d\n", page, spt->type);
	while(!kpage){
		//printf("evicting someone\n");
		evict_frame();
		kpage = frame_palloc(PAL_USER, spt);
	}
	//printf("successfully evicted\n");


	if (!install_page (spt->upage, kpage, spt->writable)) 
	{
	  frame_free (kpage);
	  return false; 
	}

	//printf("growing stack %p\n", spt->upage);
	lock_acquire(&thread_current()->spt_lock);
	list_push_back(&thread_current()->sup_page_table, &spt->spt_elem);
	hash_insert(&thread_current()->spt_hash, &spt->hash_elem);
	lock_release(&thread_current()->spt_lock);

	return true;
}


