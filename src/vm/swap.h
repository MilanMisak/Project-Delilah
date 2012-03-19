#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "vm/page.h"

void swap_init (void);

void swap_write_page (struct page *page);

void swap_read_page (struct page *page);

#endif
