#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/*
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/*
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
static struct lock *intersectionLock;
static struct cv *
  queue_ne, queue_ns, queue_nw, // North source
  queue_en, queue_es, queue_ew, // East source
  queue_sn, queue_se, queue_sw, // South source
  queue_wn, queue_we, queue_ws; // West source

/**
 * Holds the number of cars in the intersection that are blocking a certain direction
 */
static volatile int
  block_ne, block_ns, block_nw, // North source
  block_en, block_es, block_ew, // East source
  block_sn, block_se, block_sw, // South source
  block_wn, block_we, block_ws; // West source



/*
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 *
 */
void
intersection_sync_init(void)
{
  // Create lock
  intersectionLock = lock_create("intersectionLock");
  if (intersectionLock == NULL) {
    panic("could not create intersection lock");
  }

  // Create CVs
  queue_ne = cv_create("northEastCV");
  if (queue_ne == NULL) {
    panic("could not create NE CV");
    // Clean up
    lock_destroy(intersectionLock);
  }
  queue_ns = cv_create("northSouthCV");
  if (queue_ns == NULL) {
    panic("could not create NS CV");
    // Clean up
    lock_destroy(intersectionLock);
    cv_destroy(queue_ne);
  }
  queue_nw = cv_create("northWestCV");
  if (queue_nw == NULL) {
    panic("could not create NW CV");
    // Clean up
    lock_destroy(intersectionLock);
    cv_destroy(queue_ne);
    cv_destroy(queue_ns);
  }
  queue_en = cv_create("eastNorthCV");
  if (queue_en == NULL) {
    panic("could not create EN CV");
    // Clean up
    lock_destroy(intersectionLock);
    cv_destroy(queue_ne);
    cv_destroy(queue_ns);
    cv_destroy(queue_nw);
  }
  queue_es = cv_create("eastSouthCV");
  if (queue_es == NULL) {
    panic("could not create ES CV");
    // Clean up
    lock_destroy(intersectionLock);
    cv_destroy(queue_ne);
    cv_destroy(queue_ns);
    cv_destroy(queue_nw);
    cv_destroy(queue_en);
  }
  queue_ew = cv_create("eastWestCV");
  if (queue_ew == NULL) {
    panic("could not create EW CV");
    // Clean up
    lock_destroy(intersectionLock);
    cv_destroy(queue_ne);
    cv_destroy(queue_ns);
    cv_destroy(queue_nw);
    cv_destroy(queue_en);
    cv_destroy(queue_es);
  }
  queue_sn = cv_create("southNorthCV");
  if (queue_sn == NULL) {
    panic("could not create SN CV");
    // Clean up
    lock_destroy(intersectionLock);
    cv_destroy(queue_ne);
    cv_destroy(queue_ns);
    cv_destroy(queue_nw);
    cv_destroy(queue_en);
    cv_destroy(queue_es);
    cv_destroy(queue_ew);
  }
  queue_se = cv_create("southEastCV");
  if (queue_se == NULL) {
    panic("could not create SE CV");
    // Clean up
    lock_destroy(intersectionLock);
    cv_destroy(queue_ne);
    cv_destroy(queue_ns);
    cv_destroy(queue_nw);
    cv_destroy(queue_en);
    cv_destroy(queue_es);
    cv_destroy(queue_ew);
    cv_destroy(queue_sn);
  }
  queue_sw = cv_create("southWestCV");
  if (queue_sw == NULL) {
    panic("could not create SW CV");
    // Clean up
    lock_destroy(intersectionLock);
    cv_destroy(queue_ne);
    cv_destroy(queue_ns);
    cv_destroy(queue_nw);
    cv_destroy(queue_en);
    cv_destroy(queue_es);
    cv_destroy(queue_ew);
    cv_destroy(queue_sn);
    cv_destroy(queue_se);
  }
  queue_wn = cv_create("westNorthCV");
  if (queue_wn == NULL) {
    panic("could not create WN CV");
    // Clean up
    lock_destroy(intersectionLock);
    cv_destroy(queue_ne);
    cv_destroy(queue_ns);
    cv_destroy(queue_nw);
    cv_destroy(queue_en);
    cv_destroy(queue_es);
    cv_destroy(queue_ew);
    cv_destroy(queue_sn);
    cv_destroy(queue_se);
    cv_destroy(queue_sw);
  }
  queue_we = cv_create("westEastCV");
  if (queue_we == NULL) {
    panic("could not create WE CV");
    // Clean up
    lock_destroy(intersectionLock);
    cv_destroy(queue_ne);
    cv_destroy(queue_ns);
    cv_destroy(queue_nw);
    cv_destroy(queue_en);
    cv_destroy(queue_es);
    cv_destroy(queue_ew);
    cv_destroy(queue_sn);
    cv_destroy(queue_se);
    cv_destroy(queue_sw);
    cv_destroy(queue_wn);
  }
  queue_ws = cv_create("westSouthCV");
  if (queue_ws == NULL) {
    panic("could not create WS CV");
    // Clean up
    lock_destroy(intersectionLock);
    cv_destroy(queue_ne);
    cv_destroy(queue_ns);
    cv_destroy(queue_nw);
    cv_destroy(queue_en);
    cv_destroy(queue_es);
    cv_destroy(queue_ew);
    cv_destroy(queue_sn);
    cv_destroy(queue_se);
    cv_destroy(queue_sw);
    cv_destroy(queue_wn);
    cv_destroy(queue_we);
  }

  // Set initial value of blocks to 0
  // North source
  block_ne = 0;
  block_ns = 0;
  block_nw = 0;

  // East source
  block_en = 0;
  block_es = 0;
  block_ew = 0;

  // South source
  block_sn = 0;
  block_se = 0;
  block_sw = 0;

  // West source
  block_wn = 0;
  block_we = 0;
  block_ws = 0;

  return;
}

/*
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
    KASSERT(intersectionLock != NULL);
  lock_destroy(intersectionLock);
    KASSERT(queue_ne != NULL);
  cv_destroy(queue_ne);
    KASSERT(queue_ns != NULL);
  cv_destroy(queue_ns);
    KASSERT(queue_nw != NULL);
  cv_destroy(queue_nw);
    KASSERT(queue_en != NULL);
  cv_destroy(queue_en);
    KASSERT(queue_es != NULL);
  cv_destroy(queue_es);
    KASSERT(queue_ew != NULL);
  cv_destroy(queue_ew);
    KASSERT(queue_sn != NULL);
  cv_destroy(queue_sn);
    KASSERT(queue_se != NULL);
  cv_destroy(queue_se);
    KASSERT(queue_sw != NULL);
  cv_destroy(queue_sw);
    KASSERT(queue_wn != NULL);
  cv_destroy(queue_wn);
    KASSERT(queue_we != NULL);
  cv_destroy(queue_we);
    KASSERT(queue_ws != NULL);
  cv_destroy(queue_ws);
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination)
{
    KASSERT(intersectionLock != NULL);
  lock_acquire(intersectionLock);

    KASSERT(origin != NULL);
    KASSERT(destination != NULL);
  /**
   * 1. Check if the path is free
   * 2a. If not, enter queue to wait
   * 2b. If yes, update the paths that will be blocked
   */

  /**
   * North origin
   */
  if (origin == Direction.north && destination == Direction.east) {
    while (block_ne > 0) {
        KASSERT(queue_ne != NULL);
      cv_wait(queue_ne, intersectionLock);
    }
      KASSERT(block_es >= 0);
      KASSERT(block_ew >= 0);
      KASSERT(block_sn >= 0);
      KASSERT(block_se >= 0);
      KASSERT(block_sw >= 0);
      KASSERT(block_wn >= 0);
      KASSERT(block_we >= 0);
    block_es++;
    block_ew++;
    block_sn++;
    block_se++;
    block_sw++;
    block_wn++;
    block_we++;
  }
  else if (origin == Direction.north && destination == Direction.south) {
    while (block_ns > 0) {
        KASSERT(queue_ns != NULL);
      cv_wait(queue_ns, intersectionLock);
    }
      KASSERT(block_es >= 0);
      KASSERT(block_ew >= 0);
      KASSERT(block_sw >= 0);
      KASSERT(block_wn >= 0);
      KASSERT(block_ws >= 0);
      KASSERT(block_we >= 0);
    block_es++;
    block_ew++;
    block_sw++;
    block_wn++;
    block_ws++;
    block_we++;
  }
  else if (origin == Direction.north && destination == Direction.west) {
    while (block_nw > 0) {
        KASSERT(queue_nw != NULL);
      cv_wait(queue_nw, intersectionLock);
    }
      KASSERT(block_ew >= 0);
      KASSERT(block_sw >= 0);
    block_ew++;
    block_sw++;
  }
  /**
   * East origin
   */
  else if (origin == Direction.east && destination == Direction.north) {
    while (block_en > 0) {
        KASSERT(queue_en != NULL);
      cv_wait(queue_en, intersectionLock);
    }
      KASSERT(block_sn >= 0);
      KASSERT(block_wn >= 0);
    block_sn++;
    block_wn++;
  }
  else if (origin == Direction.east && destination == Direction.south) {
    while (block_es > 0) {
        KASSERT(queue_es != NULL);
      cv_wait(queue_es, intersectionLock);
    }
      KASSERT(block_ne >= 0);
      KASSERT(block_ns >= 0);
      KASSERT(block_sn >= 0);
      KASSERT(block_sw >= 0);
      KASSERT(block_wn >= 0);
      KASSERT(block_we >= 0);
      KASSERT(block_ws >= 0);
    block_ne++;
    block_ns++;
    block_sn++;
    block_sw++;
    block_wn++;
    block_we++;
    block_ws++;
  }
  else if (origin == Direction.east && destination == Direction.west) {
    while (block_ew > 0) {
        KASSERT(queue_ew != NULL);
      cv_wait(queue_ew, intersectionLock);
    }
      KASSERT(block_ne >= 0);
      KASSERT(block_ns >= 0);
      KASSERT(block_nw >= 0);
      KASSERT(block_sn >= 0);
      KASSERT(block_sw >= 0);
      KASSERT(block_wn >= 0);
    block_ne++;
    block_ns++;
    block_nw++;
    block_sn++;
    block_sw++;
    block_wn++;
  }
  /**
   * South
   */
  else if (origin == Direction.south && destination == Direction.north) {
    while (block_sn > 0) {
        KASSERT(queue_sn != NULL);
      cv_wait(queue_sn, intersectionLock);
    }
      KASSERT(block_ne >= 0);
      KASSERT(block_en >= 0);
      KASSERT(block_es >= 0);
      KASSERT(block_ew >= 0);
      KASSERT(block_wn >= 0);
      KASSERT(block_we >= 0);
    block_ne++;
    block_en++;
    block_es++;
    block_ew++;
    block_wn++;
    block_we++;
  }
  else if (origin == Direction.south && destination == Direction.east) {
    while (block_se > 0) {
        KASSERT(queue_se != NULL);
      cv_wait(queue_se, intersectionLock);
    }
      KASSERT(block_ne >= 0);
      KASSERT(block_we >= 0);
    block_ne++;
    block_we++;
  }
  else if (origin == Direction.south && destination == Direction.west) {
    while (block_sw > 0) {
        KASSERT(queue_sw != NULL);
      cv_wait(queue_sw, intersectionLock);
    }
      KASSERT(block_ne >= 0);
      KASSERT(block_ns >= 0);
      KASSERT(block_nw >= 0);
      KASSERT(block_es >= 0);
      KASSERT(block_ew >= 0);
      KASSERT(block_wn >= 0);
      KASSERT(block_we >= 0);
    block_ne++;
    block_ns++;
    block_nw++;
    block_es++;
    block_ew++;
    block_wn++;
    block_we++;
  }
  /**
   * West
   */
  else if (origin == Direction.west && destination == Direction.north) {
    while (block_wn > 0) {
        KASSERT(queue_wn != NULL);
      cv_wait(queue_wn, intersectionLock);
    }
      KASSERT(block_ne >= 0);
      KASSERT(block_ns >= 0);
      KASSERT(block_en >= 0);
      KASSERT(block_es >= 0);
      KASSERT(block_ew >= 0);
      KASSERT(block_sn >= 0);
      KASSERT(block_sw >= 0);
    block_ne++;
    block_ns++;
    block_en++;
    block_es++;
    block_ew++;
    block_sn++;
    block_sw++;
  }
  else if (origin == Direction.west && destination == Direction.east) {
    while (block_we > 0) {
        KASSERT(queue_we != NULL);
      cv_wait(queue_we, intersectionLock);
    }
      KASSERT(block_ne >= 0);
      KASSERT(block_ns >= 0);
      KASSERT(block_es >= 0);
      KASSERT(block_sn >= 0);
      KASSERT(block_se >= 0);
      KASSERT(block_sw >= 0);
    block_ne++;
    block_ns++;
    block_es++;
    block_sn++;
    block_se++;
    block_sw++;
  }
  else if (origin == Direction.west && destination == Direction.south) {
    while (block_ws > 0) {
        KASSERT(queue_ws != NULL);
      cv_wait(queue_ws, intersectionLock);
    }
      KASSERT(block_ns >= 0);
      KASSERT(block_es >= 0);
    block_ns++;
    block_es++;
  }
  /**
   * BAD INPUT
   */
  else {
    panic("invalid Direction parameters were used");
  }

  lock_release(intersectionLock);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination)
{
    KASSERT(intersectionLock != NULL);
  lock_acquire(intersectionLock);

    KASSERT(origin != NULL);
    KASSERT(destination != NULL);
  /**
   * 1. Update blocked paths
   * 2. Wake up blocked cars
   */

  /**
   * North origin
   */
  if (origin == Direction.north && destination == Direction.east) {
      KASSERT(block_es > 0);
      KASSERT(block_ew > 0);
      KASSERT(block_sn > 0);
      KASSERT(block_se > 0);
      KASSERT(block_sw > 0);
      KASSERT(block_wn > 0);
      KASSERT(block_we > 0);
    block_es--;
    block_ew--;
    block_sn--;
    block_se--;
    block_sw--;
    block_wn--;
    block_we--;
    cv_signal(queue_es, intersectionLock);
    cv_signal(queue_ew, intersectionLock);
    cv_signal(queue_sn, intersectionLock);
    cv_signal(queue_se, intersectionLock);
    cv_signal(queue_sw, intersectionLock);
    cv_signal(queue_wn, intersectionLock);
    cv_signal(queue_we, intersectionLock);
  }
  else if (origin == Direction.north && destination == Direction.south) {
      KASSERT(block_es > 0);
      KASSERT(block_ew > 0);
      KASSERT(block_sw > 0);
      KASSERT(block_wn > 0);
      KASSERT(block_ws > 0);
      KASSERT(block_we > 0);
    block_es--;
    block_ew--;
    block_sw--;
    block_wn--;
    block_ws--;
    block_we--;
    cv_signal(queue_es, intersectionLock);
    cv_signal(queue_ew, intersectionLock);
    cv_signal(queue_sw, intersectionLock);
    cv_signal(queue_wn, intersectionLock);
    cv_signal(queue_ws, intersectionLock);
    cv_signal(queue_we, intersectionLock);
  }
  else if (origin == Direction.north && destination == Direction.west) {
      KASSERT(block_ew > 0);
      KASSERT(block_sw > 0);
    block_ew--;
    block_sw--;
    cv_signal(queue_ew, intersectionLock);
    cv_signal(queue_sw, intersectionLock);
  }
  /**
   * East origin
   */
  else if (origin == Direction.east && destination == Direction.north) {
      KASSERT(block_sn > 0);
      KASSERT(block_wn > 0);
    block_sn--;
    block_wn--;
    cv_signal(queue_sn, intersectionLock);
    cv_signal(queue_wn, intersectionLock);
  }
  else if (origin == Direction.east && destination == Direction.south) {
      KASSERT(block_ne > 0);
      KASSERT(block_ns > 0);
      KASSERT(block_sn > 0);
      KASSERT(block_sw > 0);
      KASSERT(block_wn > 0);
      KASSERT(block_we > 0);
      KASSERT(block_ws > 0);
    block_ne--;
    block_ns--;
    block_sn--;
    block_sw--;
    block_wn--;
    block_we--;
    block_ws--;
    cv_signal(queue_ne, intersectionLock);
    cv_signal(queue_ns, intersectionLock);
    cv_signal(queue_sn, intersectionLock);
    cv_signal(queue_sw, intersectionLock);
    cv_signal(queue_wn, intersectionLock);
    cv_signal(queue_we, intersectionLock);
    cv_signal(queue_ws, intersectionLock);
  }
  else if (origin == Direction.east && destination == Direction.west) {
      KASSERT(block_ne > 0);
      KASSERT(block_ns > 0);
      KASSERT(block_nw > 0);
      KASSERT(block_sn > 0);
      KASSERT(block_sw > 0);
      KASSERT(block_wn > 0);
    block_ne--;
    block_ns--;
    block_nw--;
    block_sn--;
    block_sw--;
    block_wn--;
    cv_signal(queue_ne, intersectionLock);
    cv_signal(queue_ns, intersectionLock);
    cv_signal(queue_nw, intersectionLock);
    cv_signal(queue_sn, intersectionLock);
    cv_signal(queue_sw, intersectionLock);
    cv_signal(queue_wn, intersectionLock);
  }
  /**
   * South
   */
  else if (origin == Direction.south && destination == Direction.north) {
      KASSERT(block_ne > 0);
      KASSERT(block_en > 0);
      KASSERT(block_es > 0);
      KASSERT(block_ew > 0);
      KASSERT(block_wn > 0);
      KASSERT(block_we > 0);
    block_ne--;
    block_en--;
    block_es--;
    block_ew--;
    block_wn--;
    block_we--;
    cv_signal(queue_ne, intersectionLock);
    cv_signal(queue_en, intersectionLock);
    cv_signal(queue_es, intersectionLock);
    cv_signal(queue_ew, intersectionLock);
    cv_signal(queue_wn, intersectionLock);
    cv_signal(queue_we, intersectionLock);
  }
  else if (origin == Direction.south && destination == Direction.east) {
      KASSERT(block_ne > 0);
      KASSERT(block_we > 0);
    block_ne--;
    block_we--;
    cv_signal(queue_ne, intersectionLock);
    cv_signal(queue_we, intersectionLock);
   }
   else if (origin == Direction.south && destination == Direction.west) {
      KASSERT(block_ne > 0);
      KASSERT(block_ns > 0);
      KASSERT(block_nw > 0);
      KASSERT(block_es > 0);
      KASSERT(block_ew > 0);
      KASSERT(block_wn > 0);
      KASSERT(block_we > 0);
    block_ne--;
    block_ns--;
    block_nw--;
    block_es--;
    block_ew--;
    block_wn--;
    block_we--;
    cv_signal(queue_ne, intersectionLock);
    cv_signal(queue_ns, intersectionLock);
    cv_signal(queue_nw, intersectionLock);
    cv_signal(queue_es, intersectionLock);
    cv_signal(queue_ew, intersectionLock);
    cv_signal(queue_wn, intersectionLock);
    cv_signal(queue_we, intersectionLock);
  }
  /**
   * West
   */
  else if (origin == Direction.west && destination == Direction.north) {
      KASSERT(block_ne > 0);
      KASSERT(block_ns > 0);
      KASSERT(block_en > 0);
      KASSERT(block_es > 0);
      KASSERT(block_ew > 0);
      KASSERT(block_sn > 0);
      KASSERT(block_sw > 0);
    block_ne--;
    block_ns--;
    block_en--;
    block_es--;
    block_ew--;
    block_sn--;
    block_sw--;
    cv_signal(queue_ne, intersectionLock);
    cv_signal(queue_ns, intersectionLock);
    cv_signal(queue_en, intersectionLock);
    cv_signal(queue_es, intersectionLock);
    cv_signal(queue_ew, intersectionLock);
    cv_signal(queue_sn, intersectionLock);
    cv_signal(queue_sw, intersectionLock);
  }
  else if (origin == Direction.west && destination == Direction.east) {
      KASSERT(block_ne > 0);
      KASSERT(block_ns > 0);
      KASSERT(block_es > 0);
      KASSERT(block_sn > 0);
      KASSERT(block_se > 0);
      KASSERT(block_sw > 0);
    block_ne--;
    block_ns--;
    block_es--;
    block_sn--;
    block_se--;
    block_sw--;
    cv_signal(queue_ne, intersectionLock);
    cv_signal(queue_ns, intersectionLock);
    cv_signal(queue_es, intersectionLock);
    cv_signal(queue_sn, intersectionLock);
    cv_signal(queue_se, intersectionLock);
    cv_signal(queue_sw, intersectionLock);
  }
  else if (origin == Direction.west && destination == Direction.south) {
      KASSERT(block_ns > 0);
      KASSERT(block_es > 0);
    block_ns--;
    block_es--;
    cv_signal(queue_ns, intersectionLock);
    cv_signal(queue_es, intersectionLock);
  }
  /**
   * BAD INPUT
   */
  else {
    panic("invalid Direction parameters were used");
  }

  lock_release(intersectionLock);
}
