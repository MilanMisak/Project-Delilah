#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <stdbool.h>
#include <stdint.h>
#include "devices/block.h"
#include "filesys/off_t.h"
#include "threads/synch.h"
#include "vm/frame.h"


struct page
  {
    uint8_t *uaddr;             /* Page address in user virtual memory. */
    int16_t saddr;              /* Index of the swap slot. */
    struct file *file;          /* File to lazily load the page from. */
    int16_t file_start_pos;     /* Starting position in the file to read
                                   the page from. */
    uint16_t file_read_bytes;   /* How many bytes to read from the file. */
    bool write;                 /* Indication of read/write permissions. */
    struct lock *access_lock; //TODO - comment
    struct hash_elem hash_elem; /* Hash elem for a supplemental page table. */
  };

/* Called when there is a page fault to load the relevant page back into
   memory. */
bool page_load (struct page *upage, void *fault_addr);

/* Writes page at address ADDR to a memory-mapped file of length FILE_SIZE. */
void page_write_to_mapped_file (struct file *file, void *addr, int file_size);

/* Looks up a page in PAGE_TABLE specified by the virtual address
   in UADDR. Returns NULL if no page is found. */
struct page * page_lookup (struct hash *page_table, void *uaddr);

unsigned page_hash_func (const struct hash_elem *e, void *aux);

bool page_less_func (const struct hash_elem *a, const struct hash_elem *b,
                      void *aux);

#endif
