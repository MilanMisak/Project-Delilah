#ifndef VM_SWAP_H
#define VM_SWAP_H

/* An entry for the swap table. */
struct swap_table_entry
  {
    int start_sector_index;   /* Index of a sector at which entry starts. */
    int offset;               /* Offset inside the sector. */
  };

void swap_init (void);

#endif
