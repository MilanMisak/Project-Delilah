#include "vm/frame.h"
#include <debug.h>
#include <stdio.h>
#include "threads/malloc.h"
#include "threads/thread.h"

//TODO - Destroy the frame table (free memory) on exit;

static struct hash frame_table; /* Frame table*/

void
frame_init (void)
{
  hash_init (&frame_table, &frame_hash_func, &frame_less_func, NULL);
}

unsigned
frame_hash_func (const struct hash_elem *e, void *aux UNUSED) 
{
  const struct frame *f = hash_entry (e, struct frame, hash_elem);
  return hash_bytes (&f->addr, sizeof &f->addr);
}

bool
frame_less_func (const struct hash_elem *a, const struct hash_elem *b,
                 void *aux UNUSED)
{
  const struct frame *fa = hash_entry (a, struct frame, hash_elem);
  const struct frame *fb = hash_entry (b, struct frame, hash_elem);
  return fa->addr < fb->addr;
}

struct frame *
frame_lookup (void *addr)
{
  struct frame f;
  struct hash_elem *e;

  f.addr = addr;
  e = hash_find (&frame_table, &f.hash_elem);
  return e != NULL ? hash_entry (e, struct frame, hash_elem) : NULL;
};

void
frame_insert (void *faddr, void *uaddr, bool write)
{
  struct frame *f = malloc (sizeof (struct frame));
  f->addr = faddr;
  f->uaddr = uaddr;
  f->write = write;
  f->owner = thread_current ();
  
  hash_insert (&frame_table, &f->hash_elem);
}

struct frame *
frame_remove (void *kpage)
{
  struct frame *removing = frame_lookup (kpage);
  hash_delete (&frame_table,&removing->hash_elem);
  return removing;
}
