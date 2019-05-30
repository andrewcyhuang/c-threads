#include <stdlib.h>
#include <stdio.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"

#define NUM_THREADS 3
uthread_t threads[NUM_THREADS];

uthread_mutex_t mx;
uthread_cond_t run_other_threads;
int num_ran = 1;

void randomStall() {
  int i, r = random() >> 16;
  while (i++<r);
}

void waitForAllOtherThreads() {
  while (num_ran<NUM_THREADS) {
    num_ran++;
    uthread_cond_wait(run_other_threads);
  }
  uthread_cond_broadcast(run_other_threads);
}

void* p(void* v) {
  randomStall();
  uthread_mutex_lock(mx);
  printf("a\n");
  waitForAllOtherThreads();
  uthread_mutex_unlock(mx);
  printf("b\n");
  return NULL;
}

int main(int arg, char** arv) {
  uthread_init(4);
  mx = uthread_mutex_create();
  run_other_threads = uthread_cond_create(mx);
  for (int i = 0; i < NUM_THREADS; i++)
    threads[i] = uthread_create(p, NULL);
  for (int i=0; i<NUM_THREADS; i++)
    uthread_join (threads[i], NULL);
  printf("------\n");
}