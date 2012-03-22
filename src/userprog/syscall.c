#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/kernel/console.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "vm/page.h"

static void syscall_handler (struct intr_frame *);

static void kill_process (void);

static int *get_argument (int n, void *esp);
static bool is_valid_buffer (char *buffer, int size);
static int get_user (const uint8_t *uaddr);

static void h_halt     (struct intr_frame *f);
static void h_exit     (struct intr_frame *f);
static void h_exec     (struct intr_frame *f);
static void h_wait     (struct intr_frame *f);
static void h_create   (struct intr_frame *f);
static void h_remove   (struct intr_frame *f);
static void h_open     (struct intr_frame *f);
static void h_filesize (struct intr_frame *f);
static void h_read     (struct intr_frame *f);
static void h_write    (struct intr_frame *f);
static void h_seek     (struct intr_frame *f);
static void h_tell     (struct intr_frame *f);
static void h_close    (struct intr_frame *f);

static void h_mmap     (struct intr_frame *f);
static void h_munmap   (struct intr_frame *f);

void page_set_evictable (uint8_t *, bool);

/* System call handlers array. */
typedef void (*handler) (struct intr_frame *f);
static handler (handlers[15]) = {&h_halt, &h_exit, &h_exec, &h_wait, &h_create,
                                 &h_remove, &h_open, &h_filesize, &h_read,
                                 &h_write, &h_seek, &h_tell, &h_close,
                                 &h_mmap, &h_munmap};

static struct lock filesys_lock;

void
syscall_init (void) 
{
  lock_init (&filesys_lock);

  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  /* Get the system call number from the stack. */
  int syscall_number = get_user (f->esp);

  /* Call the system call handler. */
  (*handlers[syscall_number]) (f);
}

/* Kills the current process. */
static void
kill_process (void)
{
  struct thread *current = thread_current ();

  printf ("%s: exit(%d)\n", current->name, current->child->exit_status);
  thread_exit ();
}

/* Returns Nth argument (1-based) to the syscall handler from the stack. */
static int
*get_argument (int n, void *esp)
{
  int *arg = (int *) esp + n;
  if (get_user ((uint8_t *) arg) == -1)
    kill_process ();
  return arg;
}

/* Checks that BUFFER is valid and safe. */
static bool
is_valid_buffer (char *buffer, int size)
{
  return (get_user ((uint8_t *) buffer) != -1
      && get_user ((uint8_t *) buffer + size - 1) != -1);
}

/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  if (! is_user_vaddr ((void *) uaddr))
    return -1;

  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occured. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
      : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

/* The halt system call handler. */
static void
h_halt (struct intr_frame *f UNUSED)
{
  shutdown_power_off ();
}

/* The exit system call handler. */
static void
h_exit (struct intr_frame *f)
{
  int status = *get_argument (1, f->esp);
  f->eax = status;
  
  thread_current ()->child->exit_status = status;

  kill_process ();
}

/* The exec system call handler. */
static void
h_exec (struct intr_frame *f)
{
  /* Get CMD_LINE from the stack. */
  char *cmd_line = (char *) *get_argument (1, f->esp);

  lock_acquire (&filesys_lock);
  tid_t tid = process_execute (cmd_line);
  lock_release (&filesys_lock);

  f->eax = tid;
  if (tid == TID_ERROR)
    {
      /* Process cannot be executed. */
      f->eax = -1;
      return;
    }

  /* Wait for the new process to finish loading. */
  struct thread *current = thread_current ();
  struct list_elem *e;

  for (e = list_begin (&current->children);
       e != list_end (&current->children); e = list_next (e))
    {
      struct child *child = list_entry (e, struct child, elem);
      if (child->tid == tid)
        {
          sema_down (&child->loading_sema);
          if (! child->loaded_correctly)
            {
              /* Child process didn't load correctly. */
              f->eax = -1;
            }
          return;
        }
    }
      
  /* Child process with this TID not found. */
  f->eax = -1;
}

/* The wait system call handler. */
static void
h_wait (struct intr_frame *f)
{
  /* Get PID from the stack. */
  int pid = *get_argument (1, f->esp);

  int exit_status = process_wait (pid);
  f->eax = exit_status;
}

/* The create system call. */
static void
h_create (struct intr_frame *f)
{
  /* Get FILE and INITIAL_SIZE from the stack. */
  char *file = (char *) *get_argument (1, f->esp);
  if (file == NULL)
    {
      /* Error: FILE cannot be NULL. */
      kill_process ();
    }
  int initial_size = *get_argument (2, f->esp);

  /* Return TRUE if file gets created, FALSE otherwise. */
  lock_acquire (&filesys_lock);
  f->eax = filesys_create (file, initial_size);
  lock_release (&filesys_lock);
}

/* The remove system call. */
static void
h_remove (struct intr_frame *f)
{
  /* Get FILE from the stack. */
  char *file = (char *) *get_argument (1, f->esp);
  if (file == NULL)
    {
      /* Error: FILE cannot be NULL. */
      kill_process ();
    }

  /* Return TRUE if file gets removed, FALSE otherwise. */
  lock_acquire (&filesys_lock);
  f->eax = filesys_remove (file);
  lock_release (&filesys_lock);
}

/* The open system call. */
static void
h_open (struct intr_frame *f)
{
  /* Get FILE from the stack. */
  char *file = (char *) *get_argument (1, f->esp);
  if (file == NULL)
    {
      /* Error: FILE cannot be NULL. */
      kill_process ();
    }
 
  /* Try opening the file. */
  lock_acquire (&filesys_lock);
  struct file *opened_file = filesys_open (file);
  lock_release (&filesys_lock);

  if (opened_file == NULL)
    {
      /* File could not be opened. */
      f->eax = -1;
    }
  else
    {
      int fd = thread_add_open_file (opened_file);
      f->eax = fd;
    }
}

/* The filesize system call. */
static void
h_filesize (struct intr_frame *f)
{
  /* Get FD from the stack. */
  int fd = *get_argument (1, f->esp);

  /* Get FILE with given FD from the current thread. */
  struct file *file = thread_get_open_file (fd);
  if (file == NULL)
    {
      /* Error: could not find the open file with given FD. */
      kill_process ();
    }

  /* Get file size in bytes. */
  lock_acquire (&filesys_lock);
  int size = file_length (file);
  lock_release (&filesys_lock);

  f->eax = size;
}

/* The read system call. */
static void
h_read (struct intr_frame *f)
{
  /* Get FD, BUFFER and SIZE from the stack. */
  int fd = *get_argument (1, f->esp);
  char *buffer = (char *) *get_argument (2, f->esp);
  int size = *get_argument (3, f->esp);

  if (buffer == NULL || ! is_valid_buffer (buffer, size))
    {
      /* Error: BUFFER is invalid. */
      kill_process ();
    }

  if (fd == STDIN_FILENO)
    {
      int i;
      for (i = 0; i < size; i++)
        {
          *(buffer + i) = input_getc ();
        }

      f->eax = size;
    }
  else if (fd == STDOUT_FILENO)
    {
      /* Trying to read from standard output = error -> kill the process. */
      kill_process ();
    }
  else
    {
      /* Get FILE with given FD from the current thread. */
      struct file *file = thread_get_open_file (fd);
      if (file == NULL)
        {
          /* Error: could not find the open file with given FD. */
          kill_process ();
        }

      /* Try to read SIZE bytes from FILE to BUFFER. */
      lock_acquire (&filesys_lock);
      int bytes_read = file_read (file, buffer, size);
      lock_release (&filesys_lock);

      /* Return how many bytes were actually read. */
      f->eax = bytes_read;
    }
}

/* The write system call. */
static void
h_write (struct intr_frame *f)
{
  /* Get FD, BUFFER and SIZE from the stack. */
  int fd = *get_argument (1, f->esp);
  char *buffer = (char *) *get_argument (2, f->esp);
  int size = *get_argument (3, f->esp);
  
  if (buffer == NULL || ! is_valid_buffer (buffer, size))
    {
      /* Error: BUFFER is invalid. */
      kill_process ();
    }

  if (fd == STDIN_FILENO)
    {
      /* Trying to write to standard input = error -> kill the process. */
      kill_process ();
    }
  else if (fd == STDOUT_FILENO)
    {
      int buffer_max_length = 256;

      if (size <= buffer_max_length)
        {
          putbuf (buffer, size);
          f->eax = size;
        }
      else
        {
          /* Break up larger buffers into chunks of 256 bytes. */
          int bytes_written = 0;
          while (size > buffer_max_length)
            {
              putbuf (buffer + bytes_written, buffer_max_length);
              size -= buffer_max_length;
              bytes_written += buffer_max_length;
            }
          putbuf (buffer + bytes_written, size);
          f->eax = bytes_written;
        }
    }
  else
    {
      /* Get FILE with given FD from the current thread. */
      struct file *file = thread_get_open_file (fd);
      if (file == NULL)
        {
          /* Error: could not find the open file with given FD. */
          kill_process ();
        }

      /* Try to write SIZE bytes from BUFFER to FILE. */
      lock_acquire (&filesys_lock);  
      int bytes_written = file_write (file, buffer, size);
      lock_release (&filesys_lock);  

      /* Return how many bytes were actually written. */
      f->eax = bytes_written;
    }
}

/* The seek system call. */
static void
h_seek (struct intr_frame *f)
{
  /* Get FD and POSITION from the stack. */
  int fd = *get_argument (1, f->esp);
  int position = *get_argument (2, f->esp);

  if (position < 0)
    {
      /* Error: seeking with a negative position. */
      kill_process ();
    }
  
  /* Get FILE with given FD from the current thread. */
  struct file *file = thread_get_open_file (fd);
  if (file == NULL)
    {
      /* Error: could not find the open file with given FD. */
      kill_process ();
    }

  lock_acquire (&filesys_lock);
  file_seek (file, position);
  lock_release (&filesys_lock);
}

/* The tell system call. */
static void
h_tell (struct intr_frame *f)
{
  /* Get FD, BUFFER and SIZE from the stack. */
  int fd = *get_argument (1, f->esp);
  
  /* Get FILE with given FD from the current thread. */
  struct file *file = thread_get_open_file (fd);
  if (file == NULL)
    {
      /* Error: could not find the open file with given FD. */
      kill_process ();
    }

  lock_acquire (&filesys_lock);
  int position = file_tell (file);
  lock_release (&filesys_lock);

  /* Return the position of the next byte to be read or written. */
  f->eax = position;
}

/* The close system call. */
static void
h_close (struct intr_frame *f)
{
  /* Get FD from the stack. */
  int fd = *get_argument (1, f->esp);
  
  lock_acquire (&filesys_lock);
  thread_close_open_file (fd);
  lock_release (&filesys_lock);
}

/* The mmap system call. */
static void
h_mmap (struct intr_frame *f)
{
  /* Get FD and ADDR from the stack. */
  int fd = *get_argument (1, f->esp);
  void *addr = (void *) *get_argument (2, f->esp);

  if (fd == STDIN_FILENO || fd == STDOUT_FILENO)
    {
      /* Error: console input and output are not mappable. */
      f->eax = -1;
      return;
    }
  int int_addr = (int) addr;
  if (int_addr == 0 || int_addr % PGSIZE != 0)
    {
      /* Error: Pintos assumes virtual address 0 is not mapped, also
         the given virtual address must be page aligned. */
      f->eax = -1;
      return;
    }

  struct file *open_file = thread_get_open_file (fd);
  if (open_file == NULL)
    {
      /* Error: invalid FD. */
      f->eax = -1;
      return;
    }
 
  /* Get file size. */
  lock_acquire (&filesys_lock);
  off_t file_size = file_length (open_file);
  lock_release (&filesys_lock);

  if (file_size == 0)
    {
      /* Error: file size is zero bytes. */
      f->eax = -1;
      return;
    }

  if (thread_collides_with_mapped_files (addr, file_size))
    {
      /* Error: virtual address space required by the potential new mapping
         collides with an existing mapping. */
      f->eax = -1;
      return;
    }

  //TODO - check if this works
  struct hash sup_page_table = thread_current ()->sup_page_table; 
  int i;
  bool collision = false;
  for (i = int_addr; i < int_addr + file_size; i += PGSIZE)
    {
      struct page *page = page_lookup (&sup_page_table, (void *) i); 
      if (page != NULL)
        {
          collision = true;
          break;
        }
    }

  if (collision)
    {
      /* Error: required virtual address space collides with existing
         data or code. */
      f->eax = -1;
      return;
    }
  
  /* Reopen the mapped file. */
  lock_acquire (&filesys_lock);
  struct file *mapped_file = file_reopen (open_file);
  lock_release (&filesys_lock);

  /* Add pages to the page table. */
  for (i = 0; i < file_size; i += PGSIZE)
    {
      struct page *page = malloc (sizeof (struct page));
      if (page == NULL)
      {
        //TODO - do something here?
        printf ("baaad\n");
      }

      page->uaddr = addr + i;
      page->saddr = -1;
      page->write = true;

      hash_insert (&thread_current ()->sup_page_table, &page->hash_elem);
    }
  
  /* Add file's mapping. */
  int mapping_id = thread_add_mapped_file (mapped_file, addr, file_size);

  /* Return the mapping ID. */
  f->eax = mapping_id;
}

/* The munmap system call. */
static void
h_munmap (struct intr_frame *f)
{
  /* Get MAPPING from the stack. */
  int mapping = *get_argument (1, f->esp);

  thread_remove_mapped_file (mapping);
}

void
page_set_evictable (uint8_t *uaddr, bool new_evictable)
{
  void *page_start = pg_round_down (uaddr);
  printf("\nuaddr: %p\n\n", uaddr);
  struct frame *f = frame_find_upage (page_start);
  if (f != NULL)
    frame_set_evictable (f, new_evictable);
  else
    printf ("\nwell that's not good (syscall.c:577)\n\n");
}

