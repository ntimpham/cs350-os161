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
  int x = as_copy(parent->p_addrspace, &c_as);
  if (x) {
    as_destroy(c_as);
    proc_destroy(child);
    return x;
  }

  // Attach the newly created address space to the child process structure
  child->p_addrspace = c_as;

  // Create the parent/child relationship
  proc_table_lock_acquire();

  struct proc_table_entry *c_entry;
  proc_table_get(child->pid, &c_entry);
    KASSERT(c_entry != NULL);
    KASSERT(c_entry->proc == child);
    KASSERT(c_entry->pid == child->pid);
  struct proc_table_entry *p_entry;
  proc_table_get(parent->pid, &p_entry);
    KASSERT(p_entry != NULL);
    KASSERT(p_entry->proc == parent);
    KASSERT(p_entry->pid == parent->pid);

  c_entry->parent = p_entry;
  c_entry->numref++;

  x = array_add((struct array*)p_entry->children, c_entry, NULL);
  if (x) {
    proc_table_remove(child->pid);
    proc_table_lock_release();
    as_destroy(c_as);
    proc_destroy(child);
    return x;
  }
  p_entry->numref++;

  // Create thread for child process (need a safe way to pass the trapframe to
  // the child thread).
  struct trapframe *c_tf = kmalloc(sizeof(struct trapframe) );
  if (c_tf == NULL) {
    proc_table_remove(child->pid);
    proc_table_lock_release();
    as_destroy(c_as);
    proc_destroy(child);
    return ENOMEM;
  }
  *c_tf = *tf;
  x = thread_fork(curthread->t_name, child, enter_forked_process, c_tf, 0);
  if (x) {
    kfree(c_tf);
    proc_table_remove(child->pid);
    proc_table_lock_release();
    as_destroy(c_as);
    proc_destroy(child);
    return x;
  }

  proc_table_lock_release();

  *retval = child->pid;
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

  proc_table_lock_acquire();

  struct proc_table_entry *entry;
  proc_table_get(p->pid, &entry);
    KASSERT(entry != NULL);
    KASSERT(p == entry->proc);
    KASSERT(p->pid == entry->pid);

  // Parent update
  struct proc_table_entry *pe = (struct proc_table_entry*)entry->parent;
  if (pe != NULL) {
      KASSERT(pe->numref > 0);
    pe->numref--;

    // Check cleanup
    if (pe->isdead && pe->numref == 0) {
      proc_table_remove(pe->pid);
    }
  }

  // Children update
  struct array *ca = (struct array*)entry->children;
  for (unsigned i = 0; i < array_num(ca); i++) {
    struct proc_table_entry *ce = array_get(ca, i);
      KASSERT(ce != NULL);
      KASSERT(ce->numref > 0);
    ce->numref--;

    // Check cleanup
    if (ce->isdead && ce->numref == 0) {
      proc_table_remove(ce->pid);
    }
  }

  // Self update
  entry->isdead = true;
  entry->exitcode = exitcode;
  proc_table_broadcastfor(p->pid);
  // Check cleanup
  if (entry->numref == 0) {
    proc_table_remove(entry->pid);
  }

  proc_table_lock_release();

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

  proc_table_lock_acquire();

  struct proc_table_entry *ce;
  int x = proc_table_get(pid, &ce);
  if (x) return x;

    KASSERT(ce != NULL);
    KASSERT(ce->pid == pid);

  // Check if parent
  if (curproc->pid != ce->parent->pid) return ECHILD;

  x = proc_table_waiton(pid);
  if (x) {
    proc_table_lock_release();
    return x;
  }

  // encode and set status
  exitstatus = _MKWAIT_EXIT(ce->exitcode);

  proc_table_lock_release();

  result = copyout((void *)&exitstatus,status,sizeof(int));
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
