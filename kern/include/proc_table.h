#ifndef _PROC_TABLE_H_
#define _PROC_TABLE_H_

#include <types.h>
#include <proc.h>
#include <synch.h>
#include "array.h"

struct proc_table_entry {
  struct proc *proc;
  pid_t pid;
  volatile unsigned numref;
  volatile bool isdead; // condition of cv
  volatile int exitcode; // value of cv
  struct cv *exitcode_cv;
  struct proc_table_entry *parent;
  struct array *children;
};

extern struct lock *proc_table_lock;

int proc_table_add(struct proc *proc, pid_t *retval);
int proc_table_remove(pid_t pid);

int proc_table_get(pid_t pid, struct proc_table_entry **retval);

void proc_table_init(void);
void proc_table_destroy(void);

#endif /* _PROC_TABLE_H_ */
