#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "threads/synch.h"

#define MAX_USER_STRING_LEN PGSIZE

static void syscall_handler(struct intr_frame*);
struct lock filesys_lock;

void syscall_init(void) {lock_init(&filesys_lock);  
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall"); }
static bool check_1stack_arg(uint32_t* args, uint32_t* dist);
static bool check_buffer_arg(uint8_t* args, int size);
static bool check_string_arg(const uint8_t* args, int max_size);
static int get_user(const uint8_t* uaddr);
static bool put_user(uint8_t* udst, uint8_t byte);
static struct file* get_file(int fd);
static int allocate_fd();
static int sys_open(const char* file_name);
static int sys_filesize(int fd);
static int sys_read(int fd, void* buffer, unsigned size);
static int sys_write(int fd, const void* buffer, unsigned size);
static void sys_seek(int fd, unsigned position);
static unsigned sys_tell(int fd);
static void sys_close(int fd);

static bool check_1stack_arg(uint32_t* args, uint32_t* dist) {
  int result;
  int temp= 0;
  for (int k = 0; k < 4; k++) {
      uint8_t* value = ((uint8_t *) args + k);
      if (!is_user_vaddr(value)) 
          return false;
      result = get_user(value);
      if (result == -1)
          return false;
      ((uint8_t*)&temp)[k] = result;  
      }
  *dist = temp;
  return true;
  }
static bool check_buffer_arg(uint8_t* args, int size) {
  int result;
  for (int i = 0; i < size; i++) {
    if (!is_user_vaddr(args + i))
      return false;
    result = get_user(args + i);
    if (result == -1)
      return false;
  }
  return true;
  }

static bool check_string_arg(const uint8_t* args,int max_size ) {
  int result;
  /* check all the addresses are valid until the null terminator */
  for (int i = 0; i < max_size; i++) {
    if (!is_user_vaddr(args + i))
      return false;
    result = get_user(args + i);
    if (result == -1)
      return false;
    if (result == '\0')
      return true;
  }
  return false; 
}

/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful,
   -1 if a segfault occurred. */
static int get_user (const uint8_t *uaddr) {
    int result;
    asm ("movl $1f, %0; movzbl %1, %0; 1:"
    : "=&a" (result) : "m" (*uaddr));
    return result;
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful,
   false if a segfault occurred. */
static bool put_user (uint8_t *udst, uint8_t byte) {
    int error_code;
    asm ("movl $1f, %0; movb %b2, %1; 1:"
    : "=&a" (error_code), "=m" (*udst) : "q" (byte));
    return error_code != -1;
}
static int allocate_fd() {
  for (int i = 2; i < FD_MAX; i++) {
    if (thread_current()->pcb->fd_table[i] == NULL) {
      return i;
    }
  }
  return -1;
}
static struct file* get_file(int fd) {
  if (fd < 2 || fd >= FD_MAX)
    return NULL;
  return thread_current()->pcb->fd_table[fd];
}


static int sys_open(const char* file_name) {
  if (!check_string_arg((const uint8_t*)file_name, MAX_USER_STRING_LEN))
    process_exit();
  lock_acquire(&filesys_lock);
  struct file* file = filesys_open(file_name);
  lock_release(&filesys_lock);
  if (file == NULL)
    return -1;
  int fd = allocate_fd();
  if (fd == -1) {
    lock_acquire(&filesys_lock);
    file_close(file);
    lock_release(&filesys_lock);
    return -1;
  }
  thread_current()->pcb->fd_table[fd] = file;
  return fd;
}

static int sys_filesize(int fd) {
  struct file* file = get_file(fd);
  if (file == NULL || fd < 2)
    return -1;
  return file_length(file);
}

static int sys_read(int fd, void* buffer, unsigned size) {
  if (!check_buffer_arg((uint8_t*)buffer, size))
    process_exit();
  struct file* file = get_file(fd);
  if (file == NULL || fd < 2)
    return -1;
  lock_acquire(&filesys_lock);
  int bytes = file_read(file, buffer, size);
  lock_release(&filesys_lock);
  return bytes;
}
static int sys_write(int fd, const void* buffer, unsigned size) {
  if (!check_buffer_arg((uint8_t*)buffer, size))
    process_exit();
  if (fd == 1) {
    putbuf(buffer, size);
    return size;
  }
  struct file* file = get_file(fd);
  if (file == NULL || fd < 2)
    return -1;
  lock_acquire(&filesys_lock);
  int bytes = file_write(file, (void*)buffer, size);
  lock_release(&filesys_lock);
  return bytes;
}
static void sys_seek(int fd, unsigned position) {
  struct file* file = get_file(fd);
  if (file == NULL || fd < 2)
    return;
  lock_acquire(&filesys_lock);
  file_seek(file, position);
  lock_release(&filesys_lock);
}


static unsigned sys_tell(int fd) {
  struct file* file = get_file(fd);
  if (file == NULL || fd < 2)
    return -1;
  lock_acquire(&filesys_lock);
  unsigned position = file_tell(file);
  lock_release(&filesys_lock);
  return position;
}
static void sys_close(int fd) {
  struct file* file = get_file(fd);
  if (file == NULL || fd < 2)
    return;
  lock_acquire(&filesys_lock);
  file_close(file);
  lock_release(&filesys_lock);
  thread_current()->pcb->fd_table[fd] = NULL;
}
static void syscall_handler(struct intr_frame* f UNUSED) {
  uint32_t* args = ((uint32_t*)f->esp); 
  uint32_t syscall_num;
  if (!check_1stack_arg(args, &syscall_num)) {
    process_exit();
  }
  /*  
   * The following print statement, if uncommented, will print out the syscall
   * number whenever a process enters a system call. You might find it useful
   * when debugging. It will cause tests to fail, however, so you should not
   * include it in your final submission.
   */
  
  /* printf("System call number: %d\n", args[0]); */
  uint32_t arg_1, arg_2, arg_3;
  switch (syscall_num) {
    case SYS_READ:
    case SYS_WRITE:
      if (!check_1stack_arg(args + 3, &arg_3)) {
        process_exit();
      }
      __attribute__((fallthrough)); 
    case SYS_CREATE:
    case SYS_SEEK:
    case SYS_READDIR:
      if (!check_1stack_arg(args + 2, &arg_2)) {
        process_exit();
      }
      __attribute__((fallthrough));
    case SYS_PRACTICE:
    case SYS_EXIT:
    case SYS_WAIT:
    case SYS_EXEC:
    case SYS_REMOVE:
    case SYS_OPEN:
    case SYS_FILESIZE:
    case SYS_TELL:
    case SYS_CLOSE:
    case SYS_INUMBER:
    case SYS_CHDIR:
    case SYS_MKDIR:
    case SYS_ISDIR:
      if (!check_1stack_arg(args + 1, &arg_1)) {
        process_exit();
      }
      __attribute__((fallthrough));
    case SYS_HALT:
    case SYS_FORK:
      break;
    default:
      break;
  }

  switch (syscall_num) {
  case SYS_EXIT:
    thread_current()->pcb->exit_status = arg_1;
    process_exit();
    break;

  case SYS_PRACTICE:
    f->eax = arg_1 + 1;
    break;

  case SYS_HALT:
    shutdown_power_off();
    break;

  case SYS_EXEC:
    if (!check_string_arg((const uint8_t*)arg_1, MAX_USER_STRING_LEN))
      process_exit();
    pid_t pid = process_execute((const char*)arg_1);
    f->eax = (pid == TID_ERROR) ? -1 : pid;
    break;

  case SYS_WAIT:
    f->eax = process_wait(arg_1);
    break;

  case SYS_CREATE:
    if (!check_string_arg((const uint8_t*)arg_1, MAX_USER_STRING_LEN))
      process_exit();
    lock_acquire(&filesys_lock);
    f->eax = filesys_create((const char*)arg_1, arg_2);
    lock_release(&filesys_lock);
    break;

  case SYS_REMOVE:
    if (!check_string_arg((const uint8_t*)arg_1, MAX_USER_STRING_LEN))
      process_exit();
    lock_acquire(&filesys_lock);
    f->eax = filesys_remove((const char*)arg_1);
    lock_release(&filesys_lock);
    break;

  case SYS_OPEN:
    f->eax = sys_open((const char*)arg_1);
    break;

  case SYS_FILESIZE:
    f->eax = sys_filesize(arg_1);
    break;

  case SYS_READ:
    f->eax = sys_read(arg_1, (void*)arg_2, arg_3);
    break;

  case SYS_WRITE:
    f->eax = sys_write(arg_1, (void*)arg_2, arg_3);
    break;

  case SYS_SEEK:
    sys_seek(arg_1, arg_2);
    break;

  case SYS_TELL:
    f->eax = sys_tell(arg_1);
    break;

  case SYS_CLOSE:
    sys_close(arg_1);
    break;

  case SYS_FORK:
    f->eax = process_fork(f);
    break;

  default:
    break;
  }
}