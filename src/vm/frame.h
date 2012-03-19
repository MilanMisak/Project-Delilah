#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>

struct frame
  {
    void *addr;                 /* Kernel virtual address of the page. */
    void *uaddr;                /* Virtual address of the page. */
    struct thread *owner;       /* Owner of the frame. */
    struct hash_elem hash_elem; /* Hash element in frame table. */
  };

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

struct frame* frame_remove (void *);

#endif
