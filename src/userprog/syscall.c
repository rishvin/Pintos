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
#include "userprog/pagedir.h"
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

static bool is_valid_user_vaddr(const void *addr);
static void force_exit(int status);
static void syscall_handler (struct intr_frame *);
static int syscall_get(intptr_t *num);
static void syscall_get_args(intptr_t *addr, int argc, struct argv *args);
static void syscall_halt(struct argv *args, uint32_t *eax);
static void syscall_exit(struct argv *args, uint32_t *eax);
static void syscall_exec(struct argv *args, uint32_t *eax);
static void syscall_wait(struct argv *args, uint32_t *eax);
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
    {syscall_wait,              1},
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

static bool
is_valid_user_vaddr(const void *addr)
{
    return is_user_vaddr(addr) && pagedir_get_page(thread_current()->pagedir, addr);
}

static void
force_exit(int status)
{
    process_notify(status);
    thread_exit();
}

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
    if(!is_valid_user_vaddr(num) || *num < SYS_HALT || *num > SYS_INUMBER)
        force_exit(-1);
    return *num;
}

static void
syscall_get_args(intptr_t *addr, int argc, struct argv *args)
{
    int i;
    for(i = 1; i <= argc; ++i)
    {
        if(!is_valid_user_vaddr(addr + i))
            force_exit(-1);
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
    force_exit(*eax);
}

static void
syscall_exec(struct argv *args, uint32_t *eax)
{
    const char *filename = (const char*)args->arg[0];
    if(!is_valid_user_vaddr(filename))
        force_exit(-1);
    *eax = process_execute_sync(filename);

}

static void
syscall_wait(struct argv *args, uint32_t *eax)
{
    tid_t pid = (tid_t)args->arg[0];
    *eax = process_wait(pid);
}

static void
syscall_create(struct argv *args, uint32_t *eax)
{
    const char *name;
    off_t size;

    name = (const char*)args->arg[0];
    if(!name
    || !is_valid_user_vaddr(name)
    || !is_valid_user_vaddr(name + strlen(name)))
        force_exit(-1);

    size = (off_t)args->arg[1];
    *eax = filesys_create (name, size);
}

static void
syscall_remove(struct argv *args, uint32_t *eax)
{
    const char *name;

    name = (const char*)args->arg[0];
    if(!name
    || !is_valid_user_vaddr(name)
    || !is_valid_user_vaddr(name + strlen(name)))
        force_exit(-1);

    *eax = filesys_remove(name);
}

static void
syscall_open(struct argv *args, uint32_t *eax)
{
    const char *name;
    struct file *file;

    name = (const char*)args->arg[0];
    if(!name
    ||!is_valid_user_vaddr(name)
    || !is_valid_user_vaddr(name + strlen(name)))
        force_exit(-1);

    file = filesys_open(name);
    if(!file)
        *eax = FD_INVALID;
    else
        *eax = fd_insert(&thread_current()->proc->fd_node, file);
}

static void
syscall_size(struct argv *args, uint32_t *eax)
{
    int fd;
    fd = (int)args->arg[0];
    *eax = 0;

    if(fd >= FD_MIN && fd <= FD_MAX)
    {
        struct file *file = fd_search(&thread_current()->proc->fd_node, fd);
        if(file)
            *eax = file_length(file);
    }
}

static void
syscall_read(struct argv *args, uint32_t *eax)
{
    int fd;
    char *buff;
    unsigned size;

    fd = (int)args->arg[0];
    buff = (char*)args->arg[1];
    size = (unsigned)args->arg[2];

    if(!buff
    || !is_valid_user_vaddr(buff)
    || !is_valid_user_vaddr(buff + size))
        force_exit(-1);

    *eax = 0;

    if(fd == STDIN_FILENO)
    {
        if(size >= sizeof(uint8_t))
        {
            buff[0] = input_getc();
            *eax = sizeof(uint8_t);
        }
    }
    else if(fd != STDOUT_FILENO)
    {
        struct file *file = fd_search(&thread_current()->proc->fd_node, fd);
        if(file)
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
    buff = (const char*)args->arg[1];
    size = (unsigned)args->arg[2];

    if(!buff
    || !is_valid_user_vaddr(buff)
    || !is_valid_user_vaddr(buff + size))
        force_exit(-1);

    *eax = 0;

    if(fd == STDOUT_FILENO)
    {
        putbuf(buff, size);
        *eax = size;
    }
    else if(fd != STDIN_FILENO)
    {
        struct file *file = fd_search(&thread_current()->proc->fd_node, fd);
        if(file)
            *eax = file_write(file, buff, size);
    }
}

static void
syscall_seek(struct argv *args, uint32_t *eax UNUSED)
{
    int fd;
    fd = (int)args->arg[0];
    if(fd >= FD_MIN && fd <= FD_MAX)
    {
        unsigned position = (unsigned)args->arg[1];
        struct file *file = fd_search(&thread_current()->proc->fd_node, fd);
        if(file)
            file_seek(file, position);
    }
}

static void
syscall_tell(struct argv *args, uint32_t *eax)
{
    int fd;
    fd = (int)args->arg[0];
    *eax = -1;
    if(fd >= FD_MIN || fd <= FD_MAX)
    {
        struct file *file = fd_search(&thread_current()->proc->fd_node, fd);
        if(file)
            *eax = file_tell(file);
    }
}

static void
syscall_close(struct argv *args, uint32_t *eax UNUSED)
{
    int fd;
    fd = (int)args->arg[0];
    if(fd >= FD_MIN && fd <= FD_MAX)
    {
        struct file *file = fd_remove(&thread_current()->proc->fd_node, fd);
        if(file)
            file_close(file);
    }
}
