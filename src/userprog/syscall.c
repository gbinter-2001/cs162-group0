#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "devices/shutdown.h"

static void syscall_handler(struct intr_frame*);

void syscall_init(void) { intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall"); }

static void syscall_handler(struct intr_frame* f UNUSED) {
  uint32_t* args = ((uint32_t*)f->esp);

  /*
   * The following print statement, if uncommented, will print out the syscall
   * number whenever a process enters a system call. You might find it useful
   * when debugging. It will cause tests to fail, however, so you should not
   * include it in your final submission.
   */

  /* printf("System call number: %d\n", args[0]); */
  switch(args[0]){
  case SYS_EXIT:
    thread_current()->pcb->exit_status = args[1];
    process_exit();
    break;
  
  case SYS_WRITE:
    int fd = args[1];
    const void* buffer = (const void*)args[2];
    unsigned size = args[3];
    if (fd ==1){
        putbuf(buffer,size);
        f->eax = size;
    }
    else {
      f->eax = -1;
    }
    break;
  
 case SYS_PRACTICE:
    int result = args[1] + 1;
    f->eax = result;
    break;

  case SYS_HALT:
    shutdown_power_off();
    break;
  
  case SYS_EXEC:
    break;
  
  case SYS_WAIT:
    break;

 }
}