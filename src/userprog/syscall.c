#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "lib/kernel/console.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);

static void kill_process (void);
static int *get_argument (int n, void *esp);

static void h_halt     (void *esp, uint32_t *return_value);
static void h_exit     (void *esp, uint32_t *return_value);
static void h_exec     (void *esp, uint32_t *return_value);
static void h_wait     (void *esp, uint32_t *return_value);
static void h_create   (void *esp, uint32_t *return_value);
static void h_remove   (void *esp, uint32_t *return_value);
static void h_open     (void *esp, uint32_t *return_value);
static void h_filesize (void *esp, uint32_t *return_value);
static void h_read     (void *esp, uint32_t *return_value);
static void h_write    (void *esp, uint32_t *return_value);
static void h_seek     (void *esp, uint32_t *return_value);
static void h_tell     (void *esp, uint32_t *return_value);
static void h_close    (void *esp, uint32_t *return_value);

/* System call handlers array. */
typedef void (*handler) (void *esp, uint32_t *return_value);
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
  printf("HANDLER BOOOOM");
  /* Get ESP and EAX from the interrupt frame. */
  void *esp = f->esp;
  uint32_t *eax = &(f->eax);
  
  /* Get the system call number from the stack. */
  //TODO - need to check that ESP is valid before dereferencing
  int syscall_number = *((int *) esp);

  /* Call the system call handler. */
  (*handlers[syscall_number]) (esp, eax);
}

/* Kills the current process. */
static void
kill_process (void)
{
  //TODO - release locks and such

  /* Close files open by this process. */
  struct thread *current = thread_current ();
  struct list_elem *e;
  while ((e = list_begin (&current->open_files)) != NULL)
    {
      struct open_file *of = list_entry (e, struct open_file, elem);
      //TODO - close file
    }

  thread_exit ();
}

/* Returns Nth argument (1-based) to the syscall handler from the stack. */
static int
*get_argument (int n, void *esp)
{
  int *arg = (int *) esp + n;
  // TODO - need to check that the pointer is valid and safe
  return arg;
}

/*Reads a byte at user virtual address UADDR.
  UADDR must be below PHYS_BASE.
  Returns the byte value if successful, -1 if a segfault
  occurred. */
static int
get_user (const uint8_t *uaddr) {
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occured. */
static bool
put_user (uint8_t *udst, uint8_t byte) {
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
      : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

/* The halt system call handler. */
static void
h_halt (void *esp, uint32_t *return_value)
{
  printf ("HALTBOOM");
  shutdown_power_off ();
}

/* The exit system call handler. */
static void
h_exit (void *esp, uint32_t *return_value)
{
  int status = *get_argument (1, esp);
  *return_value = status;
  
  thread_current ()->child->exitStatus = status;
  /* TODO - free all the children */

  kill_process ();
}

/* The exec system call handler. */
static void
h_exec (void *esp, uint32_t *return_value)
{
  /* Get CMD_LINE from the stack. */
  char *cmd_line = (char *) *get_argument (1, esp);
  //TODO - check that cmd_line is safe

  lock_acquire (&filesys_lock);
  tid_t tid = process_execute (cmd_line);
  lock_release (&filesys_lock);

  *return_value = tid;
  if (tid == TID_ERROR)
  {
    /* Error: process cannot be executed. */
    *return_value = -1;
  }
}

/* The wait system call handler. */
static void
h_wait (void *esp, uint32_t *return_value)
{
  //TODO - wait SC
}

/* The create system call. */
static void
h_create (void *esp, uint32_t *return_value)
{
  /* Get FILE and INITIAL_SIZE from the stack. */
  char *file = (char *) *get_argument (1, esp);
  //TODO - check file is safe
  int initial_size = *get_argument (2, esp);

  /* Return TRUE if file gets created, FALSE otherwise. */
  lock_acquire (&filesys_lock);
  *return_value = filesys_create (file, initial_size);
  lock_release (&filesys_lock);
}

/* The remove system call. */
static void
h_remove (void *esp, uint32_t *return_value)
{
  /* Get FILE from the stack. */
  char *file = (char *) *get_argument (1, esp);
  //TODO - check file is safe

  /* Return TRUE if file gets removed, FALSE otherwise. */
  lock_acquire (&filesys_lock);
  *return_value = filesys_remove (file);
  lock_release (&filesys_lock);
}

/* The open system call. */
static void
h_open (void *esp, uint32_t *return_value)
{
  /* Get FILE from the stack. */
  char *file = (char *) *get_argument (1, esp);
  //TODO - check file is safe
 
  /* Try opening the file. */
  lock_acquire (&filesys_lock);
  struct file *opened_file = filesys_open (file);
  lock_release (&filesys_lock);

  if (opened_file == NULL)
  {
    *return_value = -1;
  }
  else
  {
    //TODO - store the struct file somewhere?
    
    
    *return_value = 5; //TODO - an actual fd needed here
  }
}

/* The filesize system call. */
static void
h_filesize (void *esp, uint32_t *return_value)
{
  //TODO - filesize SC
}

/* The read system call. */
static void
h_read (void *esp, uint32_t *return_value)
{
  //TODO - read SC
}

/* The write system call. */
static void
h_write (void *esp, uint32_t *return_value)
{
  /* Get FD, BUFFER and SIZE from the stack. */
  int fd = *get_argument (1, esp);
  char *buffer = (char *) *get_argument (2, esp);
  int size = *get_argument (3, esp);

  //TODO - check for safe memory of buffer

  if (fd == STDIN_FILENO)
  {
    /* Trying to read from standard input = error -> kill the process. */
    kill_process ();
  }
  else if (fd == STDOUT_FILENO)
  {
    //TODO - break up larger buffers?
    lock_acquire (&filesys_lock);
    putbuf (buffer, size);
    lock_release (&filesys_lock);

    *return_value = size;
  }
  else
  {
    //TODO - writing to files
    putbuf (&("BALLS"), 5);
  }
}

/* The seek system call. */
static void
h_seek (void *esp, uint32_t *return_value)
{
  //TODO - seek SC
}

/* The tell system call. */
static void
h_tell (void *esp, uint32_t *return_value)
{
  //TODO - tell SC
}

/* The close system call. */
static void
h_close (void *esp, uint32_t *return_value)
{
  int fd = *get_argument (1, esp);
  //TODO - check that fd is safe
  thread_close_open_file (fd);
}
