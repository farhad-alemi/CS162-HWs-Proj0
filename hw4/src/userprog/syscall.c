#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"

static void syscall_handler(struct intr_frame*);

void syscall_init(void) { intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall"); }

void syscall_exit(int status) {
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit();
}

/*
 * This does not check that the buffer consists of only mapped pages; it merely
 * checks the buffer exists entirely below PHYS_BASE.
 */
static void validate_buffer_in_user_region(const void* buffer, size_t length) {
  uintptr_t delta = PHYS_BASE - buffer;
  if (!is_user_vaddr(buffer) || length > delta)
    syscall_exit(-1);
}

/*
 * This does not check that the string consists of only mapped pages; it merely
 * checks the string exists entirely below PHYS_BASE.
 */
static void validate_string_in_user_region(const char* string) {
  uintptr_t delta = PHYS_BASE - (const void*)string;
  if (!is_user_vaddr(string) || strnlen(string, delta) == delta)
    syscall_exit(-1);
}

static int syscall_open(const char* filename) {
  struct thread* t = thread_current();
  if (t->open_file != NULL)
    return -1;

  t->open_file = filesys_open(filename);
  if (t->open_file == NULL)
    return -1;

  return 2;
}

static int syscall_write(int fd, void* buffer, unsigned size) {
  struct thread* t = thread_current();
  if (fd == STDOUT_FILENO) {
    putbuf(buffer, size);
    return size;
  } else if (fd != 2 || t->open_file == NULL)
    return -1;

  return (int)file_write(t->open_file, buffer, size);
}

static int syscall_read(int fd, void* buffer, unsigned size) {
  struct thread* t = thread_current();
  if (fd != 2 || t->open_file == NULL)
    return -1;

  return (int)file_read(t->open_file, buffer, size);
}

static void syscall_close(int fd) {
  struct thread* t = thread_current();
  if (fd == 2 && t->open_file != NULL) {
    file_close(t->open_file);
    t->open_file = NULL;
  }
}

static void* syscall_sbrk(int increment) {
  struct thread* t = thread_current();
  void* temp = t->heap_brk;
  void* page_addr;
  size_t page_cnt;
  bool success;

  if (increment > 0) {
    page_cnt = (pg_round_up(temp + increment) - pg_round_up(temp)) / PGSIZE;
    page_addr = palloc_get_multiple(PAL_ZERO | PAL_USER, page_cnt);

    if (page_addr == NULL && page_cnt) {
      goto fail;
    }

    temp = pg_round_up(temp);

    for (size_t i = 0; i < page_cnt; ++i) {
      success = pagedir_set_page(t->pagedir, temp + i * PGSIZE, page_addr + i * PGSIZE, true);

      if (!success) {
        goto fail;
      }
    }

    temp = t->heap_brk;
    t->heap_brk += increment;
    return temp;
  } else if (increment == 0) {
    return temp;
  } else {
    increment = (increment + temp < t->heap_base) ? t->heap_brk : increment;
    page_cnt = (pg_round_up(temp) - pg_round_up(increment + temp)) / PGSIZE;
    temp = pg_round_up(increment + temp);

    for (size_t i = 0; i < page_cnt; ++i) {
      palloc_free_page(pagedir_get_page(t->pagedir, temp + i * PGSIZE));
      pagedir_clear_page(t->pagedir, temp + i * PGSIZE);
    }

    temp = t->heap_brk;
    t->heap_brk += increment;
    return temp;
  }

fail:
  return (void*)-1;
}

static void syscall_handler(struct intr_frame* f) {
  uint32_t* args = (uint32_t*)f->esp;
  struct thread* t = thread_current();
  t->in_syscall = true;

  validate_buffer_in_user_region(args, sizeof(uint32_t));
  switch (args[0]) {
    case SYS_EXIT:
      validate_buffer_in_user_region(&args[1], sizeof(uint32_t));
      syscall_exit((int)args[1]);
      break;

    case SYS_OPEN:
      validate_buffer_in_user_region(&args[1], sizeof(uint32_t));
      validate_string_in_user_region((char*)args[1]);
      f->eax = (uint32_t)syscall_open((char*)args[1]);
      break;

    case SYS_WRITE:
      validate_buffer_in_user_region(&args[1], 3 * sizeof(uint32_t));
      validate_buffer_in_user_region((void*)args[2], (unsigned)args[3]);
      f->eax = (uint32_t)syscall_write((int)args[1], (void*)args[2], (unsigned)args[3]);
      break;

    case SYS_READ:
      validate_buffer_in_user_region(&args[1], 3 * sizeof(uint32_t));
      validate_buffer_in_user_region((void*)args[2], (unsigned)args[3]);
      f->eax = (uint32_t)syscall_read((int)args[1], (void*)args[2], (unsigned)args[3]);
      break;

    case SYS_CLOSE:
      validate_buffer_in_user_region(&args[1], sizeof(uint32_t));
      syscall_close((int)args[1]);
      break;
    case SYS_SBRK:
      f->eax = (uint32_t)syscall_sbrk((int)args[1]);
      break;

    default:
      printf("Unimplemented system call: %d\n", (int)args[0]);
      break;
  }

  t->in_syscall = false;
}
