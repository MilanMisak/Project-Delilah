#include "swap.h"
#include <bitmap.h>
#include "devices/block.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "vm/frame.h"

#define ENTRY_COUNT (BLOCK_SECTOR_SIZE * \
                       block_size (block_get_role (BLOCK_SWAP)) / \
                       PGSIZE)

/* Bitmap to record which indices in swap table are busy. */
static struct bitmap *used_map;

/* Initializes the swap table. */
void
swap_init (void)
{
  used_map = bitmap_create (ENTRY_COUNT);
}


void
swap_write_frame (struct frame *frame)
{
  size_t index = bitmap_scan_and_flip (used_map, 0, 1, false);
  if (index == BITMAP_ERROR)
    {
      //TODO - error case
    }

}
