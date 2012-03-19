#include "swap.h"
#include <bitmap.h>
#include "devices/block.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

#define ENTRY_COUNT (BLOCK_SECTOR_SIZE * \
                       block_size (block_get_role (BLOCK_SWAP)) / \
                       PGSIZE)

/* Bitmap to record which indices in swap table are busy. */
static struct bitmap *used_map;

static struct swap_table_entry **swap_table;

/* Initializes the swap table. */
void
swap_init (void)
{
  used_map = bitmap_create (ENTRY_COUNT);
  swap_table = (struct swap_table_entry **)
                   malloc (ENTRY_COUNT * sizeof (struct swap_table_entry));
}
