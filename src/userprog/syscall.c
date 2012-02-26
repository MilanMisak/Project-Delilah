#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

static int get_integer_argument(int n, void *esp);
static void *get_pointer_argument(int n, void * esp);

static void h_halt (void *esp, uint32_t *return_value);
static void h_exit (void *esp, uint32_t *return_value);
static void h_exec (void *esp, uint32_t *return_value);
static void h_wait (void *esp, uint32_t *return_value);
static void h_create (void *esp, uint32_t *return_value);
static void h_remove (void *esp, uint32_t *return_value);
static void h_open (void *esp, uint32_t *return_value);
static void h_filesize (void *esp, uint32_t *return_value);
static void h_read (void *esp, uint32_t *return_value);
static void h_write (void *esp, uint32_t *return_value);
static void h_seek (void *esp, uint32_t *return_value);
static void h_tell (void *esp, uint32_t *return_value);
static void h_close (void *esp, uint32_t *return_value);

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
  printf ("system call!\n");

  /* Get ESP and EAX from the interrupt frame. */
  void *esp = f->esp;
  uint32_t *eax = &(f->eax);
  
  /* Get the system call number from the stack. */
  int syscall_number;
  asm ("movl %1, %0" : "=m" (syscall_number) : "r" (esp));

  /* Call the system call handler. */
  (*handlers[syscall_number]) (esp, eax);
}

static int
get_integer_argument(int n, void *esp)
{
  int arg;
  //TODO - remove or make work using memory syntax
  //asm ("movl %1(%2), %0" : "=r" (arg) : "i" (n), "r" (e));
  asm ("movl %1, %0" : "=r" (arg) : "r" (esp + 4 * n));
  return arg;
}

static void
*get_pointer_argument(int n, void *esp)
{
  void *arg;
  asm ("movl %1, %0" : "=r" (arg) : "r" (esp + 4 * n));
  return arg;
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

  /* Set EAX to status. */
  asm ("movl %0, %%eax" : : "a" (status));

  //TODO - do we need to free stuff?

  thread_exit ();
}

/* The exec system call handler. */
static void
h_exec (void *esp, uint32_t *return_value)
{
  char *cmd_line = get_pointer_argument (1, esp);

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
