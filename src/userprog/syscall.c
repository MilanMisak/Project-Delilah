#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/kernel/console.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);

static void kill_process (void);
static int *get_argument (int n, void *esp);

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

/* System call handlers array. */
typedef void (*handler) (struct intr_frame *f);
static handler (handlers[13]) = {&h_halt, &h_exit, &h_exec, &h_wait, &h_create,
                                 &h_remove, &h_open, &h_filesize, &h_read,
                                 &h_write, &h_seek, &h_tell, &h_close};

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
  //TODO - need to check that ESP is valid before dereferencing
  int syscall_number = *((int *) f->esp);

  /* Call the system call handler. */
  (*handlers[syscall_number]) (f);
}

/* Kills the current process. */
static void
kill_process (void)
{
  //TODO - release locks and such

  /* Close files open by this process. */
  struct thread *current = thread_current ();
  struct list_elem *e;
  for (e = list_begin (&current->open_files);
       e != list_end (&current->open_files); e = list_next (e))
    {
      struct open_file *of = list_entry (e, struct open_file, elem);
      //TODO - close file
    }

  printf ("%s: exit(%d)\n", current->name, current->child->exitStatus);
  thread_exit ();
}

/* Returns Nth argument (1-based) to the syscall handler from the stack. */
static int
*get_argument (int n, void *esp)
{
  int *arg = (int *) esp + n;
  if (get_user ((uint8_t *) arg) == -1)
    kill_process ();
  // TODO - need to check that the pointer is valid and safe
  return arg;
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
h_halt (struct intr_frame *f)
{
  shutdown_power_off ();
}

/* The exit system call handler. */
static void
h_exit (struct intr_frame *f)
{
  int status = *get_argument (1, f->esp);
  f->eax = status;
  
  thread_current ()->child->exitStatus = status;
  /* TODO - free all the children */

  kill_process ();
}

/* The exec system call handler. */
static void
h_exec (struct intr_frame *f)
{
  /* Get CMD_LINE from the stack. */
  char *cmd_line = (char *) *get_argument (1, f->esp);
  //TODO - check that cmd_line is safe

  lock_acquire (&filesys_lock);
  tid_t tid = process_execute (cmd_line);
  lock_release (&filesys_lock);

  f->eax = tid;
  if (tid == TID_ERROR)
  {
    /* Error: process cannot be executed. */
    f->eax = -1;
  }
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
  //TODO - check file is safe
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
  //TODO - check file is safe

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
  //TODO - check file is safe
 
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
  if (buffer == NULL)
    {
      /* Error: BUFFER cannot be NULL. */
      kill_process ();
    }
  int size = *get_argument (3, f->esp);

  if (fd == STDIN_FILENO)
    {
//TODO - read
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
//TODO - read
    }
}

/* The write system call. */
static void
h_write (struct intr_frame *f)
{
  /* Get FD, BUFFER and SIZE from the stack. */
  int fd = *get_argument (1, f->esp);
  char *buffer = (char *) *get_argument (2, f->esp);
  if (buffer == NULL)
    {
      /* Error: BUFFER cannot be NULL. */
      kill_process ();
    }
  int size = *get_argument (3, f->esp);

  //TODO - check for safe memory of buffer

  if (fd == STDIN_FILENO)
    {
      /* Trying to write to standard input = error -> kill the process. */
      kill_process ();
    }
  else if (fd == STDOUT_FILENO)
    {
      int buffer_max_length = 256;

      lock_acquire (&filesys_lock);
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
      lock_release (&filesys_lock);
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
    //TODO - writing to files
    }
}

/* The seek system call. */
static void
h_seek (struct intr_frame *f)
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
  //TODO - seek SC
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
  //TODO - tell SC
}

/* The close system call. */
static void
h_close (struct intr_frame *f)
{
  int fd = *get_argument (1, f->esp);
  //TODO - check that fd is safe
  thread_close_open_file (fd);
}
