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
#include "devices/block.h"
#include "vm/swap.h"

void swap_init (void)
{
	block = block_get_role (BLOCK_SWAP);
	if (!block)
	{
		return;
	}
	bitmap = bitmap_create( block_size(block) / 8 );
	if (!bitmap)
	{
		return;
	}
	bitmap_set_all(bitmap, false);
	lock_init(&swap_lock);
}

void read_from_block(uint8_t* frame, int index){
	for(int i = 0; i < 8; i++){
		block_read(block, index + i, frame + (i * BLOCK_SECTOR_SIZE));
	}
}

void write_from_block(uint8_t* frame, int index){
	for(int i = 0; i < 8; i++){
		block_write(block, index + i, frame + (i * BLOCK_SECTOR_SIZE));
	}
}

size_t look_for_free_index(void){
	//bitmap_scan_and_flip (struct bitmap *b, size_t start, size_t cnt, bool value)
	size_t index = bitmap_scan_and_flip(bitmap, 0, 1, 0);

	//if swap is full return -1;
	if(index == BITMAP_ERROR){
		PANIC("Swap is full!");
	}

	return index;
}

size_t swap_out(void* frame){
	struct thread* t = thread_current();
	//printf("thread current trying to obtain lock %s\n", t->name);
	lock_acquire(&swap_lock);
	//printf("2\n");
	size_t index = look_for_free_index();
	//printf("index %d\n", index);

	write_from_block(frame, index);
	//printf("thread current will release lock after this line %s\n", t->name);
	lock_release(&swap_lock);


	return index;
}

void check_swap_block(int index){
	if(bitmap_test(bitmap, index) == 0){
		PANIC("Trying to swap in a free block!");
	}
}


void swap_in(int index, void* frame){
	lock_acquire(&swap_lock);

	bitmap_set(bitmap, index, 1);
	write_from_block(frame, index);

	lock_release(&swap_lock);
}