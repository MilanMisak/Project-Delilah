#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/fixed-point.h"
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/timer.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
/* This list is ordered by priority */
static struct list ready_list;

/* Number of processes on the ready list. */
static int ready_count;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* List of processes which are sleeping after the call to
   timer_sleep in timer. */
static struct list sleeping_list;

/* Semaphore for accessing the sleeping_list */
static struct semaphore sleep_sema;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */
static int load_avg;            /* System load average. In fixed-point
                                   arithmetic. */

/* True only when thread_wake_up is running. */
static bool wake_up_running;

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority,
    int recent_cpu, int nice);
static bool is_thread (struct thread *) UNUSED;
static int thread_calculate_priority (struct thread *);
static void thread_recalculate_priority (struct thread *,
    void *aux UNUSED);
static void thread_recalculate_load_avg (void);
static void thread_recalculate_recent_cpu (struct thread *,
    void *aux UNUSED);
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

static bool wakes_up_earlier (const struct list_elem *elem_1,
    const struct list_elem *elem_2, void *aux);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);
  list_init (&sleeping_list);

  ready_count = 0;

  sema_init (&sleep_sema, 1);
  wake_up_running = false;

  /* Initialised to 0, but needs to be converted to fixed-point arithmetic. */
  load_avg = FP_TO_FIXED_POINT(0);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  int initial_recent_cpu = FP_TO_FIXED_POINT(0);
  init_thread (initial_thread, "main", PRI_DEFAULT, initial_recent_cpu, 0);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Only when the BSD scheduler is running. */
  if (thread_mlfqs)
  {
    int ticks = timer_ticks ();

    /* Increment recent_cpu of the current thread unless it is idle. */
    if (t != idle_thread)
      t->recent_cpu = FP_ADD_INT(t->recent_cpu, 1);

    /* Recalculate recent_cpu for every thread and system load average
       once per second. */
    if (ticks % TIMER_FREQ == 0)
      {
        thread_recalculate_load_avg ();
        thread_foreach (&thread_recalculate_recent_cpu, NULL);
      }
    
    /* Recalculate priorities of all the threads every fourth clock tick. */
    if (ticks % 4 == 0)
      thread_foreach (&thread_recalculate_priority, NULL);
  }

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority, thread_get_recent_cpu (),
               thread_get_nice ());
  /* Need to have a thread struct before calculating priority. */
  if (thread_mlfqs)
    t->priority = thread_calculate_priority (t); 
  tid = t->tid = allocate_tid ();

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack' 
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  intr_set_level (old_level);

  /* Add to run queue. */
  thread_unblock (t);

  yield_if_necessary ();

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  list_insert_ordered (&ready_list, &t->elem, &has_higher_priority, NULL);
  ready_count++;
  t->status = THREAD_READY;
  t->blocking_lock = NULL;
  intr_set_level (old_level);
}

/* Puts threads to sleep (blocks it) for at least the given number of ticks */
void
thread_sleep (int64_t ticks_when_awake)
{
    struct thread *cur = thread_current ();

    ASSERT (cur->status == THREAD_RUNNING);

    cur->ticks_when_awake = ticks_when_awake;
    
    sema_down (&sleep_sema);
    /* We want to insert threads into sleeping_list ordered. */
    list_insert_ordered (&sleeping_list, &cur->sleep_elem,
                         &wakes_up_earlier, NULL);
    sema_up (&sleep_sema);
    
    /* Put the thread to sleep */
    sema_down (&cur->sleep_sema);
}

/* Wakes up all the sleeping threads that can be awoken at this time
   (in terms of total timer ticks). */
void
thread_wake_up (void)
{
  int ticks = timer_ticks();

  sema_down (&sleep_sema);
  
  struct list_elem *e;
  for (e = list_begin (&sleeping_list); e != list_end (&sleeping_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, sleep_elem);

      /* If the first thread can't wake up now neither can any other. */
      if (t->ticks_when_awake > ticks)
        break;

      list_remove (&t->sleep_elem);
      sema_up (&t->sleep_sema);
    }
  
  sema_up (&sleep_sema);
}

/* Returns true if the thread in elem_1 should wake up earlier than
   the thread in elem_2. */
bool
wakes_up_earlier (const struct list_elem *elem_1,
    const struct list_elem *elem_2, void *aux UNUSED)
{
  struct thread *thread_1 = list_entry (elem_1, struct thread, sleep_elem);
  struct thread *thread_2 = list_entry (elem_2, struct thread, sleep_elem);
  return thread_1->ticks_when_awake < thread_2->ticks_when_awake;
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) 
    {
      list_insert_ordered (&ready_list, &cur->elem, &has_higher_priority, NULL);
      ready_count++;
    }
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Forces the current thread to yield if it no longer has highest priority */
void
yield_if_necessary (void)
{
  if (!wake_up_running && !is_highest_priority ())
    thread_yield ();
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  /* The call should be ignored with when BSD scheduler is running. */
  if (thread_mlfqs)
    return;

  ASSERT (PRI_MIN <= new_priority && new_priority <= PRI_MAX);

  struct thread *cur = thread_current ();
  cur->self_set_priority = new_priority;
  thread_choose_priority (cur);
  yield_if_necessary ();
}

/* Chooses the priority field for a specified thread. */
void
thread_choose_priority (struct thread *t)
{
  t->priority = t->self_set_priority;

  sema_down (&t->priority_sema);
  if (!list_empty (&t->donated_priorities))
    {
      struct donated_priority *d =
          list_entry (list_front (&t->donated_priorities),
                      struct donated_priority, priority_elem);

      if (d->priority > t->priority)
        t->priority = d->priority;
  }
  sema_up (&t->priority_sema);
}

/* Called when a thread cannot acquire a lock, donates priority to the lock
   owner and threads causing that to block if necessary.*/
void       
thread_donate_priority (struct thread *donating_thread)
{
  if (donating_thread->blocking_lock == NULL)
    return;

  struct thread *receiving_thread = donating_thread->blocking_lock->holder;
  struct list_elem *e;
  bool priority_in_list = false;

  sema_down (&receiving_thread->priority_sema);

  for (e = list_begin (&receiving_thread->donated_priorities);
       e != list_end (&receiving_thread->donated_priorities);
       e = list_next (e))
    {
      struct donated_priority *d = list_entry (e, struct donated_priority,
                                               priority_elem);

      if (d->blocking_lock == donating_thread->blocking_lock)
        {
          priority_in_list = true;
          if (donating_thread->priority > d->priority)
            {
              d->priority = donating_thread->priority;
              sema_up (&receiving_thread->priority_sema);
              break; 
            }
          sema_up (&receiving_thread->priority_sema);
          return;       
        }             
    }
  
  if (!priority_in_list)
    {
      struct donated_priority *donation =
          malloc (sizeof (struct donated_priority));
      donation->priority = donating_thread->priority;
      donation->blocking_lock = donating_thread->blocking_lock;
      list_insert_ordered (&receiving_thread->donated_priorities,
                           &donation->priority_elem,
                           &has_higher_priority_donation, NULL);
      sema_up (&receiving_thread->priority_sema);
    }

  thread_choose_priority(receiving_thread);
  thread_donate_priority(receiving_thread);
}

/* Removes a donated priority from a thread (if it exists) for the
   corresponding lock. Called when a thread releases a lock. */
void
thread_remove_priority (struct thread *t, struct lock *l)
{  
  struct list_elem *e;
         
  for (e = list_begin (&t->donated_priorities);
       e != list_end (&t->donated_priorities);        
       e = list_next (e))
    {
      struct donated_priority *d =
      list_entry (e, struct donated_priority, priority_elem);
      
      if (d->blocking_lock == l)
        {
          sema_down (&t->priority_sema);
          list_remove (e);
          sema_up (&t->priority_sema);
          free (d);
          return;
        }
    }
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* Calculates (does NOT change) a new priority for the given thread.
   Used by the BSD scheduler. */
int
thread_calculate_priority (struct thread *t)
{
  /* Priority = PRI_MAX in fixed-point aritmetic. */
  int priority = FP_TO_FIXED_POINT(PRI_MAX);
  /* Priority -= recent_cpu / 4 */
  priority = FP_SUBTRACT(priority, FP_DIVIDE_INT(t->recent_cpu, 4));
  /* Priority -= nice * 2 */
  priority = FP_SUBTRACT(priority, 
                         FP_MULTIPLY_INT(FP_TO_FIXED_POINT(t->nice), 2));
  /* Return priority rounded down to the nearest integer. */
  return FP_TO_INT_TRUNCATE(priority);
}

/* Calculates and sets a new priority for the given thread.
   Used by the BSD scheduler. */
void
thread_recalculate_priority (struct thread *t, void *aux UNUSED)
{
  t->priority = thread_calculate_priority (t);
}

/* Sets the current thread's nice value to new_nice. */
void
thread_set_nice (int new_nice) 
{
  ASSERT (new_nice >= -20 && new_nice <= 20);

  struct thread *cur = thread_current ();
  cur->nice = new_nice;
  cur->priority = thread_calculate_priority (cur);
  
  /* Yield if the running thread no longer has the highest priority. */
  yield_if_necessary ();
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  return thread_current ()->nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  return FP_TO_INT_ROUND(FP_MULTIPLY_INT(load_avg, 100));
}

/* Recalculates and changes the system load average. */
void
thread_recalculate_load_avg (void)
{
  /* Ready_threads = ready_count */
  int ready_threads = ready_count;
  /* Increment ready_threads by 1 to account for the current thread. */
  if (thread_current () != idle_thread)
    ready_threads++;
  /* Convert ready_threads to fixed_point arithmetic. */
  ready_threads = FP_TO_FIXED_POINT(ready_threads);
  /* Ready_threads /= 60 */
  ready_threads = FP_DIVIDE_INT(ready_threads, 60);

  /* New_load_avg = load_avg * 59 */
  int new_load_avg = FP_MULTIPLY_INT(load_avg, 59);
  /* New_load_avg /= 60 */
  new_load_avg = FP_DIVIDE_INT(new_load_avg, 60);
  /* New_load_avg += ready_threads */
  new_load_avg = FP_ADD(new_load_avg, ready_threads);

  load_avg = new_load_avg;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  return FP_TO_INT_ROUND(FP_MULTIPLY_INT(thread_current ()->recent_cpu, 100));
}

/* Recalculates and sets a new value for recent_cpu of the given thread. */
void
thread_recalculate_recent_cpu (struct thread *t, void *aux UNUSED)
{
  /* Coefficient = load_avg * 2 */
  int coefficient = FP_MULTIPLY_INT(load_avg, 2);
  /* Coefficient = (load_avg * 2) / (load_avg * 2 + 1) */
  coefficient = FP_DIVIDE(coefficient, FP_ADD_INT(coefficient, 1));

  int recent_cpu = t->recent_cpu;
  /* Recent_cpu = recent_cpu * coefficient */
  recent_cpu = FP_MULTIPLY(recent_cpu, coefficient);
  /* Recent_cpu += nice */
  recent_cpu = FP_ADD_INT(recent_cpu, t->nice);

  t->recent_cpu = recent_cpu;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority,
    int recent_cpu, int nice)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->self_set_priority = priority;
  /* Recent_cpu and nice used only by BSD scheduler but they can be here.
     They will be initialised and kept at 0. */
  t->recent_cpu = recent_cpu;
  t->nice = nice;
  t->magic = THREAD_MAGIC;
 
  sema_init (&t->sleep_sema, 0);
  list_init (&t->donated_priorities);
  sema_init (&t->priority_sema, 1);

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
  {
    ready_count--;
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
  }
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  /* Wake up any threads that can be woken up. */
  wake_up_running = true;
  thread_wake_up ();
  wake_up_running = false;

  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}


/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);

/* Determines whether or not the running thread has the highest priority. */
bool
is_highest_priority (void)
{
  if (list_empty (&ready_list))
    return true;
  
  struct thread *first =
      list_entry (list_front (&ready_list), struct thread, elem);
  return (thread_current ()->priority == first->priority);
}

/* Used to find the maximum of a list, by priority. */
bool
has_lower_priority (const struct list_elem *elem_1,
                    const struct list_elem *elem_2, void *aux UNUSED)
{
  struct thread *thread_1 = list_entry (elem_1, struct thread, elem);
  struct thread *thread_2 = list_entry (elem_2, struct thread, elem);
  return (thread_1->priority < thread_2->priority);
}

/* Used to order lists in descending order of priority. */
bool
has_higher_priority (const struct list_elem *elem_1,
                     const struct list_elem *elem_2, void *aux UNUSED)
{
  struct thread *thread_1 = list_entry (elem_1, struct thread, elem);
  struct thread *thread_2 = list_entry (elem_2, struct thread, elem);
  return (thread_1->priority > thread_2->priority);
}

