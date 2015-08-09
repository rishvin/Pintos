#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include <fp.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/timer.h"

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

#define MLFQS_TICK_EXPIRE 4

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */


/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Priority list */
static struct list priority_queue[PRI_MAX - PRI_MIN + 1];

/* mlfq related variables */
static fp_t load_avg;

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

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

inline static int8_t thread_bm_at(int bm, int slot);
static int thread_bm_get_unset(int bm);
static void thread_bm_update_at(int *bm, int slot, int8_t on);
static void thread_update_lock(struct thread *t, struct lock *lock, struct thread *child_thread);
static int thread_get_max_inherit_priority(struct thread *t);

static void thread_init_priority_queue(void);

//static 
void thread_calc_rcpu(struct thread *t);
static void thread_calc_priority(struct thread *t);

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

  thread_init_priority_queue();
  list_init (&all_list);
  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
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
  t->rcpu = FP_INC(t->rcpu);
  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
  {
      kernel_ticks++; 
      //printf("cpu  for %s = %d\n", t->name, t->rcpu);
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
  init_thread (t, name, priority);
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
  
  /* Check if the created thread priority is more than running thread priority,
     if yes then yield.
  */
  if(thread_current()->priority < t->priority)
  {
      thread_yield();
  }
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
  ASSERT (is_thread (t));
  if(t->sleep_time <= 0 && !t->is_waiting)
  {
    enum intr_level old_level;
    ASSERT (t->status == THREAD_BLOCKED);
    old_level = intr_disable ();
    if(t != idle_thread)
        thread_push_to_priority_queue(t);
    t->sleep_time = 0;
    t->status = THREAD_READY;
    intr_set_level (old_level);
  }
}

void thread_on_tick(struct thread *t, void *aux)
{
    int64_t ticks = *(int64_t*)aux;
    if((ticks % TIMER_FREQ) == 0)
        thread_calc_rcpu(t);
    if(t->status == THREAD_BLOCKED)
    {
        if(t->sleep_time > 0)
            t->sleep_time--;
        else if(t->is_waiting == 0)
            thread_unblock(t);
    }        
    if(t->status != THREAD_BLOCKED)
    {
        if((thread_mlfqs) && (ticks % MLFQS_TICK_EXPIRE == 0))
            thread_calc_priority(t);
    }
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
    thread_push_to_priority_queue(cur);

  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
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

/* Sets the current thread's priority to NEW_PRIORITY. 
*/
void
thread_set_priority (int new_priority) 
{
    if(!thread_mlfqs)
    {
        struct thread *t = thread_current();
        int old_priority = t->priority;
        int update_priority;
    
        t->saved_priority = new_priority;
    
        if(new_priority > old_priority) 
            update_priority = new_priority;
        else
        {
            enum intr_level old_level = intr_disable();
            update_priority = thread_get_max_inherit_priority(t);
            intr_set_level(old_level);
            if(update_priority == -1)
                update_priority = new_priority;
        }
        if(old_priority != update_priority)
        {
            enum intr_level old_level = intr_disable();
            thread_update_priority_queue(t, update_priority);
            intr_set_level(old_level);
            t->priority = update_priority;
        }
        if(old_priority > update_priority)
            thread_yield();
    }
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice) 
{
  if(thread_mlfqs)
  {
      enum intr_level old_level = intr_disable();
      ASSERT(nice >= NICE_MIN && nice <= NICE_MAX);
      thread_current()->nice = nice;
      thread_calc_priority(thread_current());
      thread_current()->saved_priority = thread_current()->priority;
      intr_set_level(old_level);
  }
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  return thread_current()->nice;
}

/* This function is called every 1 time in 60 sec
   to calculate the load on the system. 
*/

void thread_calc_load_avg()
{
    static fp_t cmax = FP_CONV_INT(59) / 60;
    static fp_t cmin = FP_CONV_INT(1) / 60;
    
    ASSERT (intr_get_level () == INTR_OFF);
    load_avg = FP_MUL(cmax, load_avg) + cmin * thread_get_active_count();
}

/* This function is called every 1 time in 60 sec 
   to calculate the recent cpu usage by the thread.
*/
//static 
void thread_calc_rcpu(struct thread *t)
{
    fp_t val = 2 * load_avg;
    t->rcpu = FP_MUL(FP_DIV(val, FP_INC(val)), t->rcpu) + FP_CONV_INT(t->nice);
}

/* This function calulates the priority of thread, 1 time in every 4 sec. */
static void thread_calc_priority(struct thread *t)
{
    enum intr_level old_level;
    ASSERT(thread_mlfqs);
    int np = PRI_MAX - FP_GET_INT_RND(t->rcpu / MLFQS_TICK_EXPIRE) - t->nice * 2;
    np = np < PRI_MIN ? PRI_MIN : np > PRI_MAX ? PRI_MAX : np;
    old_level = intr_disable();
    thread_update_priority_queue(t, np);  
    intr_set_level(old_level);
    t->priority = np;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
    int avg;
    enum intr_level old_level = intr_disable();
    avg = FP_GET_INT_RND(load_avg * 100);
    intr_set_level(old_level);
    return avg;
    
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
    int rcpu;
    enum intr_level old_level = intr_disable();
    rcpu = FP_GET_INT_RND(thread_current()->rcpu * 100);
    intr_set_level(old_level);
    return rcpu;
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
init_thread (struct thread *t, const char *name, int priority)
{
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);
  
  memset (t, 0, sizeof *t);
  t->locks_bm = 0;
  t->nice = NICE_DEFAULT;
  if(t == initial_thread)
      t->rcpu = 0;
  else
      t->rcpu = thread_current()->rcpu;
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  if(!thread_mlfqs)
      t->priority = priority;
  else 
      thread_calc_priority(t);
  t->saved_priority = t->priority;
  t->magic = THREAD_MAGIC;
  list_push_back (&all_list, &t->allelem);
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
    struct thread *t = thread_pop_from_priority_queue();
    if(t == NULL)
        t = idle_thread;
    return t;
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

static void thread_init_priority_queue()
{
    int index;
    for(index = 0; index <= PRI_MAX - PRI_MIN; ++index)
        list_init(&priority_queue[index]);
}

void thread_push_to_priority_queue(struct thread *t)
{
    int priority;
    
    ASSERT(intr_get_level() == INTR_OFF);
    ASSERT (is_thread (t));

    priority = t->priority;
    ASSERT(PRI_MIN <= priority && priority <= PRI_MAX);
    
    list_push_back (&priority_queue[priority - PRI_MIN], &t->elem);
}

struct thread* thread_pop_from_priority_queue()
{
    struct thread *t = NULL;
    int index;
    
    ASSERT(intr_get_level() == INTR_OFF);

    for(index = PRI_MAX - PRI_MIN; index >= 0; --index)
    {
        if(!list_empty(&priority_queue[index]))
        {
            t = list_entry(list_pop_front (&priority_queue[index]), struct thread, elem);
            break;
        }
    }
    return t;
}

/* Update the priority_queue in case a new priority is assigned to the thread. */
void thread_update_priority_queue(struct thread *t, int new_priority)
{
    struct list *pq;
    struct list_elem *elem;
    ASSERT(intr_get_level() == INTR_OFF);
    ASSERT(PRI_MIN <= new_priority && new_priority <= PRI_MAX);
    ASSERT(PRI_MIN <= t->priority && t->priority <= PRI_MAX);
    
    if(t->priority == new_priority)
        return;
    
    pq = &priority_queue[t->priority];
    elem = list_begin(pq);
    
    if(elem != list_end(pq))
    {
        for(elem = list_next(elem); elem != list_end(pq); elem = list_next(elem))
        {
            if(&t->elem == elem)
            {
                list_remove(elem);
                list_push_back(&priority_queue[new_priority], elem);
                break;
            }
        }
    }
}

int thread_get_active_count()
{
    ASSERT(intr_get_level() == INTR_OFF);
    int count = ((thread_current() != idle_thread)) ? 1 : 0;
    int i;
    for(i = 0; i < PRI_MAX - PRI_MIN; ++i)
        count += list_size(&priority_queue[i]);
    return count;
}

/* Functions related to hold locks. */
inline static int8_t thread_bm_at(int bm, int slot)
{
    return (bm & (1 << slot));
}

static int thread_bm_get_unset(int bm)
{
    int slot;
    for(slot = 0; slot < THREAD_LOCKS; ++slot)
        if(thread_bm_at(bm, slot) == 0)
            return slot;
    return -1;
}

static void thread_bm_update_at(int *bm, int slot, int8_t on)
{
    if(on)
        *bm = *bm | (1 << slot);
    else
        *bm = *bm & ((~0) ^ (1 << slot));
}

static void thread_update_lock(struct thread *t, struct lock *lock, struct thread *child_thread)
{
    ASSERT(!thread_mlfqs)
    int bm = t->locks_bm;
    struct thread_lock *locks = t->locks;
    
    int slot;
    for(slot = 0; slot < THREAD_LOCKS; ++slot)
    {
        if(thread_bm_at(bm, slot) && (locks[slot].lock == lock))
        {
            locks[slot].child_thread = child_thread;
            return;
        }
    }
    ASSERT(0);
}

static int thread_get_max_inherit_priority(struct thread *t)
{
    ASSERT(!thread_mlfqs)
    int priority = PRI_MIN - 1;
    int bm = t->locks_bm;
    struct thread_lock *locks = t->locks;
    
    int slot;
    for(slot = 0; slot < THREAD_LOCKS; ++slot)
    {
        if(thread_bm_at(bm, slot) && locks[slot].child_thread)
        {
            int new_priority = locks[slot].child_thread->priority;
            if(new_priority > priority)
                priority = new_priority;
        }
    }
    return priority;
}

uint8_t thread_add_lock(struct thread *t, struct lock *lock, struct thread *child_thread)
{
    ASSERT(!thread_mlfqs)
    int slot = thread_bm_get_unset(t->locks_bm);
    if(slot == -1)
        return 0;
    else
    {
        struct thread_lock *node = &t->locks[slot];
        node->lock = lock;
        node->child_thread = child_thread;
        thread_bm_update_at(&t->locks_bm, slot, 1);
    }
    return 1;
}

void  thread_remove_lock(struct thread *t, struct lock *lock)
{
    ASSERT(!thread_mlfqs)
    int *bm = &t->locks_bm;
    struct thread_lock *locks = t->locks;
    int slot;
    for(slot = 0; slot < THREAD_LOCKS; ++slot)
    {
        if(thread_bm_at(*bm, slot) && (locks[slot].lock == lock))
        {
            thread_bm_update_at(bm, slot, 0);
            locks[slot].lock = NULL;
            locks[slot].child_thread = NULL;
            return;
        }
    }
    ASSERT(0);
}

int thread_get_max_priority(struct thread *t)
{
    ASSERT(!thread_mlfqs)
    int priority = thread_get_max_inherit_priority(t);
    if((priority == PRI_MIN - 1) || (priority < t->saved_priority))
        priority = t->saved_priority;
    return priority;
}

void thread_donate_priority(struct thread *t, struct lock *lock , struct thread *child_thread)
{
    ASSERT(!thread_mlfqs)
    if(t)
    {
        int new_priority = child_thread->priority;
        ASSERT(is_thread(t));
        if(t->priority >= new_priority)
            return;
        thread_update_priority_queue(t, new_priority);
        t->priority = new_priority;
        thread_update_lock(t, lock, child_thread);
        thread_donate_priority(t->parent_thread, t->parent_lock, child_thread);
    }
}

void init_mlfqs()
{
    thread_mlfqs = true;
}

struct thread* thread_search(tid_t tid)
{
    struct list_elem *e;
    ASSERT (intr_get_level () == INTR_OFF);

    for (e = list_begin (&all_list); e != list_end (&all_list);
         e = list_next (e))
    {
        struct thread *t = list_entry (e, struct thread, allelem);
        if(t->tid == tid)
            return t;
    }
    return NULL;
}


/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
