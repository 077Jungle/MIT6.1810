// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

struct bucket {
  struct spinlock lock;

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
};

struct {
  struct buf buf[NBUF];

  struct bucket buckets[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  for(int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.buckets[i].lock, "bcache");

    // Create linked list of buffers
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
    bcache.buckets[i].head.next = &bcache.buckets[i].head;
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.buckets[0].head.next;
    b->prev = &bcache.buckets[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].head.next->prev = b;
    bcache.buckets[0].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int idx = blockno % NBUCKET;
  acquire(&bcache.buckets[idx].lock);
  // Is the block already cached?
  for(b = bcache.buckets[idx].head.next; b != &bcache.buckets[idx].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.buckets[idx].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.buckets[idx].head.prev; b != &bcache.buckets[idx].head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.buckets[idx].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.buckets[idx].lock);
  // other unused buffer
  
  for(int i = 0; i < NBUCKET; i++) {
    acquire(&bcache.buckets[i].lock);
    for(b = bcache.buckets[i].head.prev; b != &bcache.buckets[i].head; b = b->prev) {
      if(b->refcnt == 0) {
        b->next->prev = b->prev;
        b->prev->next = b->next;
        release(&bcache.buckets[i].lock);
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        acquire(&bcache.buckets[idx].lock);
        b->next = bcache.buckets[idx].head.next;
        b->prev = &bcache.buckets[idx].head;
        bcache.buckets[idx].head.next->prev = b;
        bcache.buckets[idx].head.next = b;
        release(&bcache.buckets[idx].lock);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.buckets[i].lock);
  }


  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int idx = b->blockno % NBUCKET;

  acquire(&bcache.buckets[idx].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.buckets[idx].head.next;
    b->prev = &bcache.buckets[idx].head;
    bcache.buckets[idx].head.next->prev = b;
    bcache.buckets[idx].head.next = b;
  }
  
  release(&bcache.buckets[idx].lock);
}

void
bpin(struct buf *b) {
  int idx = b->blockno % NBUCKET;
  acquire(&bcache.buckets[idx].lock);
  b->refcnt++;
  release(&bcache.buckets[idx].lock);
}

void
bunpin(struct buf *b) {
  int idx = b->blockno % NBUCKET;
  acquire(&bcache.buckets[idx].lock);
  b->refcnt--;
  release(&bcache.buckets[idx].lock);
}


