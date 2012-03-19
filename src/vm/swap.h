#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "vm/frame.h"

void swap_init (void);
void swap_write_frame (struct frame *frame);

#endif
