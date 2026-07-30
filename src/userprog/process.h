#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H
#include <stdbool.h>

#include "threads/thread.h"
#include <stdint.h>
#include "threads/interrupt.h"

// At most 8MB can be allocated to the stack

// These defines will be used in Project 2: Multithreading
#define MAX_STACK_PAGES (1 << 11)
#define MAX_THREADS 127

/* PIDs and TIDs are the same type. PID should be
   the TID of the main thread of the process */
typedef tid_t pid_t;

/* Thread functions (Project 2: Multithreading) */
typedef void (*pthread_fun)(void*);
typedef void (*stub_fun)(pthread_fun, void*);

#include "filesys/file.h"
#define FD_MAX 128


/* The process control block for a given process. Since
   there can be multiple threads per process, we need a separate
   PCB from the TCB. All TCBs in a process will have a pointer
   to the PCB, and the PCB will have a pointer to the main thread
   of the process, which is `special`. */
struct process {
  /* Owned by process.c. */
  uint32_t* pagedir;          /* Page directory. */
  char process_name[16];      /* Name of the main thread */
  struct thread* main_thread; /* Pointer to main thread */
  int exit_status;  
  struct process_status* parent_status;
  struct list children;
  struct file* fd_table[FD_MAX];
  struct file* executable;
};

struct exec_info{
    char* file_name;
    bool load_success;
    struct semaphore load_done;
    struct process_status* process_status;
};

struct process_status{
    pid_t pid;
    int exit_status;
    struct semaphore wait_done;
    bool waited;
    int ref_count;
    struct list_elem elem;
    struct lock ref_lock;
};
struct fork_info{
    struct intr_frame if_;
    struct process_status* process_status;
    struct semaphore done;
    bool success;
    struct process* parent;
};

void userprog_init(void);

pid_t process_execute(const char* file_name);
int process_wait(pid_t);
void process_exit(void);
void process_activate(void);

bool is_main_thread(struct thread*, struct process*);
pid_t get_pid(struct process*);

tid_t pthread_execute(stub_fun, pthread_fun, void*);
tid_t pthread_join(tid_t);
void pthread_exit(void);
void pthread_exit_main(void);
pid_t process_fork(struct intr_frame*);

#endif /* userprog/process.h */
