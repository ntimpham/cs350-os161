#ifndef _PIDLIST_H_
#define _PIDLIST_H_

#include <types.h>
#include <proc.h>

int pid_list_getpid(struct proc *proc, pid_t *retval);
int pid_list_removepid(pid_t pid);
int pid_list_getparent(struct proc *proc, pid_t *retval);
int pid_list_setparent(struct proc *proc, pid_t parentpid);
void pid_list_init();
void pid_list_destroy();

#endif /* _PIDLIST_H_ */
