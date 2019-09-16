#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  printf("\tbm: reading block %d\n", id);
  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  printf("\tbm: writing to block %d\n", id);
  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  blockid_t blkid = IBLOCK(sb.ninodes, sb.nblocks) + 1;
  char buf[BLOCK_SIZE];
  do {
    read_block(BBLOCK(blkid), buf);
    for(; blkid % BPB != 0; ++blkid) {
      if((buf[blkid / 8] & (1 << (blkid & 0x7))) == 0) {
        // allocate this block
        buf[blkid / 8] |= 1 << (blkid & 0x7);
        write_block(BBLOCK(blkid), buf);
        printf("\tbm: allocated block %d\n", blkid);
        return blkid;
      }
    }
  } while(blkid < sb.nblocks);
  return -1;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  char buf[BLOCK_SIZE];
  read_block(BBLOCK(id), buf);
  buf[id / 8] &= ~(1 << (id & 0x7));
  write_block(BBLOCK(id), buf);
  printf("\tbm: free block %d\n", id);
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  static uint32_t next_inum = 1;
  char buf[BLOCK_SIZE];

  memset(buf, 0, sizeof(buf));
  inode_t *newnode = (inode_t *)buf;
  newnode->type = type;
  newnode->atime = newnode->ctime = newnode->mtime = time(nullptr);
  bm->write_block(IBLOCK(next_inum, bm->sb.nblocks), buf);
  printf("\tim: allocated inode num = %d\n", next_inum);
  return next_inum++;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */
  printf("\tim: freeing inode %d\n", inum);
  inode_t *node = get_inode(inum);
  if(node != nullptr) {
    node->type = 0;
    put_inode(inum, node);
    free(node);
  }
  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  ino_disk->ctime = time(nullptr);
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */
  inode_t *inode = get_inode(inum);
  blockid_t *blk_list = inode->blocks;

  char *buf = (char *)malloc(inode->size);
  *buf_out = buf;
  char internal_buf[BLOCK_SIZE];
  int rsize = inode->size;
  
  printf("\tim: reading file %d, size = %d\n", inum, rsize);
  while(rsize) {
    for(int i = 0; rsize > 0 && i < NDIRECT && blk_list[i]; i++) {
      if(rsize >= BLOCK_SIZE) {
        bm->read_block(blk_list[i], &(buf[i * BLOCK_SIZE]));
        rsize -= BLOCK_SIZE;
      } else {
        // reuse internal buf here directly
        bm->read_block(blk_list[i], internal_buf);
        memcpy(&(buf[i * BLOCK_SIZE]), internal_buf, rsize);
        rsize = 0;
      }
    }
    if(rsize > 0) {
      // not done, use indirect block & continue
      // reuse buf directly, won't cause trouble
      printf("Following indirect block @ %d\n", blk_list[NDIRECT]);
      bm->read_block(blk_list[NDIRECT], internal_buf);
      blk_list = (blockid_t *)internal_buf;
      buf += BLOCK_SIZE * NDIRECT;
    }
  }
  inode->atime = time(nullptr);
  put_inode(inum, inode);
  *size = inode->size;
  free(inode);
  return;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  printf("\tim: writing to file %d, size = %d\n", inum, size);
  inode_t *node = get_inode(inum);
  blockid_t *blk_list = node->blocks;
  int rsize = size;
  int i;
  char internal_buf[BLOCK_SIZE];
  int blocklist_blkid;
  char blocklist_buf[BLOCK_SIZE];

  while(rsize) {
    for(i = 0; rsize > 0 && i < NDIRECT; i++) {
      if(!blk_list[i]) {
        blk_list[i] = bm->alloc_block();
      }
      if(rsize >= BLOCK_SIZE) {
        bm->write_block(blk_list[i], &buf[i * BLOCK_SIZE]);
        rsize -= BLOCK_SIZE;
      } else {
        memset(internal_buf, 0, sizeof(internal_buf));
        memcpy(internal_buf, (void *)&buf[i * BLOCK_SIZE], rsize);
        bm->write_block(blk_list[i], internal_buf);
        rsize = 0;
      }
    }
    if(rsize) {
      // allocate indirect block & continue
      if(blk_list == (blockid_t *)blocklist_buf) {
        // flush previous indirect block & update
        printf("Flushing indirect block buffer @ %d\n", blocklist_blkid);
        blk_list[NDIRECT] = bm->alloc_block();
        bm->write_block(blocklist_blkid, blocklist_buf);
        blocklist_blkid = blk_list[NDIRECT];
      } else
        blocklist_blkid = blk_list[NDIRECT] = bm->alloc_block();
      
      printf("Following indirect block @ %d\n", blocklist_blkid);
      // clear indirect block buffer
      memset(blocklist_buf, 0, BLOCK_SIZE);
      // set the pointer to block buffer
      blk_list = (blockid_t *)blocklist_buf;
      // add offset to buf & continue
      buf += BLOCK_SIZE * NDIRECT;
    } else if(blk_list == (blockid_t *)blocklist_buf) {
      // flush previous indirect block & exit
      printf("Flushing indirect block buffer @ %d\n", blocklist_blkid);
      bm->write_block(blocklist_blkid, blocklist_buf);
    }
  }
  // update inode information
  node->size = size;
  node->mtime = time(nullptr);
  put_inode(inum, node);
  free(node);
  return;
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  inode *node = get_inode(inum);
  if(node) {
    a.type = node->type;
    a.size = node->size;
    a.atime = node->atime;
    a.ctime = node->ctime;
    a.mtime = node->mtime;
    free(node);
  } else {
    a.type = 0;
    a.size = 0;
    a.atime = a.ctime = a.mtime = 0;
  }
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  printf("\tim: removing file %d\n", inum);
  inode_t *node = get_inode(inum);
  blockid_t *blk_list = node->blocks;
  char buf[BLOCK_SIZE];
  do {
    for(int i = 0; i < NDIRECT && blk_list[i]; i++)
      // free block one by one
      bm->free_block(blk_list[i]);
    if(blk_list[NDIRECT]) {
      bm->read_block(blk_list[NDIRECT], buf);
      blk_list = (blockid_t *)buf;
    } else break;
  } while(true);
  free(node);
  free_inode(inum);
  return;
}
