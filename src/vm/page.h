#include <stdbool.h>
#include <stdint.h>
//#include "filesys/off_t.h"


struct page
{
 uint8_t *uaddr;   /* Address of the page in user virtual memory. */
 uint32_t *saddr;  /* Address of the page in swap. */
 const char *name; /* Name of the page if stored in filesys. */
 bool write;       /* Boolean to indicate read/write permissions. */
};

/* Called when there is a page fault to load the relevant page back into
   memory. */
void page_load(uint32_t *pd, struct page *upage);

/* Loads a page from the swap partition into memory */
void page_swap_load(struct page *upage, void *kpage);

/* Loads a page from the file system into memory */
void page_filesys_load(struct page *upage, void *kpage);
