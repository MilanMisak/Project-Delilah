#include "vm/page.h"
#include <debug.h>
#include <stddef.h>
#include "devices/block.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include "threads/palloc.h"
#include "userprog/process.h"

/* Loads a page from the file system into memory */
void page_filesys_load (struct page *upage, void *kpage);

void
page_load (struct page *upage)
{
  /*TODO - Get kpage.*/
  void *kpage = palloc_get_page (PAL_USER);
  install_page (upage, kpage, upage->write);

  /* Load the page into memory again.*/
  if (upage->saddr == -1)
    swap_read_page (upage);
    //page_swap_load (upage, kpage);
  else
    page_filesys_load (upage, kpage);
}

void
page_write (uint32_t *pd UNUSED, struct page *upage)
{
  //TODO - remove page from thread's page table or something
  if (upage->saddr != -1)
    swap_write_page (upage);
}

void
page_filesys_load (struct page *upage UNUSED, void *kpage UNUSED)
{
  //TODO - page_filesys_load
}

struct page *
page_lookup (struct hash *page_table, void *uaddr)
{
  struct page p;
  struct hash_elem *e;

  p.uaddr = uaddr;
  e = hash_find (page_table, &p.hash_elem);
  return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

unsigned
page_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  const struct page *p = hash_entry (e, struct page, hash_elem);
  return hash_bytes (&p->uaddr, sizeof &p->uaddr);
}

bool
page_less_func (const struct hash_elem *a, const struct hash_elem *b,
                void *aux UNUSED)
{
  const struct page *pa = hash_entry (a, struct page, hash_elem);
  const struct page *pb = hash_entry (b, struct page, hash_elem);
  return pa->uaddr < pb->uaddr;
}
