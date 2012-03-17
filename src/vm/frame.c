#include "vm/frame.h"

static struct hash frame_table; /* Frame table*/

struct frame
  {
    struct hash_elem hash_elem;
    void *addr;
  };

unsigned
frame_hash_func (const struct hash_elem *e, void *aux) {
  const struct frame *f = hash_entry (e, struct frame, hash_elem);
  return hash_bytes (&f->addr, sizeof &f->addr);
}

bool
frame_less_func (const struct hash_elem *a, const struct hash_elem *b,
                 void *aux)
{
  const struct frame *fa = hash_entry (a, struct frame, hash_elem);
  const struct frame *fb = hash_entry (b, struct frame, hash_elem);
  return fa->addr < fb->addr;
}

struct frame *
frame_lookup (const struct hash frame_hash, const void *address)
{
  struct frame f;
  struct hash_elem *e;

  f.addr = address;
  e = hash_find (&frame_hash, &f.hash_elem);
  return e != NULL ? hash_entry (e, struct frame, hash_elem) : NULL;
}

