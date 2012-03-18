#include "vm/page.h"
#include <debug.h>
#include <stddef.h>
#include "userprog/pagedir.h"

void
page_load(uint32_t *pd, struct page *upage)
{
  /*TODO - Get this using a frame table function.*/
  void *kpage;

  /* Add the page back into the process' page directory */
  pagedir_set_page (pd, (void *) upage->uaddr, kpage, upage->write);

  /* Load the page into memory again.*/
  if(upage->saddr == NULL)
    page_swap_load (upage, kpage);
  else
    page_filesys_load (upage, kpage);

}

void
page_write (uint32_t *pd UNUSED, struct page *upage UNUSED)
{
}

void
page_swap_load (struct page *upage UNUSED, void *kpage UNUSED)
{
}

void
page_swap_write (struct page *upage UNUSED)
{
}

void
page_filesys_load (struct page *upage UNUSED, void *kpage UNUSED)
{
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

struct page *
page_lookup (struct hash *page_table, void *uaddr)
{
  struct page p;
  struct hash_elem *e;

  p.uaddr = uaddr;
  e = hash_find (page_table, &p.hash_elem);
  return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}
