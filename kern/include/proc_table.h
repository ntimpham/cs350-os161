#ifndef _PIDLIST_H_
#define _PIDLIST_H_

#include <types.h>
#include <proc.h>

int proc_table_add(struct proc *proc, pid_t *retval);
int proc_table_remove(pid_t pid);

void proc_table_init();
void proc_table_destroy();

#endif /* _PIDLIST_H_ */
