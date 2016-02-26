#include <types.h>
#include <kern/errno.h>
#include <proc.h>
#include <synch.h>
#include <proc_table.h>
#include "array.h"
#include "opt-A2.h"

static struct array *proc_table; // Key: pid, Val: proc
static struct lock *proc_table_lock;

// Helper functions
struct proc_table_entry* entry_create(void);
void entry_destroy(struct proc_table_entry *entry);

/**
 * adds the given process to process table
 * @param  proc   process to add to table
 * @param  retval destination for pid
 * @return        0 success, ESRCH
 */
int proc_table_add(struct proc *proc, pid_t *retval) {
  if (proc == NULL) return EINVAL;

    KASSERT(proc_table != NULL);
    KASSERT(proc_table_lock != NULL);
  lock_acquire(proc_table_lock);

  pid_t pid = 0;
  pid_t freepid = 0;
  // Check if already exists
  for (unsigned i = 1; i < array_num(proc_table); i++) {
    struct proc_table_entry *ret = array_get(proc_table, i);
    if (ret == NULL) {
      freepid = i;
    }
    else if (ret->proc == proc) {
      lock_acquire(ret->lock);
      pid = i;
      lock_release(ret->lock);
    }
  }

  // Search failed, add new proc
  if (pid == 0) {
    struct proc_table_entry *entry;

    entry = entry_create();
    if (entry == NULL) {
      lock_release(proc_table_lock);
      return ENOMEM;
    }

    if (freepid != 0) {
      pid = freepid;
      entry->proc = proc;
      entry->pid = pid;

      array_set(proc_table, pid, entry);
    }
    else {
      int y = array_add(proc_table, entry, (unsigned*)&pid);
      if (y) {
        lock_release(proc_table_lock);
        return y;
      }
    }
  }

  // Set return value if included
  if (retval != NULL) *retval = pid;

  lock_release(proc_table_lock);
  return 0; // Success
}

/**
 * removes the process from the process table
 * @param  pid process id of process to remove
 * @return     0 success, -1 error
 */
int proc_table_remove(pid_t pid) {
  if (pid <= 0 || (unsigned)pid >= array_num(proc_table)) {
    return EDOM;
  }

    KASSERT(proc_table != NULL);
    KASSERT(proc_table_lock != NULL);
  lock_acquire(proc_table_lock);

  entry_destroy(array_get(proc_table, pid) );
  array_set(proc_table, pid, NULL);

  lock_release(proc_table_lock);
  return 0; // Success
}

/**
 * gets the proc_table_entry of the given process
 * @param  pid    process id of entry to get
 * @param  retval destination to store entry
 * @return        0 success, -1 error
 */
int proc_table_get(pid_t pid, struct proc_table_entry **retval) {
  if (pid <= 0 || (unsigned)pid >= array_num(proc_table)) {
    return EDOM; // out of range
  }

    KASSERT(proc_table != NULL);
    KASSERT(proc_table_lock != NULL);
  lock_acquire(proc_table_lock);

  struct proc_table_entry *entry = array_get(proc_table, pid);
  if (entry == NULL) {
    lock_release(proc_table_lock);
    return ESRCH; // process not found
  }
  *retval = entry;

  lock_release(proc_table_lock);
  return 0; // Success
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
  // Set index 0 of pid_list to NULL
  int x = array_setsize(proc_table, 1);
  if (x != 0) {
    array_destroy(proc_table);
    panic(x + ": array_setsize for proc_table_init failed\n");
  }
  array_set(proc_table, 0, NULL);
  // Initilize proc_table_lock
  proc_table_lock = lock_create("proc_table_lock");
  if (proc_table_lock == NULL) {
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
  array_destroy(proc_table);
  lock_destroy(proc_table_lock);
}

/**
 * creates a new blank proc_table_entry
 * @return new blank proc_table_entry
 */
struct proc_table_entry *entry_create(void) {
  struct proc_table_entry *entry;

  kmalloc(sizeof(*entry));
  if (entry == NULL) return NULL;

  entry->proc = NULL;
  entry->pid = 0;
  entry->numref = 0;
  entry->isdead = false;
  entry->exitcode = 0;

  entry->exitcode_cv = cv_create("proc_table_entry_cv");
  if (entry->exitcode_cv == NULL) {
    kfree(entry);
    return NULL;
  }

  entry->lock = lock_create("proc_table_entry_lock");
  if (entry->lock == NULL) {
    cv_destroy(entry->exitcode_cv);
    kfree(entry);
    return NULL;
  }

  entry->parent = NULL;

  entry->children = array_create();
  if (entry->children == NULL) {
    lock_destroy(entry->lock);
    cv_destroy(entry->exitcode_cv);
    kfree(entry);
    return NULL;
  }

  return entry;
}

/**
 * destroys the given proc_table_entry
 * @param entry the proc_table_entry to destroy
 */
void entry_destroy(struct proc_table_entry *entry) {
  if (entry != NULL) {
    cv_destroy(entry->exitcode_cv);
    lock_destroy(entry->lock);
    array_destroy(entry->children);
    kfree(entry);
  }
}
