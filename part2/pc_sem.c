#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"
#include "uthread_sem.h"

#define MAX_ITEMS      10
#define NUM_ITERATIONS 200
#define NUM_PRODUCERS  2
#define NUM_CONSUMERS  2
#define NUM_PROCESSORS 4

uthread_sem_t mx;
uthread_sem_t availableSpace;
uthread_sem_t takenSpace;
uthread_sem_t complete;

// histogram [i] == # of times list stored i items
int histogram [MAX_ITEMS+1]; 

// number of items currently produced but not yet consumed
// invariant that you must maintain: 0 >= items >= MAX_ITEMS
int items = 0;

// if necessary wait until items < MAX_ITEMS and then increment items
// assertion checks the invariant that 0 >= items >= MAX_ITEMS
void* producer (void* v) {
  for (int i=0; i<NUM_ITERATIONS; i++) {
    uthread_sem_wait(availableSpace);
    uthread_sem_wait(mx);
    items += 1;
    assert(items >= 0 && items <= MAX_ITEMS);
    histogram [items] ++;
    uthread_sem_signal(mx);
    uthread_sem_signal(takenSpace);
  }
  uthread_sem_signal(complete);
  return NULL;
}

// if necessary wait until items > 0 and then decrement items
// assertion checks the invariant that 0 >= items >= MAX_ITEMS
void* consumer (void* v) {
  for (int i=0; i<NUM_ITERATIONS; i++) {
    uthread_sem_wait(takenSpace);
    uthread_sem_wait(mx);
    items -= 1;
    assert(items >= 0 && items <= MAX_ITEMS);
    histogram [items] ++;
    uthread_sem_signal(mx);
    uthread_sem_signal(availableSpace);
  }
  uthread_sem_signal(complete);
  return NULL;
}

int main (int argc, char** argv) {

  // init the thread system
  uthread_init (NUM_PROCESSORS);

  // init semiphores
  mx = uthread_sem_create(1);
  availableSpace = uthread_sem_create(MAX_ITEMS);
  takenSpace = uthread_sem_create(0);
  complete = uthread_sem_create(0);

  // start the threads
  uthread_t threads [NUM_PRODUCERS + NUM_CONSUMERS];
  for (int i = 0; i < NUM_PRODUCERS; i++)
    threads [i] = uthread_create (producer, 0);
  for (int i = NUM_PRODUCERS; i < NUM_PRODUCERS + NUM_CONSUMERS; i++)  
    threads [i] = uthread_create (consumer, 0);

  // wait for threads to complete
  for (int i=0; i < NUM_PRODUCERS + NUM_CONSUMERS; i++)
    uthread_sem_wait(complete);

  // sum up
  printf ("items value histogram:\n");
  int sum=0;
  for (int i = 0; i <= MAX_ITEMS; i++) {
    printf ("  items=%d, %d times\n", i, histogram [i]);
    sum += histogram [i];
  }
  // checks invariant that ever change to items was recorded in histogram exactly one
  assert (sum == (NUM_PRODUCERS + NUM_CONSUMERS) * NUM_ITERATIONS);
}
