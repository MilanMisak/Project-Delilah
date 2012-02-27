#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "lib/kernel/console.h"
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

static void *get_argument(int n, void *esp);
static uint32_t get_integer_argument(int n, void *esp);

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
static handler (handlers[3]) = {&h_halt, &h_exit, &h_exec};

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  /* Get ESP and EAX from the interrupt frame. */
  void *esp = f->esp;
  uint32_t *eax = &(f->eax);
  
  /* Get the system call number from the stack. */
  //TODO - need to check that ESP is valid before dereferencing
  uint32_t syscall_number = *((uint32_t *) esp);

  /* Call the system call handler. */
  (*handlers[syscall_number]) (esp, eax);
}

/* Returns Nth argument (1-based) to the syscall handler from the stack. */
static void
*get_argument (int n, void *esp)
{
  void *arg = esp + 4 * n;
  // TODO - need to check that the pointer is valid and safe
  return arg;
}

/* Returns Nth argument to the syscall handler and casts it to integer. */
static uint32_t
get_integer_argument (int n, void *esp)
{
  void *arg = get_argument (n, esp);
  return *((uint32_t *) arg);
}

/* The halt system call handler. */
static void
h_halt (void *esp, uint32_t *return_value)
{
  shutdown_power_off ();
}

/* The exit system call handler. */
static void
h_exit (void *esp, uint32_t *return_value)
{
  int status = get_integer_argument (1, esp);
  *return_value = status;

  //TODO - do we need to free stuff? like release locks and such

  thread_exit ();
}

/* The exec system call handler. */
static void
h_exec (void *esp, uint32_t *return_value)
{
  //TODO - exec SC
  thread_exit ();
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
  //TODO - create SC
}

/* The remove system call. */
static void
h_remove (void *esp, uint32_t *return_value)
{
  //TODO - remove SC
}

/* The open system call. */
static void
h_open (void *esp, uint32_t *return_value)
{
  //TODO - open SC
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
  //TODO - write SC

  /* Get FD, BUFFER and SIZE from the stack. */
  uint32_t fd = get_integer_argument (esp, 1);
  char *buffer = (char *) get_argument (esp, 2);
  uint32_t size = get_integer_argument (esp, 3);

  //TODO - check for safe memory of buffer

  if (fd == STDIN_FILENO)
  {
    //TODO - error - reading from stdin
  }
  else if (fd == STDOUT_FILENO)
  {
    //TODO - break up larger buffers
    putbuf (buffer, size);

    *return_value = size;
  }
  else
  {
    //TODO - writing to files
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
  //TODO - close SC
}
