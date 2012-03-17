#include <stddef.h>
#include "vm/page.h"
#include "userprog/pagedir.h"

void
page_load(uint32_t *pd, struct page *upage)
{
  /*TODO - Get this using a frame table function.*/
  void *kpage;

  /* Add the page back into the process' page directory */
  pagedir_set_page(pd, (void *) upage->uaddr, kpage, upage->write);

  /* Load the page into memory again.*/
  if(upage->saddr == NULL)
    page_swap_load(upage, kpage);
  else
    page_filesys_load(upage, kpage);

}

void
page_swap_load(struct page *upage, void *kpage)
{
}

void
page_filesys_load(struct page *upage, void *kpage)
{
}
