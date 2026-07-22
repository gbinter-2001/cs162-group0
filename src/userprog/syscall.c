#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"


static void syscall_handler(struct intr_frame*);

void syscall_init(void) { intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall"); }
static bool check_1stack_arg(uint32_t* args);
static int get_user(const uint8_t* uaddr);
static bool put_user(uint8_t* udst, uint8_t byte);



static bool check_1stack_arg(uint32_t* args, int* dist) {
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
static bool check_string_arg(const uint8_t* args,int max_size) {
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


static void syscall_handler(struct intr_frame* f UNUSED) {
  uint32_t* args = ((uint32_t*)f->esp); 
  int syscall_num;
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

  switch(syscall_num){
  case SYS_EXIT:
    thread_current()->pcb->exit_status = arg_1;
    process_exit();
    break;
  
  case SYS_WRITE:
    int fd = arg_1;

    const void* buffer = (const void*)arg_2;
    unsigned size = arg_3;
    if (fd ==1){
        putbuf(buffer,size);
        f->eax = size;
    }
    else {
      f->eax = -1;
    }
    break;
  
 case SYS_PRACTICE:
    int result = arg_1 + 1;
    f->eax = result;
    break;

  case SYS_HALT:
    shutdown_power_off();
    break;
  
  case SYS_EXEC:
    if (!check_string_arg(arg_1, MAX_PATH_LEN)) {
      process_exit();
    }
    pid_t pid = process_execute(arg_1);
    if (pid == TID_ERROR) {
      f->eax = -1;
    }
    else {
      f->eax = pid;
    }
    break;
  
  case SYS_WAIT:
    int wait_result = process_wait(arg_1);
    f->eax = wait_result;
  case SYS_CREATE:
    if (!check_string_arg(arg_1, MAX_PATH_LEN)) 
      return false;
    lock_acquire(&filesys_lock);
    bool success = filesys_create(arg_1, arg_2);
    lock_release(&filesys_lock);  
    f->eax = success ? 0 : -1;
    break;
  }
}