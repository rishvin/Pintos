#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static inline void
return_value(struct intr_frame *f, uint32_t ret)
{
    f->eax = ret;
}

static inline int
syscall_type(struct intr_frame *f)
{
    return (int)*((intptr_t *)(f->esp));
}

static inline void*
syscall_begin_arg(struct intr_frame *f)
{
    return get_next_addr(f->esp);
}

static void
syscall_exit(struct intr_frame *f)
{
    return_value(f, *(uint32_t*)syscall_begin_arg(f));
}

static void
syscall_write(struct intr_frame *f)
{
    void *addr;
    void *buffer;

    int fd;
    unsigned size;

    addr = syscall_begin_arg(f);
    fd = *(int*)addr;

    addr = get_next_addr(addr);
    buffer = addr;

    addr = get_next_addr(addr);
    size = *(unsigned*)addr;

    if(fd == STDOUT_FILENO)
    {
        //putbuf(buffer, size);
    }
    return_value(f, 0);
}

static void
syscall_handler (struct intr_frame *f)
{

  switch(syscall_type(f))
  {
      case SYS_HALT: break;
      case SYS_EXIT: syscall_exit(f); break;
      case SYS_EXEC: break;
      case SYS_WAIT: break;
      case SYS_CREATE: break;
      case SYS_REMOVE: break;
      case SYS_OPEN: break;
      case SYS_FILESIZE: break;
      case SYS_READ: break;
      case SYS_WRITE: syscall_write(f); break;
      case SYS_SEEK: break;
      case SYS_TELL: break;
      case SYS_CLOSE: break;
      case SYS_MMAP: break;
      case SYS_MUNMAP: break;
      case SYS_CHDIR: break;
      case SYS_MKDIR: break;
      case SYS_READDIR: break;
      case SYS_ISDIR: break;
      case SYS_INUMBER: break;
  }
  thread_exit ();
}
