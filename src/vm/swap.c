#include "swap.h"
#include <bitmap.h>
#include "devices/block.h"

static struct bitmap *used_map;

/* Initializes the swap table. */
void
swap_init (void)
{
  used_map = bitmap_create (BLOCK_SECTOR_SIZE * block_size (block_get_role (BLOCK_SWAP)));
}
