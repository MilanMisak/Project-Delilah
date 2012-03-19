#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>

/* Initializes the frame table. */
void frame_init (void);

/* Hash function for frames. */
unsigned frame_hash_func (const struct hash_elem *e, void *aux);

/* Function for ordering frames. */
bool frame_less_func (const struct hash_elem *a, const struct hash_elem *b,
                      void *aux);

/* Returns a frame when given a frame kernel virtual address. */
struct frame *frame_lookup (void *addr);

/* Given a frame virtual address, makes a frame struct and inserts it into the table. */
void frame_insert (void *faddr, void *uaddr);

#endif
