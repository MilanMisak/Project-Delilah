#include "swap.h"
#include <bitmap.h>
#include "devices/block.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

#define ENTRY_COUNT (BLOCK_SECTOR_SIZE * \
                       block_size (block_get_role (BLOCK_SWAP)) / \
                       PGSIZE)

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

/* Bitmap to record which indices in swap table are busy. */
static struct bitmap *used_map;

/* Block to represent swap partition. */
static struct block *swap_device;

/* Initializes the swap table. */
void
swap_init (void)
{
  used_map = bitmap_create (ENTRY_COUNT);
  swap_device = block_get_role (BLOCK_SWAP);
}

size_t
swap_write_page (struct page *page)
{
  size_t index = bitmap_scan_and_flip (used_map, 0, 1, false);
  if (index == BITMAP_ERROR)
    {
      PANIC("swap partition is full");
    }

  block_sector_t sector = index * SECTORS_PER_PAGE;
  void *buffer = page->uaddr;
  unsigned int i;
  for (i = 0; i < SECTORS_PER_PAGE; i++) 
    {
      block_write (swap_device, sector, buffer);
      buffer += 512;
    } 
  return index;
}

void
swap_read_page (struct page *page)
{
  printf ("Start of swap_read_page\n");
  bitmap_flip (used_map, page->saddr);
  printf ("Bitmap flipped\n");
  printf ("uaddr: %p", page->uaddr);
  void *buffer = page->uaddr;
  unsigned int i;
  printf ("Before reading loop\n");
  for (i = 0; i < SECTORS_PER_PAGE; i++)
    {
      printf ("Loop top\n");
      block_read (swap_device, (page->saddr * SECTORS_PER_PAGE), buffer);
      buffer += 512;
      printf ("Loop bottom\n");
    }
  page->saddr = -1;
  printf ("After reading loop\n");
}

void
swap_remove_page (struct page *page)
{
  //bitmap_flip (&used_map, page->saddr);
  //TODO: freeing?
}


