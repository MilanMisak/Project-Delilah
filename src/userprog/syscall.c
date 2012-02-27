#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "lib/kernel/console.h"
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

static int *get_argument(int n, void *esp);
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
static int
*get_argument (int n, void *esp)
{
  int *arg = (int *) esp + n;
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
  shutdown_power_off ();
}

/* The exit system call handler. */
static void
h_exit (void *esp, uint32_t *return_value)
{
  int status = *get_argument (1, esp);
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
  /* Get FILE and INITIAL_SIZE from the stack. */
  char *file = (char *) get_argument (1, esp);
  //TODO - check file is safe
  int initial_size = *get_argument (2, esp);

  /* Return TRUE if file gets created, FALSE otherwise. */
  *return_value = filesys_create (file, initial_size);
}

/* The remove system call. */
static void
h_remove (void *esp, uint32_t *return_value)
{
  /* Get FILE from the stack. */
  char *file = (char *) get_argument (1, esp);
  //TODO - check file is safe

  /* Return TRUE if file gets removed, FALSE otherwise. */
  *return_value = filesys_remove (file);
}

/* The open system call. */
static void
h_open (void *esp, uint32_t *return_value)
{
  /* Get FILE from the stack. */
  char *file = (char *) get_argument (1, esp);
  //TODO - check file is safe

  struct file *opened_file = filesys_open (file);
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
  char *buffer = (char *) get_argument (2, esp);
  int size = *get_argument (3, esp);

  printf("FD: %i %i: \n", fd, size);
  printf(buffer);

  //TODO - check for safe memory of buffer

  if (fd == STDIN_FILENO)
  {
    //TODO - error - reading from stdin
  }
  else if (fd == STDOUT_FILENO)
  {
    //TODO - break up larger buffers?
    lock_acquire (&filesys_lock);
    /*putbuf (&("BOOM\n"), 5);
    char *a = "" + size;
    putbuf (&a, 1);*/
    //putbuf (buffer, size);
    lock_release (&filesys_lock);

    *return_value = size;
  }
  else
  {
    //TODO - writing to files
    putbuf (&("BALLS"), 5);
    //char *a = "" + fd;
    //putbuf (&a, 1);
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
