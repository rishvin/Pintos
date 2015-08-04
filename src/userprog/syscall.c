#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "userprog/process.h"

struct argv
{
    void *arg[3];
};

typedef void (syscall_fn)(struct argv *args, uint32_t *eax);

struct syscall
{
    syscall_fn *fn;
    int argc;
};

static void syscall_handler (struct intr_frame *);
static int syscall_get(intptr_t *num);
static void syscall_get_args(intptr_t *addr, int argc, struct argv *args);
static void syscall_halt(struct argv *args, uint32_t *eax);
static void syscall_exit(struct argv *args, uint32_t *eax);
static void syscall_exec(struct argv *args, uint32_t *eax);
static void syscall_write(struct argv *args, uint32_t *eax);

static
struct syscall syscall_tbl[] =
{
    {syscall_halt,              0},
    {syscall_exit,              1},
    {syscall_exec,              1},
    {NULL,                      0},
    {NULL,                      0},
    {NULL,                      0},
    {NULL,                      0},
    {NULL,                      0},
    {NULL,                      0},
    {syscall_write,             3},
    {NULL,                      0},
    {NULL,                      0},
    {NULL,                      0},
    {NULL,                      0},
    {NULL,                      0},
    {NULL,                      0},
    {NULL,                      0},
    {NULL,                      0},
    {NULL,                      0},
    {NULL,                      0}
};

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)
{
  void *addr = f->esp;
  int sys_num = syscall_get(addr);
  struct syscall *sysc = &syscall_tbl[sys_num];
  struct argv args;
  syscall_get_args(addr, sysc->argc, &args);
  sysc->fn(&args, &f->eax);
}

static int
syscall_get(intptr_t *num)
{
    if(!is_user_vaddr(num) || *num < SYS_HALT || *num > SYS_INUMBER)
        thread_exit();
    return *num;
}

static void
syscall_get_args(intptr_t *addr, int argc, struct argv *args)
{
    int i;
    for(i = 1; i <= argc; ++i)
    {
        if(!is_user_vaddr(addr + i))
            thread_exit();
        args->arg[i - 1] = (void*)*(addr + i);
    }
}

static void
syscall_halt(struct argv *args UNUSED, uint32_t *eax UNUSED)
{
    shutdown_power_off();
}

static void
syscall_exit(struct argv *args, uint32_t *eax)
{
    *eax = (uint32_t)args->arg[0];
    printf ("%s: exit(%d)\n", thread_current()->name, *eax);
    thread_exit ();
}

static void
syscall_exec(struct argv *args, uint32_t *eax)
{
    const char *filename = (const char*)args->arg[0];
    *eax = process_execute_sync(filename);

}

static void
syscall_write(struct argv *args, uint32_t *eax)
{
    int fd;
    const char *buff;
    unsigned size;

    fd = (int)args->arg[0];
    if(fd < STDIN_FILENO)
        thread_exit();

    buff = (const char*)args->arg[1];
    size = (unsigned)args->arg[2];

    if(!is_user_vaddr(buff) || !is_user_vaddr(buff + size))
        thread_exit();

    if(fd == STDOUT_FILENO)
    {
        putbuf(buff, size);
    }

    *eax = 0;
}
