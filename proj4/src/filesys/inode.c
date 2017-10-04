#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/thread.h"
/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44


/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (0 <= pos && pos < inode->data.length){

    off_t offset = pos / BLOCK_SECTOR_SIZE;


    return pos_to_sector_index(&inode->data, offset);
  }

    //return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  else{
    return -1;
  }
}


block_sector_t pos_to_sector_index (const struct inode_disk *inode_disk, off_t index){
  if(index < DIRECT_BLOCKS){
    return inode_disk->direct_index[index];
  }

  if(index < DIRECT_BLOCKS + 128){
    block_sector_t indirect_block[128];

    block_read(fs_device, inode_disk->indirect_index, indirect_block);

    block_sector_t returnblock = indirect_block[index - DIRECT_BLOCKS];

    return returnblock;
  }

  off_t index_base = DIRECT_BLOCKS + 128;
  if(index < DIRECT_BLOCKS + 128 + 128*128){
    off_t first_index =  (index - index_base) / 128;
    off_t second_index = (index - index_base) % 128;

    block_sector_t doubly_block[128];

    block_read(fs_device, inode_disk->d_indirect_index, doubly_block);

    block_sector_t indirect_block[128];

    block_read(fs_device, doubly_block[first_index], indirect_block);

    block_sector_t returnblock = indirect_block[second_index];

    return returnblock;
  }

  return -1;
}


/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool is_dir)
{
  if(is_dir){
    printf("inode isdir\n");
  }
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {

      size_t sectors = bytes_to_sectors (length);

      disk_inode->length = length;

      if (disk_inode->length > MAX_FILE_SIZE) {
        disk_inode->length = MAX_FILE_SIZE;
      }


      disk_inode->magic = INODE_MAGIC;
      disk_inode->is_directory = is_dir;
      disk_inode->parent = ROOT_DIR_SECTOR;
      if(inode_alloc(sectors, disk_inode)) {
        block_write (fs_device, sector, disk_inode); 
        success = true;
      }
      free(disk_inode);


      // if (free_map_allocate (sectors, &disk_inode->start)) 
      //   {
      //     block_write (fs_device, sector, disk_inode);
      //     if (sectors > 0) 
      //       {
      //         static char zeros[BLOCK_SECTOR_SIZE];
      //         size_t i;
              
      //         for (i = 0; i < sectors; i++) 
      //           block_write (fs_device, disk_inode->start + i, zeros);
      //       }
      //     success = true; 
      //   } 
      // free (disk_inode);
    }
  return success;
}


/*Allocates SECTOR number of sectors and calculates how many
direct, indirect, doubly indirect sectors needed and 
initializes them as zeros. */
bool inode_alloc (size_t sectors, struct inode_disk* disk_inode){
  //printf("need %d blocks for this length %d\n", sectors, disk_inode->length );
  static char zeros[BLOCK_SECTOR_SIZE];

  size_t num_direct_sectors = DIRECT_BLOCKS;  
  if(sectors < DIRECT_BLOCKS){
    num_direct_sectors = sectors;
  }

  for(unsigned int i = 0; i < num_direct_sectors; i++){
    free_map_allocate(1, &disk_inode->direct_index[i]);
    block_write(fs_device, disk_inode->direct_index[i], zeros);
  }
 // printf("1\n");
  if(sectors > DIRECT_BLOCKS){

    size_t num_indirect_sectors = NUM_SECTORS_IN_BLOCK;

    if(sectors < DIRECT_BLOCKS + NUM_SECTORS_IN_BLOCK){
      num_indirect_sectors = sectors - DIRECT_BLOCKS;
    }



    block_sector_t indirect_block[NUM_SECTORS_IN_BLOCK];

    bool success = free_map_allocate(1, &disk_inode->indirect_index);
   // printf("need this many blocks %d\n", num_indirect_sectors);
    for(unsigned int i = 0; i < num_indirect_sectors; i++){
      free_map_allocate(1, &indirect_block[i]);
      block_write(fs_device, indirect_block[i], zeros);
    }

    block_write(fs_device, disk_inode->indirect_index, indirect_block);
  }

  if(sectors > DIRECT_BLOCKS + 1*NUM_SECTORS_IN_BLOCK){
    size_t num_double_indirect_sectors = sectors - (DIRECT_BLOCKS + NUM_SECTORS_IN_BLOCK);

    size_t lvl_one_index = num_double_indirect_sectors / NUM_SECTORS_IN_BLOCK;
    size_t lvl_two_index = num_double_indirect_sectors % NUM_SECTORS_IN_BLOCK;
    

    //block to store doubly indirect sector numbers
    free_map_allocate(1, &disk_inode->d_indirect_index);

    block_sector_t indirect_block[NUM_SECTORS_IN_BLOCK];


    for(unsigned int i = 0; i < lvl_one_index; i++){
      
      //block to store indirect sector numbers 
      free_map_allocate(1, &indirect_block[i]);

      block_sector_t double_indirect_block[NUM_SECTORS_IN_BLOCK];
      
      for(unsigned int j = 0; j < NUM_SECTORS_IN_BLOCK; j++){
        
        //block to store zeros
        free_map_allocate(1, &double_indirect_block[j]);
        block_write(fs_device, double_indirect_block[j], zeros);
      }

      block_write(fs_device, indirect_block[i], double_indirect_block);
    }

    //block to store last indirect sector number
    free_map_allocate(1, &indirect_block[lvl_one_index]);
    block_sector_t double_indirect_block[NUM_SECTORS_IN_BLOCK];

    for(unsigned int i = 0; i < lvl_two_index; i++){
      //block that are needed in the new indirect sector's entries
      free_map_allocate(1, &double_indirect_block[i]);
      block_write(fs_device, double_indirect_block[i], zeros);
    }
    block_write(fs_device, indirect_block[lvl_one_index], double_indirect_block);

    block_write(fs_device, disk_inode->d_indirect_index, indirect_block);

  }

  return true;
}

void inode_dealloc_direct(struct inode* inode, size_t numSectors){
  
  for(unsigned int i = 0; i < numSectors; i++){
    free_map_release(inode->data.direct_index[i], 1);
  }
}


void inode_dealloc_indirect(struct inode* inode, size_t numSectors){
  // inode_dealloc_direct(inode, DIRECT_BLOCKS);

  numSectors -= DIRECT_BLOCKS;

  block_sector_t indirect_indices[NUM_SECTORS_IN_BLOCK];

  block_read(fs_device, inode->data.indirect_index, indirect_indices);

  for(unsigned int i = 0; i < numSectors; i++){
    free_map_release(indirect_indices[i], 1);
  }

  free_map_release(inode->data.indirect_index, 1);
}


void inode_dealloc_double_indirect(struct inode* inode, size_t numSectors){
  // inode_dealloc_direct(inode, numSectors);
  // inode_dealloc_indirect(inode, numSectors);
  numSectors -= (DIRECT_BLOCKS + NUM_SECTORS_IN_BLOCK);

  block_sector_t double_indirect_indices[NUM_SECTORS_IN_BLOCK];

  block_read(fs_device, inode->data.d_indirect_index, double_indirect_indices);


  size_t lvl_one_index = numSectors / NUM_SECTORS_IN_BLOCK;
  size_t lvl_two_index = numSectors % NUM_SECTORS_IN_BLOCK;

  block_sector_t indirect_indices[NUM_SECTORS_IN_BLOCK];

  block_read(fs_device, double_indirect_indices[lvl_one_index], indirect_indices);



  for(unsigned int i = 0; i < lvl_two_index; i++){
    free_map_release(indirect_indices[i], 1);
  }

  free_map_release(double_indirect_indices[lvl_one_index], 1);

  for(unsigned int i = 0; i < lvl_one_index; i++){
    block_read(fs_device, double_indirect_indices[i], indirect_indices);

    for(unsigned int j = NUM_SECTORS_IN_BLOCK-1; j >= 0; j--){
      free_map_release(indirect_indices[j], 1);
    }
    free_map_release(double_indirect_indices[i], 1);
  }

  free_map_release(inode->data.d_indirect_index, 1);
}



bool inode_dealloc(struct inode* inode){
  off_t length = inode->data.length;
  if(length < 0){
    return false;
  }

  size_t sectors = bytes_to_sectors(length);

  if(sectors < DIRECT_BLOCKS){
    inode_dealloc_direct(inode, sectors);
  } else if (sectors < DIRECT_BLOCKS + NUM_SECTORS_IN_BLOCK){

    inode_dealloc_direct(inode, DIRECT_BLOCKS);
    inode_dealloc_indirect(inode, sectors);
  } else {

    inode_dealloc_direct(inode, DIRECT_BLOCKS);
    inode_dealloc_indirect(inode, sectors);
    inode_dealloc_double_indirect(inode, sectors);
  }

  return true;  
}



/*Expand INODE if writing pass EOF */
size_t inode_expand(struct inode* inode, size_t size){
  //printf("inode_expand inode_length %d\n", inode_length(inode));
  size_t curr_sectors = bytes_to_sectors(inode_length(inode));

  size_t add_sectors = bytes_to_sectors(size) - curr_sectors;

  for(unsigned int i = 0; i < add_sectors; i++){
    add_sector(inode, curr_sectors);
    curr_sectors++;


  }

  return size;
}

void add_sector(struct inode* inode, size_t sectors){
  //printf("adding sector\n");
  //printf("sectors number %d\n", sectors);
  static char zeros[BLOCK_SECTOR_SIZE];
  if(sectors < DIRECT_BLOCKS){
    free_map_allocate(1, &inode->data.direct_index[sectors]);
    block_write(fs_device, inode->data.direct_index[sectors], zeros);
  } 

  //need to create new indirect sector and one more block
  else if(sectors == DIRECT_BLOCKS){
    
    free_map_allocate(1, &inode->data.indirect_index);
    block_sector_t indirect_block[NUM_SECTORS_IN_BLOCK];

    free_map_allocate(1, &indirect_block[0]);
    block_write(fs_device, indirect_block[0], zeros);
    block_write(fs_device, inode->data.indirect_index, indirect_block);

  } 
  //need to add blocks to indirect sector
  else if(sectors < DIRECT_BLOCKS + NUM_SECTORS_IN_BLOCK){
    size_t index = sectors - DIRECT_BLOCKS;

    block_sector_t indirect_block[NUM_SECTORS_IN_BLOCK];

    block_read(fs_device, inode->data.indirect_index, indirect_block);

    free_map_allocate(1, &indirect_block[index]);

    block_write(fs_device, indirect_block[index], zeros);
    block_write(fs_device, inode->data.indirect_index, indirect_block);


  } 
  //need to create new doubly indirect and one indirect and one more block
  else if(sectors == DIRECT_BLOCKS + NUM_SECTORS_IN_BLOCK){
    free_map_allocate(1, &inode->data.d_indirect_index);

    block_sector_t double_indirect_block[NUM_SECTORS_IN_BLOCK];

    free_map_allocate(1, &double_indirect_block[0]);

    block_sector_t indirect_block[NUM_SECTORS_IN_BLOCK];

    free_map_allocate(1, &indirect_block[0]);

    block_write(fs_device, indirect_block[0], zeros);
    block_write(fs_device, double_indirect_block[0], indirect_block);
    block_write(fs_device, inode->data.d_indirect_index, double_indirect_block);


  } 
  //need to create new indirect and one more block
  else if((sectors - (DIRECT_BLOCKS + NUM_SECTORS_IN_BLOCK)) % NUM_SECTORS_IN_BLOCK == 0){
    size_t index = (sectors - (DIRECT_BLOCKS + NUM_SECTORS_IN_BLOCK)) / NUM_SECTORS_IN_BLOCK;

    block_sector_t double_indirect_block[NUM_SECTORS_IN_BLOCK];    

    block_read(fs_device, inode->data.d_indirect_index, double_indirect_block);

    free_map_allocate(1, &double_indirect_block[index]);

    block_sector_t indirect_block[NUM_SECTORS_IN_BLOCK];

    free_map_allocate(1, &indirect_block[0]);

    block_write(fs_device, indirect_block[0], zeros);
   
    block_write(fs_device, double_indirect_block[index], indirect_block);
    
    block_write(fs_device, inode->data.d_indirect_index, double_indirect_block);


  } 

  //need to add another to current indirect block
  else {
    size_t lvl_one_index = (sectors - (DIRECT_BLOCKS + NUM_SECTORS_IN_BLOCK)) / NUM_SECTORS_IN_BLOCK;
    size_t lvl_two_index = (sectors - (DIRECT_BLOCKS + NUM_SECTORS_IN_BLOCK)) % NUM_SECTORS_IN_BLOCK;

    block_sector_t double_indirect_block[NUM_SECTORS_IN_BLOCK];    

    block_read(fs_device, inode->data.d_indirect_index, double_indirect_block);

    block_sector_t indirect_block[NUM_SECTORS_IN_BLOCK];    

    block_read(fs_device, double_indirect_block[lvl_one_index], indirect_block);

    free_map_allocate(1, &indirect_block[lvl_two_index]);

    block_write(fs_device, indirect_block[lvl_two_index], zeros);
    block_write(fs_device, double_indirect_block[lvl_one_index], indirect_block);
  }
}





/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          // free_map_release (inode->data.start,
          //                   bytes_to_sectors (inode->data.length)); 
          inode_dealloc(inode);
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{

  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;
      
      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt){
    return 0;
  }
  //printf("size+offset %d , inode_length %d \n", size+offset, inode_length(inode) );
  if(size + offset > inode_length(inode)){
    inode->data.length = inode_expand(inode, size + offset);
    block_write(fs_device, inode->sector, &inode->data);

  }

  block_sector_t sector_id = byte_to_sector (inode, offset);
  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);
  // printf("write at inode length %d\n", inode_length(inode));
  // printf("bytes written %d, inode size: %d\n", bytes_written, inode->data.length);
  return bytes_written;

}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
