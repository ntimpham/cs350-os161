#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <proc_table.h>
#include <machine/trapframe.h>
#include "array.h"
#include "opt-A2.h"

#if OPT_A2
int
sys_fork(struct trapframe *tf,
      pid_t *retval)
{

  struct proc *parent = curproc;
  struct proc *child;

  *retval = -1;

  // Create process structure for child process
  child = proc_create_runprogram(parent->p_name);
  if (child == NULL) return ENOMEM;

  // Create and copy address space (and data) from parent to child
  struct addrspace *c_as = as_create();
  if (c_as == NULL) {
    proc_destroy(child);
    return ENOMEM;
  }
  int x = as_copy(parent->p_addrspace, (struct addrspace **)&c_as);
  if (x) {
    as_destroy(c_as);
    proc_destroy(child);
    return x;
  }

  // Attach the newly created address space to the child process structure
  child->p_addrspace = c_as;

  // Assign PID to child process and create the parent/child relationship
  pid_t c_pid;
  x = proc_table_add(child, (pid_t*)&c_pid);
  if (x) {
    as_destroy(c_as);
    proc_destroy(child);
    return x;
  }
  child->pid = c_pid;
  struct proc_table_entry *c_entry;
  proc_table_get(c_pid, (struct proc_table_entry**)&c_entry);
  lock_acquire(c_entry->lock);
  proc_table_get(parent->pid, (struct proc_table_entry**)&(c_entry->parent) );
  lock_release(c_entry->lock);

  struct proc_table_entry *p_entry;
  proc_table_get(parent->pid, (struct proc_table_entry**)&p_entry);
  lock_acquire(p_entry->lock);
  x = array_add(p_entry->children, c_entry, NULL);
  lock_release(p_entry->lock);
  if (x) {
    proc_table_remove(c_pid);
    as_destroy(c_as);
    proc_destroy(child);
    return x;
  }

  // Create thread for child process (need a safe way to pass the trapframe to
  // the child thread).
  struct trapframe *c_tf = kmalloc(sizeof(struct trapframe) );
  if (c_tf == NULL) {
    proc_table_remove(c_pid);
    as_destroy(c_as);
    proc_destroy(child);
    return ENOMEM;
  }
  *c_tf = *tf;
  x = thread_fork(curthread->t_name, child, enter_forked_process, c_tf, 0);
  if (x) {
    kfree(c_tf);
    proc_table_remove(c_pid);
    as_destroy(c_as);
    proc_destroy(child);
    return x;
  }

  *retval = c_pid;
  return 0; // Success
}
#endif

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;

#if OPT_A2
  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  struct proc_table_entry *entry;
  proc_table_get(p->pid, &entry);
    KASSERT(entry != NULL);

  lock_acquire(entry->lock);
    KASSERT(p == entry->proc);
    KASSERT(p->pid == entry->pid);

  // Parent update
  struct proc_table_entry *pe = entry->parent;
  if (pe != NULL) {
    lock_acquire(pe->lock);
      KASSERT(pe->numref > 0);
    pe->numref--;

    // Check cleanup
    if (pe->isdead && pe->numref == 0) {
      proc_table_remove(pe->pid);
    }
    else lock_release(pe->lock);
  }

  // Children update
  struct array *c = entry->children;
  for (unsigned i = 0; i < array_num(c); i++) {
    struct proc_table_entry *centry = array_get(c, i);
      KASSERT(centry != NULL);
    lock_acquire(centry->lock);
      KASSERT(centry->numref > 0);
    centry->numref--;

    // Check cleanup
    if (centry->isdead && centry->numref == 0) {
      proc_table_remove(centry->pid);
    }
    else lock_release(centry->lock);
  }

  // Self cleanup
  if (entry->numref == 0) {
    proc_table_remove(p->pid);
  }
  else {
    entry->isdead = true;
    entry->exitcode = exitcode;
    cv_broadcast(entry->exitcode_cv, entry->lock);
    lock_release(entry->lock);
  }

  // Cleanup process
    KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  as = curproc_setas(NULL);
  as_destroy(as);
  proc_remthread(curthread);
  proc_destroy(p);

  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");

  /*
  //
  //@@@@@=====@@@@@===== END OF _exit implementation =====@@@@@=====@@@@@
  //
   */
#else
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);

  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
#endif
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
#if OPT_A2
    KASSERT(curproc != NULL);
  *retval = curproc->pid;
  return 0; // Success

  /*
  //
  //@@@@@=====@@@@@===== END OF getpid implementation =====@@@@@=====@@@@@
  //
   */
#else
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = 1;
#endif
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{

  int exitstatus;
  int result;

#if OPT_A2
  if (status == NULL) {
    return EFAULT;
  }
  if (options != 0) {
    return EINVAL;
  }

  struct proc_table_entry *entry;
  int x = proc_table_get(pid, &entry);
  if (x) return x;

  lock_acquire(entry->lock);

    KASSERT(entry != NULL);
    KASSERT(entry->pid == pid);

  // Check if parent
  if (curproc->pid != entry->parent->pid) {
    lock_release(entry->lock);
    return ECHILD;
  }

  while (!entry->isdead) {
    cv_wait(entry->exitcode_cv, entry->lock);
  }
  // encode and set status
  exitstatus = _MKWAIT_EXIT(entry->exitcode);
  result = copyout((void *)&exitstatus,status,sizeof(int));

  lock_release(entry->lock);

  if (result) return result;

  *retval = pid;
  return 0; // Success

  /*
  //
  //@@@@@=====@@@@@===== END OF waitpid implementation =====@@@@@=====@@@@@
  //
   */
#else
  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */
  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
#endif
}
