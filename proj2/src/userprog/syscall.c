#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "process.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include <list.h>
#include <user/syscall.h>
#include <string.h>

static void syscall_handler (struct intr_frame *);

void check_valid_address(const void * addr);
void halt(void );
void exit(int status);
int write(int fd, const void* buffer, unsigned size);
int read(int fd, void* buffer, unsigned size);
bool create(const char* file, unsigned initial_size);
void close(int fd);
pid_t exec(const char* cmd_line);
int open(const char* file);
bool remove(const char* file);
void close_all(void);

struct lock filesys;

void acquire_lock(void)
{
  lock_acquire(&filesys);
}

void release_lock(void)
{
  lock_release(&filesys);
}

void
syscall_init (void) 
{
  lock_init(&filesys);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  check_valid_address(f->esp);
  int sys_code = *(int*)f->esp;

  switch(sys_code){
  	case SYS_HALT:
  	{
  		halt();
  		break;
  	}
  	
  	case SYS_EXIT:
  	{
  		int status = *((int*)f->esp + 1);
  		exit(status);
  		break;
  	}
  	
  	case SYS_EXEC:
  	{
  		//printf("SYS_EXEC\n");

  		char* cmd_line = *((int*)f->esp + 1);
  		check_valid_address(cmd_line);
  		//printf("%p\n", cmd_line);
  		acquire_lock();
  		f->eax = exec(cmd_line);
  		release_lock();
  		break;
  	}
  	case SYS_WAIT:
  	{
  		//printf("SYS_WAIT\n");

		check_valid_address((int*)f->esp + 1);
		int pid = *((int*)f->esp + 1);
		// printf("%d\n", pid);
		//acquire_lock();
		f->eax = wait(pid);
		//release_lock();
  		break;
  	}
  	case SYS_CREATE:
  	{
		check_valid_address((unsigned*)f->esp + 2);
  		char* file = *((int*)f->esp + 1);

  		//printf("%p\n",file);

  		check_valid_address(file);
		unsigned initial_size = *((unsigned*)f->esp + 2);
		if(file == NULL){
			exit(-1);
		}

		acquire_lock();
		f->eax = create(file, initial_size);
		release_lock();

		break;
	}
	
	case SYS_REMOVE:
	{
		// printf("SYS_REMOVE\n");
		char* file = *((int*)f->esp + 1);
		check_valid_address(file);

		acquire_lock();
		f->eax = remove(file);
		release_lock();
		break;
  	}
  	case SYS_OPEN:
  	{
  		// printf("SYS_OPEN\n");
  		char* file = *((int*)f->esp + 1);
  		check_valid_address(file);

  		acquire_lock();
  		f->eax = open(file);
  		release_lock();
  		break;
  	}
  	
  	case SYS_FILESIZE:
  	{
  		// printf("SYS_FILESIZE\n");
		check_valid_address((int*)f->esp + 1);
		int fd = *((int*)f->esp + 1);

		acquire_lock();
		f->eax = filesize(fd);

		release_lock();

  		break;
  	}
  	
  	case SYS_READ:
  	{
  		// printf("SYS_READ\n");
  		check_valid_address((int*)f->esp + 1);
		//printf("checking buffer\n");
		check_valid_address((void*)(*((int*)f->esp + 2)));
		//printf("check buffer\n");
		check_valid_address((unsigned*)f->esp + 3);

		int fd = *((int*)f->esp + 1);
		void* buffer = (void*)(*((int*)f->esp + 2));
		unsigned size = *((unsigned*)f->esp + 3);

		//printf("Buffer pointer %p\n", buffer);

		acquire_lock();
		f->eax = read(fd,buffer,size);
		release_lock();
		//printf("F->EAX %d\n", f->eax);
  		break;
  	}
  	
  	case SYS_WRITE:
	{
		// printf("SYS_WRITE\n");	
		check_valid_address((int*)f->esp + 1);
		check_valid_address((void*)(*((int*)f->esp + 2)));
		check_valid_address((unsigned*)f->esp + 3);

		int fd = *((int*)f->esp + 1);
		void* buffer = (void*)(*((int*)f->esp + 2));
		unsigned size = *((unsigned*)f->esp + 3);

		//check that I am not writing to an executable

		acquire_lock();
		f->eax = write(fd,buffer,size);
		release_lock();
		break;
	}	
  	
  	case SYS_SEEK:
  	{
  		// printf("SYS_SEEK\n");	
  		check_valid_address((int*)f->esp + 1);
  		check_valid_address((unsigned*)f->esp + 2);

  		int fd = *((int*)f->esp + 1);
  		unsigned position = *((unsigned*)f->esp + 2);
  		acquire_lock();
  		seek(fd, position);
  		release_lock();
  		break;
  	}
  	
  	case SYS_TELL:
  	{
  		// printf("SYS_TELL\n");
  		check_valid_address((int*)f->esp + 1);
  		int fd = *((int*)f->esp + 1);
  		acquire_lock();
  		tell(fd);
  		release_lock();
  		break;
  	}
  	
  	case SYS_CLOSE:
  	{
  		check_valid_address((int*)f->esp + 1);
  		int fd = *((int*)f->esp + 1);

  		acquire_lock();
  		close(fd);
  		release_lock();

  		break;
  	}
  	
  	default:
  	{
  		printf("No syscall found\n");
  	}

  }
}

 
void check_valid_address(const void * addr){
	if(!is_user_vaddr(addr)){
		exit(-1);
	}

	void *flag = pagedir_get_page(thread_current()->pagedir, addr);
	if(!flag){
		exit(-1);
	}
}


void halt (void ){
	shutdown_power_off();
};

 
void exit(int status){
	if(status < 0){
		status = -1;
	}

	//make sure parent is still alive
	if(thread_current()->parent != NULL){
		lock_acquire(&thread_current()->parent->children_lock);
		struct child_status* waiting = NULL;

		for (struct list_elem* iter = list_begin(&thread_current()->parent->children); 
			iter != list_end(&thread_current()->parent->children);
			iter = list_next (iter))
		{
			struct child_status *cs = list_entry (iter, struct child_status, cs_elem);

			if(cs->pid == thread_current()->tid){
				cs->exit_status = status;
				waiting = cs;
			}
		}

		lock_release(&thread_current()->parent->children_lock);

		acquire_lock();
		close_all();
		file_close(thread_current()->myfile);
		release_lock();

		printf("%s: exit(%d)\n",thread_current()->name, status);

		sema_up(&waiting->sema);	

	//if parent is already dead	
	} else {
		acquire_lock();
		close_all();
		file_close(thread_current()->myfile);
		release_lock();

		printf("%s: exit(%d)\n",thread_current()->name, status);
	}

	//sema_up(&waiting->sema);
	thread_exit();
};

pid_t exec(const char* cmd_line){
	char * exec_name = malloc (strlen(cmd_line)+1);
	strlcpy(exec_name, cmd_line, strlen(cmd_line)+1);
	  
	char * saveptr;
	exec_name = strtok_r(exec_name," ",&saveptr);

	struct file* openfile = filesys_open (exec_name);
	free(exec_name);
	if(openfile == NULL){
		return -1;
	}
	//test to see if file exists
	file_close(openfile);

	int pid = process_execute(cmd_line);

	return pid;
}

int wait(pid_t pid){
	return process_wait(pid);
}

bool create(const char* file, unsigned initial_size){
	
	return filesys_create(file, initial_size);
}

bool remove(const char* file){
	return filesys_remove(file);
}

int open(const char* file){
	struct file* f = filesys_open(file);
	if(f == NULL){
		return -1;
	}
	struct open_file *open = malloc(sizeof(struct open_file));
	//printf("Opening file at this pointer: %p\n", f);
	if(!open){
		return -1;
	}
	open->file_pointer = f;
	open->fd = thread_current()->file_id;
	thread_current()->file_id++;
	list_push_back(&thread_current()->open_list, &open->file_elem);
	return open->fd;
}

int filesize(int fd){
	struct open_file* open = NULL;

	for (struct list_elem* iter = list_begin(&thread_current()->open_list); 
		iter != list_end(&thread_current()->open_list);
			iter = list_next (iter))
	{
		struct open_file *f = list_entry (iter, struct open_file, file_elem);
		if(f->fd == fd){
			open = f;
			break;
		}
	}

	int filesize = file_length(open->file_pointer);

	return filesize;
}

int read(int fd, void* buffer, unsigned size){

	if(fd == 0){
		unsigned char* buff = (unsigned char*) buffer;
		for (unsigned int i = 0; i < size; i++)
		{
			buff[i] = input_getc();
		}
		return size;
	} else {
		struct open_file* open = NULL;

		for (struct list_elem* iter = list_begin(&thread_current()->open_list); 
			iter != list_end(&thread_current()->open_list);
				iter = list_next (iter))
		{
			struct open_file *f = list_entry (iter, struct open_file, file_elem);
			if(f->fd == fd){
				open = f;
				break;
			}
		}

		if(open == NULL){
			return -1;
		} else {
			//acquire_lock();
			int returnval = file_read (open->file_pointer, buffer, size);
			//release_lock();
			return returnval;
		}
	}
}

int write(int fd, const void* buffer, unsigned size){
	if(fd ==1)
	{
		putbuf(buffer,size);
		return size;
	} else {
		struct open_file* open = NULL;

		for (struct list_elem* iter = list_begin(&thread_current()->open_list); 
			iter != list_end(&thread_current()->open_list);
				iter = list_next (iter))
		{
			struct open_file *f = list_entry (iter, struct open_file, file_elem);
			if(f->fd == fd){
				open = f;
				break;
			}
		}

		if(open == NULL){
			return -1;
		} else {
			//acquire_lock();

			int returnval = file_write(open->file_pointer, buffer, size);
			//release_lock();
			return returnval;
		}
	}
}

void seek(int fd, unsigned position){
	struct thread* t = thread_current();
	struct open_file* openf = NULL;
	for(struct list_elem *iter = list_begin(&t->open_list);
		iter != list_end(&t->open_list);
		iter = list_next(iter))
	{   
		struct open_file* f= list_entry(iter, struct open_file, file_elem);
		if(f->fd == fd){
			openf = f;
		}
	}
	file_seek(openf->file_pointer, position);
}

unsigned tell(int fd){
	struct thread* t = thread_current();
	struct open_file* openf = NULL;
	for(struct list_elem *iter = list_begin(&t->open_list);
		iter != list_end(&t->open_list);
		iter = list_next(iter))
	{   
		struct open_file* f= list_entry(iter, struct open_file, file_elem);
		if(f->fd == fd){
			openf = f;
		}
	}
	
	return file_tell(openf->file_pointer);
}

void close(int fd){

	struct thread* t = thread_current();
	struct open_file* file = NULL;
	for(struct list_elem *iter = list_begin(&t->open_list);
		iter != list_end(&t->open_list);
		iter = list_next(iter))
	{   
		struct open_file* f= list_entry(iter, struct open_file, file_elem);
		if(f->fd == fd){
			file = f;
			file_close(f->file_pointer);
			list_remove(&f->file_elem);
			break;
		}
	}
	if(!file){
		free(file);
	}
}	

void close_all(void){
	struct thread* t = thread_current();


	//I DID NOT ITERATE THROUGH THE LIST CORRECTLY
	// for(struct list_elem *iter = list_begin(&t->open_list);
	// 	iter != list_end(&t->open_list);
	// 	iter = list_next(iter))
	// {   
	// 	struct open_file* f= list_entry(iter, struct open_file, file_elem);
	// 	file_close(f->file_pointer);
	// 	list_remove(&f->file_elem);
	// 	//free(f);
	// }

	while(!list_empty(&t->open_list) ){
		struct open_file *f = list_entry( list_pop_front(&t->open_list),
			struct open_file, file_elem);
		file_close(f->file_pointer);
		free(f);
	}

}


