#include "lib.h"
#define NUM_INODE 1e6
#define NUM_BLOCKS_INODE 64*256
#define INODE_SIZE 64
#define NUM_BLOCKS 256*256*128
#define NUM_BLOCKS_BLOCK_BITMAP 256
#define BLOCK_BITMAP_START 0
#define INODE_START NUM_BLOCKS_BLOCK_BITMAP
#define NUM_ENTRY_CACHE 32768
#define MAX_OBJS 1e6
#define MEM_BLOCK_START INODE_START + NUM_BLOCKS_INODE

long total_blocks = 0;
// cache policy, data strcucture?
#define malloc_4k(x,num) do{\
                         (x) = mmap(NULL,num*BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);\
                         if((x) == MAP_FAILED)\
                              (x)=NULL;\
                     }while(0); 

#define free_4k(x,y) munmap((x), y*BLOCK_SIZE)
static char * block_bitmap;
static char * inodes;

struct cache_struct{
     int block_num;
     int dirty;
};

struct cache_struct *cache_tab;

struct inode{
  int size;
  int valid;
  int id;
  // pointer implementation is wrong as we shouldn't find next_free block during this
  int pt[4];
  char key[32];
  char left[4];
};

// struct inode* obj;

#ifdef CACHE
static void remove_object_cached(struct objfs_state *objfs, int block_num)
{
  if(cache_tab[block_num%NUM_ENTRY_CACHE].block_num == block_num){
    cache_tab[block_num%NUM_ENTRY_CACHE].dirty = 0;
    cache_tab[block_num%NUM_ENTRY_CACHE].block_num = -1;
  }
  return;
}
static int find_read_cached(struct objfs_state *objfs, int block_num, char *user_buf)
{
  char * cache_ptr = (objfs->cache + 4096*(block_num%NUM_ENTRY_CACHE));
  if(cache_tab[block_num%NUM_ENTRY_CACHE].block_num != block_num){
    if(cache_tab[block_num%NUM_ENTRY_CACHE].dirty == 1){
      int blk = cache_tab[block_num%NUM_ENTRY_CACHE].block_num;
      write_block(objfs,blk,cache_ptr);
      cache_tab[block_num%NUM_ENTRY_CACHE].dirty = 0;
    }
    cache_tab[block_num%NUM_ENTRY_CACHE].block_num = block_num;
    read_block(objfs, block_num, cache_ptr);
  }
  memcpy(user_buf, cache_ptr, 4096);
  return 0;
}
/*Find the object in the cache and update it*/
static int find_write_cached(struct objfs_state *objfs, int block_num, const char *user_buf, int size)
{
  dprintf("I am in cached write\n");
  char * cache_ptr = (objfs->cache + 4096*(block_num%NUM_ENTRY_CACHE));
  if(cache_tab[block_num%NUM_ENTRY_CACHE].block_num != block_num){  /*Not in cache*/
    if(cache_tab[block_num%NUM_ENTRY_CACHE].dirty == 1){
      int blk = cache_tab[block_num%NUM_ENTRY_CACHE].block_num;
      write_block(objfs,blk,cache_ptr);
      cache_tab[block_num%NUM_ENTRY_CACHE].dirty = 0;
    }
    if(read_block(objfs, block_num, cache_ptr) < 0)
      return -1;
    cache_tab[block_num%NUM_ENTRY_CACHE].block_num = block_num;
  }
  memcpy(cache_ptr, user_buf, size);
  cache_tab[block_num%NUM_ENTRY_CACHE].dirty = 1;
  return 0;
}
/*Sync the cached block to the disk if it is dirty*/
static int obj_sync(struct objfs_state *objfs, int block_num)
{
  char * cache_ptr = (objfs->cache + 4096*(block_num%NUM_ENTRY_CACHE));
  if(!cache_tab[block_num%NUM_ENTRY_CACHE].dirty || cache_tab[block_num%NUM_ENTRY_CACHE].block_num!=block_num)
    return 0;
  if(write_block(objfs, block_num, cache_ptr) < 0)
    return -1;
  cache_tab[block_num%NUM_ENTRY_CACHE].dirty = 0;
  return 0;
}
#else
static void remove_object_cached(struct objfs_state *objfs, int block_num)
{
  return;
}
static int find_read_cached(struct objfs_state *objfs, int block_num, char *user_buf)
{
  read_block(objfs, block_num, user_buf);
  return 0;
}
static int find_write_cached(struct objfs_state *objfs, int block_num, char *user_buf, int size)
{
  write_block(objfs,block_num,user_buf);
  return 0;
}
static int obj_sync(struct objfs_state *objfs, int block_num)
{
  return 0;
}
#endif

long next_bitmap(struct objfs_state* objfs){
  for(int i=((MEM_BLOCK_START)>>3);i<((total_blocks)>>3);i++){
    char a = block_bitmap[i];
    if((a&0x0FF)==(0x0FF)){
      dprintf("%x ya\n",block_bitmap[i]);
      continue;
    }
    else{
      for(int j=7;j>=0;j--){
        if(!(a&((0x01)<<j))){
          dprintf("\n--finding free: i-%d j-%d\n",i,j);
          dprintf("%x\n",block_bitmap[i]);
          block_bitmap[i] |= ((0x01)<<j);
          dprintf("%x\n",block_bitmap[i]);
          return ((i<<3) + (7-j));
        }
      }
    }
  }
  return -1;
}

void remove_bit(int b_no){
  int i = (b_no>>3);
  int j = 7-(b_no%8);
  block_bitmap[i] ^= ((0x01)<<j);
  return;
}

/*
Returns the object ID.  -1 (invalid), 0, 1 - reserved
*/
long find_object_id(const char *key, struct objfs_state *objfs)
{
  for(int ctr=0; ctr < MAX_OBJS; ++ctr){
    struct inode* obj = ((struct inode*)(inodes))+ctr;
    if(obj->id && !strcmp(obj->key, key)){
      dprintf("\nI am finding: %s %d\n",obj->key,obj->id);
      return obj->id;
    }
  }
  return -1;
}

/*
  Creates a new object with obj.key=key. Object ID must be >=2.
  Must check for duplicates.

  Return value: Success --> object ID of the newly created object
                Failure --> -1
*/
long create_object(const char *key, struct objfs_state *objfs)
{
  // dprintf("\nI am in create\n");
  if(strlen(key) > 32)
    return -1;
  struct inode* obj;
  if(find_object_id(key, objfs)>0)
    return -1;
  for(int ctr=0; ctr < MAX_OBJS; ++ctr){
    obj = ((struct inode*)(inodes)) + ctr;
    if(!obj->id){
      // lock
      obj->id = ctr+2;
      obj->size = 0;
      obj->pt[0] = 0;
      obj->pt[1] = 0;
      obj->pt[2] = 0;
      obj->pt[3] = 0;
      break;
    }
    else if(obj->id && !strcmp(obj->key, key)){
      return -1;
    }
  }
  if(!obj->id){
   dprintf("%s: objstore full\n", __func__);
   return -1;
  }
  strcpy(obj->key, key);
  dprintf("%s: %d\n", obj->key,obj->id);
  return obj->id;
}
/*
  One of the users of the object has dropped a reference
  Can be useful to implement caching.
  Return value: Success --> 0
                Failure --> -1
*/
long release_object(int objid, struct objfs_state *objfs)
{
  return 0;
}

/*
  Destroys an object with obj.key=key. Object ID is ensured to be >=2.

  Return value: Success --> 0
                Failure --> -1
*/
long destroy_object(const char *key, struct objfs_state *objfs){
  for(int ctr=0; ctr < MAX_OBJS; ++ctr){
    struct inode * obj = ((struct inode*)(inodes)) + ctr;
    if(obj->id && !strcmp(obj->key, key)){
      char * buf1;
      malloc_4k(buf1,1);
      for(int i=0;i<=3;i++){
        int b_no = obj->pt[i];
        if(b_no == 0){
          break;
        }
        find_read_cached(objfs,b_no,buf1);
        int off = 0;
        while(off<=1023){
          b_no = *((int *)(buf1) + off);
          if(b_no == 0) break;
          // concurrency
          remove_bit(b_no);
          remove_object_cached(objfs,b_no);
          off++;
        }
        remove_bit(obj->pt[i]);
        remove_object_cached(objfs,obj->pt[i]);
        obj->pt[i] = 0;
      }
      free_4k(buf1,1);
      obj->id = 0;
      obj->size = 0;
      obj->pt[0] = 0;
      obj->pt[1] = 0;
      obj->pt[2] = 0;
      obj->pt[3] = 0;
      return 0;
    }
  }
  return -1;
}

/*
  Renames a new object with obj.key=key. Object ID must be >=2.
  Must check for duplicates.  
  Return value: Success --> object ID of the newly created object
                Failure --> -1
*/

long rename_object(const char *key, const char *newname, struct objfs_state *objfs)
{
  long objid;
  if((objid = find_object_id(key, objfs)) < 0)  
    return -1;
  if(find_object_id(newname, objfs)>0)
    return -1;
  // lock
  struct inode* obj = ((struct inode*)(inodes)) + objid - 2;
  if(strlen(newname) > 32)
    return -1;
  strcpy(obj->key, newname);
  dprintf("\nrename: %s %d %d",obj->key,obj->size,obj->id);
  return 0;
}

/*
  Writes the content of the buffer into the object with objid = objid.
  Return value: Success --> #of bytes written
                Failure --> -1
*/
long objstore_write(int objid, const char *buf, int size, struct objfs_state *objfs, off_t offset)
{
  // dprintf("\nI am in write\n");
  if(objid<2) return -1;
  struct inode* obj = ((struct inode*)(inodes)) + (objid - 2);
  int file_size = obj->size;
  if(offset > file_size) return -1;
  int ind_st = offset >> 22;
  int ind_end = (offset+size) >> 22;
  int l_st = (offset - ((ind_st)<<22)) >> 12;
  int write = size;
  int off;
  int index = 0;
  for(int i=ind_st;i<=ind_end;i++){
    if(write<=0)break;
    char * buf1;
    malloc_4k(buf1,1);
    int b_no = obj->pt[i];
    if(b_no == 0){
      b_no = next_bitmap(objfs);
      dprintf("\n---free block: %d\n",b_no);
      if(b_no==-1){
        dprintf("%s: objstore full\n", __func__);
        return -1;
      }
      for(int k=0;k<1024;k++){
        *(((int *)(buf1)) + k) = 0;
      }
      obj->pt[i] = b_no;
    }
    else{
      find_read_cached(objfs,b_no,buf1);
    }
    if(i==ind_st)off = l_st;
    else off = 0;
    while(off<=1023){
      b_no = *((int *)(buf1) + off);
      if(b_no == 0){
        b_no = next_bitmap(objfs);
        dprintf("\n---free block: %d\n",b_no);
        if(b_no==-1){
          dprintf("%s: objstore full\n", __func__);
          return -1;
        }
        obj->size += 4096;
        *((int *)(buf1) + off) = b_no;
      }
      char *temp;
      temp = mmap(NULL,BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
      for(int j=0;j<4096;j++){
        *(temp+j) = *(buf+(4096*index)+j);
      }
      find_write_cached(objfs,b_no,temp,4096);
      free_4k(temp,1);
      off++;
      index++;
      write -= 4096;
      if(write<=0)break;
    }
    find_write_cached(objfs,obj->pt[i],buf1,4096);
    free_4k(buf1,1);
  }
  return size;
}

/*
  Reads the content of the object onto the buffer with objid = objid.
  Return value: Success --> #of bytes written
                Failure --> -1
*/
long objstore_read(int objid, char *buf, int size, struct objfs_state *objfs, off_t offset)
{
  if(objid<2) return -1;
  struct inode* obj = ((struct inode*)(inodes)) + (objid - 2);
  int file_size = obj->size;
  dprintf("\nI am in read, id: %d size: %d\n",objid,file_size);
  if(offset > file_size) return -1;
  if((offset + size) > file_size) size = file_size - offset;
  int ind_st = offset >> 22;
  int ind_end = (offset+size) >> 22;
  int l_st = (offset - ((ind_st)<<22)) >> 12;
  int read = size;
  int off;
  int index = 0;
  for(int i=ind_st;i<=ind_end;i++){
    if(read<=0) break;
    char * buf1;
    malloc_4k(buf1,1);
    int b_no = obj->pt[i];
    find_read_cached(objfs,b_no,buf1);
    dprintf("\nreading l1: %s bno: %d\n",buf1,b_no);
    if(i==ind_st)off = l_st;
    else off = 0;
    while(off<=1023){
      char *temp;
      temp = mmap(NULL,BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
      find_read_cached(objfs,*((int *)(buf1) + off),temp);
      for(int j=0;j<4096;j++){
        *(buf+(4096*index)+j) = *(temp + j);
      }
      dprintf("\nll read: %s %d\n",buf+(4096*index),*((int *)(buf1) + off));
      free_4k(temp,1);
      off++;
      index++;
      read -= 4096;
      free_4k(buf1,1);
      if(read<=0)break;
    }
  }
  return size;
}

/*
  Reads the object metadata for obj->id = buf->st_ino
  Fillup buf->st_size and buf->st_blocks correctly
  See man 2 stat 
*/
int fillup_size_details(struct stat *buf, struct objfs_state *objfs)
{
  struct inode* obj = ((struct inode *)(inodes)) + (buf->st_ino - 2);
  if(buf->st_ino < 2 || obj->id != buf->st_ino)
    return -1;
  buf->st_size = obj->size;
  buf->st_blocks = obj->size >> 9;
  if(((obj->size >> 9) << 9) != obj->size)
     buf->st_blocks++;
  return 0;
}

/*
   Set your private pointers, anyway you like.
*/
int objstore_init(struct objfs_state *objfs)
{
  malloc_4k(block_bitmap,NUM_BLOCKS_BLOCK_BITMAP);
  malloc_4k(inodes,NUM_BLOCKS_INODE);
  malloc_4k(cache_tab,64);
  for(int i=0;i<NUM_BLOCKS_BLOCK_BITMAP;i++){
    read_block(objfs,i+BLOCK_BITMAP_START,(block_bitmap+4096*i));
  }
  for(int i=0;i<NUM_BLOCKS_INODE;i++){
    read_block(objfs,i+INODE_START,(inodes+4096*i));
  }
  for(int i=0;i<NUM_ENTRY_CACHE;i++){
    (cache_tab+i)->block_num = -1;
    (cache_tab+i)->dirty = 0;
  }
  // struct stat sbuf;
  // if(fstat(objfs->blkdev, &sbuf) < 0){
  //  perror("fstat");
  //  exit(-1);
  // }
  total_blocks = (objfs->disksize) >> 12;
  objfs->objstore_data = (void *)inodes;
  dprintf("Done objstore init\n");
  return 0;
}

/*
   Cleanup private data. FS is being unmounted
*/
int objstore_destroy(struct objfs_state *objfs)
{
  for(int i=0;i<NUM_BLOCKS_BLOCK_BITMAP;i++){
    write_block(objfs,i+BLOCK_BITMAP_START,(block_bitmap+4096*i));
  }
  for(int i=0;i<NUM_BLOCKS_INODE;i++){
    write_block(objfs,i+INODE_START,(inodes+4096*i));
  }
  for(int i=0;i<NUM_ENTRY_CACHE;i++){
    if(cache_tab[i].block_num == -1)continue;
    obj_sync(objfs, cache_tab[i].block_num);
  }
  free_4k(block_bitmap,NUM_BLOCKS_BLOCK_BITMAP);
  free_4k(inodes,NUM_BLOCKS_INODE);
  free_4k(cache_tab,64);
  objfs->objstore_data = NULL;
  dprintf("Done objstore destroy\n");
  return 0;
}
