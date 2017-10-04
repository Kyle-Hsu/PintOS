#ifndef VM_SWAP_H
#define VM_SWAP_H

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
#include <bitmap.h>

struct lock swap_lock;
struct block* block;
struct bitmap* bitmap;



void swap_init (void);
void read_from_block(uint8_t* frame, int index);
void write_from_block(uint8_t* frame, int index);
size_t look_for_free_index(void);
size_t swap_out(void* frame);
void check_swap_block(int index);
void swap_in(int index, void* frame);

#endif /* vm/swap.h */
