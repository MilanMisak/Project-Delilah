//#ifndef VM_FRAME_H
//#define VM_FRAME_H

#include "lib/kernel/hash.h"

unsigned frame_hash_func (const struct hash_elem *e, void *aux);

bool frame_less_func (const struct hash_elem *a, const struct hash_elem *b,
                      void *aux);

struct frame *frame_lookup (const struct hash frame_hash, const void *address);

//#endif
