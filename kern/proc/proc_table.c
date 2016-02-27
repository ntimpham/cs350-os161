#include <types.h>
#include <kern/errno.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <limits.h>
#include <proc_table.h>
#include "array.h"
#include "opt-A2.h"

static struct array *proc_table; // Key: pid, Val: proc
static struct lock *proc_table_lock;

// Helper functions
int entry_create(struct proc_table_entry **retval);
void entry_destroy(struct proc_table_entry *entry);

/**
 * adds the given process to process table
 * @param  proc   process to add to table
 * @param  retval destination for pid
 * @return        error code
 */
int proc_table_add(struct proc *proc, pid_t *retval) {
  if (proc == NULL) return EINVAL;

    KASSERT(proc_table != NULL);
    KASSERT(proc_table_lock != NULL);
    KASSERT(lock_do_i_hold(proc_table_lock));

  pid_t pid = 0;
  struct proc_table_entry *entry;

  int x = entry_create(&entry);
  if (x) {
    return x;
  }

  // Check for reusable pid
  pid_t count = PID_MIN;
  for (unsigned i = 1; i < array_num(proc_table); i++) {
    struct proc_table_entry *tmp = array_get(proc_table, i);
    if (tmp == NULL) {
      pid = count;
      array_set(proc_table, i, entry);
      break;
    }
    count++;
  }

  // Need to add new pid
  if (pid == 0) {
    if (count > PID_MAX) {
      return ENPROC;
    }
    pid = count;
    array_add(proc_table, entry, NULL);
  }

    KASSERT(pid > 0);
  entry->proc = proc;
  entry->pid = pid;

  // Set return value if included
  if (retval != NULL) *retval = pid;

  return 0; // Success
}

/**
 * removes the process from the process table
 * @param  pid process id of process to remove
 * @return     error code
 */
int proc_table_remove(pid_t pid) {
  if (pid < PID_MIN || pid >= PID_MAX) {
    return EDOM;
  }

    KASSERT(proc_table != NULL);
    KASSERT(proc_table_lock != NULL);
    KASSERT(lock_do_i_hold(proc_table_lock));

  for (unsigned i = 0; i < array_num(proc_table); i++) {
    struct proc_table_entry *tmp = array_get(proc_table, i);
    if (tmp != NULL) {
      if (tmp->pid == pid) {
        entry_destroy(tmp);
        array_set(proc_table, i, NULL);

        return 0; // Success
      }
    }
  }

  return ESRCH;
}

/**
 * gets the proc_table_entry of the given process
 * @param  pid    process id of entry to get
 * @param  retval destination to store entry
 * @return        error code
 */
int proc_table_get(pid_t pid, struct proc_table_entry **retval) {
  if (pid < PID_MIN || pid >= PID_MAX) {
    return EDOM; // out of range
  }

    KASSERT(proc_table != NULL);
    KASSERT(proc_table_lock != NULL);
    KASSERT(lock_do_i_hold(proc_table_lock));

  struct proc_table_entry *entry = NULL;
  for (unsigned i = 0; i < array_num(proc_table); i++) {
    struct proc_table_entry *tmp = array_get(proc_table, i);
    if (tmp != NULL) {
      if (tmp->pid == pid) {
        entry = tmp;
        break;
      }
    }
  }
  if (entry == NULL) {
    return ESRCH; // process not found
  }
  *retval = entry;

  return 0; // Success
}

int proc_table_waiton(pid_t pid) {
  if (pid < PID_MIN || pid >= PID_MAX) {
    return EDOM; // out of range
  }

    KASSERT(proc_table != NULL);
    KASSERT(proc_table_lock != NULL);
    KASSERT(lock_do_i_hold(proc_table_lock));

  struct proc_table_entry *entry;
  int x = proc_table_get(pid, &entry);
  if (x) {
    return x;
  }
    KASSERT(entry != NULL);
    KASSERT(entry->pid == pid);

  // Check if parent
  if (entry->parent->pid != curproc->pid) {
    return ECHILD;
  }

  while (!entry->isdead) {
    cv_wait((struct cv*)entry->exitcode_cv, proc_table_lock);
  }

  return 0; // Success
}

int proc_table_broadcastfor(pid_t pid) {
  if (pid < PID_MIN || pid >= PID_MAX) {
    return EDOM; // out of range
  }

    KASSERT(proc_table != NULL);
    KASSERT(proc_table_lock != NULL);
    KASSERT(lock_do_i_hold(proc_table_lock));

  struct proc_table_entry *entry;
  int x = proc_table_get(pid, &entry);
  if (x) {
    return x;
  }
    KASSERT(entry != NULL);
    KASSERT(entry->pid == pid);
    KASSERT(entry->proc == curproc);

  cv_broadcast((struct cv*)entry->exitcode_cv, proc_table_lock);

  return 0; // Success
}

void proc_table_lock_acquire(void) {
    KASSERT(proc_table_lock != NULL);
  lock_acquire(proc_table_lock);
}

void proc_table_lock_release(void) {
    KASSERT(proc_table_lock != NULL);
  lock_release(proc_table_lock);
}

/**
 * creates the global process table
 */
void proc_table_init(void) {
  // Initialize proc_table
  proc_table = array_create();
  if (proc_table == NULL) {
    panic("array_create for proc_table_init failed\n");
  }

  // Add kernel proc
  struct proc_table_entry *entry;
  int x = entry_create(&entry);
  if (x) panic("entry_create for proc_table_init failed\n");
  entry->proc = kproc;
  entry->pid = 1;
  x = array_add(proc_table, entry, NULL);
  if (x) panic("array_add for proc_table_init failed\n");

  // Initialize proc_table_lock
  proc_table_lock = lock_create("proc_table_lock");
  if (proc_table_lock == NULL) {
    entry_destroy(entry);
    array_destroy(proc_table);
    panic("lock_create for proc_table_init failed\n");
  }
}

/**
 * destroys the global process table
 */
void proc_table_destroy(void) {
  for (unsigned i = 0; i < array_num(proc_table); i++) {
    entry_destroy(array_get(proc_table, i) );
  }
  unsigned size = array_num(proc_table);
  for (unsigned i = 0; i < size; i++) {
    array_remove(proc_table, 0);
  }
  array_destroy(proc_table);
  lock_destroy(proc_table_lock);
}

/**
 * creates a new blank proc_table_entry
 * @return new blank proc_table_entry
 */
int entry_create(struct proc_table_entry **retval) {
  if (retval == NULL) {
    return EINVAL;
  }

  struct proc_table_entry *entry;

  entry = kmalloc(sizeof(*entry));
  if (entry == NULL) return ENOMEM;

  entry->proc = NULL;
  entry->pid = 0;
  entry->numref = 0;
  entry->isdead = false;
  entry->exitcode = 0;

  entry->exitcode_cv = cv_create("proc_table_entry_cv");
  if (entry->exitcode_cv == NULL) {
    kfree(entry);
    return ENOMEM;
  }

  entry->parent = NULL;

  entry->children = array_create();
  if (entry->children == NULL) {
    cv_destroy((struct cv*)entry->exitcode_cv);
    kfree(entry);
    return ENOMEM;
  }

  *retval = entry;
  return 0; // Success
}

/**
 * destroys the given proc_table_entry
 * @param entry the proc_table_entry to destroy
 */
void entry_destroy(struct proc_table_entry *entry) {
  if (entry != NULL) {
    cv_destroy((struct cv*)entry->exitcode_cv);
    unsigned size = array_num((struct array*)entry->children);
    for (unsigned i = 0; i < size; i++) {
      array_remove((struct array*)entry->children, 0);
    }
    array_destroy((struct array*)entry->children);
    kfree(entry);
  }
}
