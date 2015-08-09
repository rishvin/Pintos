#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/fd.h"


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
static void syscall_create(struct argv *args, uint32_t *eax);
static void syscall_remove(struct argv *args, uint32_t *eax);
static void syscall_open(struct argv *args, uint32_t *eax);
static void syscall_size(struct argv *args, uint32_t *eax);
static void syscall_read(struct argv *args, uint32_t *eax);
static void syscall_write(struct argv *args, uint32_t *eax);
static void syscall_seek(struct argv *args, uint32_t *eax UNUSED);
static void syscall_tell(struct argv *args, uint32_t *eax);
static void syscall_close(struct argv *args, uint32_t *eax UNUSED);


static
struct syscall syscall_tbl[] =
{
    {syscall_halt,              0},
    {syscall_exit,              1},
    {syscall_exec,              1},
    {NULL,                      0},
    {syscall_create,            2},
    {syscall_remove,            1},
    {syscall_open,              1},
    {syscall_size,              1},
    {syscall_read,              3},
    {syscall_write,             3},
    {syscall_seek,              2},
    {syscall_tell,              1},
    {syscall_close,             1},
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
syscall_create(struct argv *args, uint32_t *eax)
{
    const char *name;
    off_t size;

    name = (const char*)args->arg[0];
    if(!name
    || !is_user_vaddr(name)
    || !is_user_vaddr(name + strlen(name)))
        thread_exit();
    size = (off_t)args->arg[1];

    *eax = filesys_create (name, size);
}

static void
syscall_remove(struct argv *args, uint32_t *eax)
{
    const char *name;
    name = (const char*)args->arg[0];
    if(!name
    || !is_user_vaddr(name)
    || !is_user_vaddr(name + strlen(name)))
        thread_exit();
    *eax = filesys_remove(name);
}

static void
syscall_open(struct argv *args, uint32_t *eax)
{
    const char *name;
    struct file *file;
    name = (const char*)args->arg[0];
    if(!name
    ||!is_user_vaddr(name)
    || !is_user_vaddr(name + strlen(name)))
        thread_exit();
    file = filesys_open(name);
    if(!file)
        *eax = FD_INVALID;
    else
        *eax = fd_insert(file);
}

static void
syscall_size(struct argv *args, uint32_t *eax)
{
    int fd;
    struct file *file;
    fd = (int)args->arg[0];
    if(fd < FD_MIN || fd > FD_MAX)
        *eax = -1;
    file = fd_search(fd);
    if(!file)
        *eax = -1;
    else *eax = file_length(file);
}


static void
syscall_read(struct argv *args, uint32_t *eax)
{
    int fd;
    char *buff;
    unsigned size;

    fd = (int)args->arg[0];
    if(fd < STDIN_FILENO)
        thread_exit();

    buff = (char*)args->arg[1];
    size = (unsigned)args->arg[2];

    if(!buff
    || !is_user_vaddr(buff)
    || !is_user_vaddr(buff + size))
        thread_exit();

    if(fd == STDOUT_FILENO)
        *eax = 0;
    else if(fd == STDIN_FILENO)
    {
        if(size >= sizeof(uint8_t))
        {
            buff[0] = input_getc();
            *eax = sizeof(uint8_t);
        }
        else
            *eax = 0;
    }
    else
    {
        struct file *file = fd_search(fd);
        if(!file)
            *eax = 0;
        else
            *eax = file_read(file, buff, size);
    }
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

    if(!buff
    || !is_user_vaddr(buff)
    || !is_user_vaddr(buff + size))
        thread_exit();

    if(fd == STDIN_FILENO)
        *eax = 0;
    else if(fd == STDOUT_FILENO)
    {
        putbuf(buff, size);
        *eax = size;
    }
    else
    {
        struct file *file = fd_search(fd);
        if(!file)
            *eax = 0;
        else
            *eax = file_write(file, buff, size);
    }
}

static void
syscall_seek(struct argv *args, uint32_t *eax UNUSED)
{
    int fd;
    unsigned position;
    struct file *file;

    fd = (int)args->arg[0];
    if(fd < FD_MIN || fd > FD_MAX)
        thread_exit();

    position = (unsigned)args->arg[1];
    file = fd_search(fd);
    if(!file)
        thread_exit();
    else
        file_seek(file, position);
}

static void
syscall_tell(struct argv *args, uint32_t *eax)
{
    int fd;
    struct file *file;
    fd = (int)args->arg[0];
    if(fd < FD_MIN || fd > FD_MAX)
        thread_exit();
    file = fd_search(fd);
    *eax = 0;
    if(!file)
        thread_exit();
    else
        *eax = file_tell(file);
}

static void
syscall_close(struct argv *args, uint32_t *eax UNUSED)
{
    int fd;
    struct file *file;

    fd = (int)args->arg[0];
    if(fd < FD_MIN || fd > FD_MAX)
        thread_exit();

    file = fd_remove(fd);
    if(!file)
        thread_exit();
    else
        file_close(file);
}
