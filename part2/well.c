#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"

#ifdef VERBOSE
#define VERBOSE_PRINT(S, ...) printf(S, ##__VA_ARGS__);
#else
#define VERBOSE_PRINT(S, ...) ;
#endif

#define MAX_OCCUPANCY 3
#define NUM_ITERATIONS 100
#define NUM_PEOPLE 20
#define FAIR_WAITING_COUNT 4

/**
 * You might find these declarations useful.
 */
enum Endianness
{
  LITTLE = 0,
  BIG = 1
};
const static enum Endianness oppositeEnd[] = {BIG, LITTLE};

struct Well
{
  uthread_mutex_t mx;
  uthread_cond_t end_conds[2];
  int num_waiters[2];
  int num_occupants;
  int fairness_count;
  enum Endianness endianness;
};

struct Well *createWell()
{
  struct Well *Well = malloc(sizeof(struct Well));
  Well->mx = uthread_mutex_create();
  Well->end_conds[LITTLE] = uthread_cond_create(Well->mx);
  Well->end_conds[BIG] = uthread_cond_create(Well->mx);
  Well->num_waiters[LITTLE] = 0;
  Well->num_waiters[BIG] = 0;
  Well->num_occupants = 0;
  Well->fairness_count = 0;
  Well->endianness = 0;
  return Well;
}

struct Well *Well;

#define WAITING_HISTOGRAM_SIZE (NUM_ITERATIONS * NUM_PEOPLE)
int entryTicker; // incremented with each entry
int waitingHistogram[WAITING_HISTOGRAM_SIZE];
int waitingHistogramOverflow;
uthread_mutex_t waitingHistogrammutex;
int occupancyHistogram[2][MAX_OCCUPANCY + 1];

void enterWell(enum Endianness g)
{
  uthread_mutex_lock(Well->mx);
  while (1)
  {
    // initialize locals for access to vals
    int fair_count = Well->fairness_count;
    int occupant_count = Well->num_occupants;
    enum Endianness end = Well->endianness;

    // initialize Booleans for enter Well
    int empty = occupant_count == 0;
    int full = occupant_count >= MAX_OCCUPANCY;
    int unfair = fair_count >= FAIR_WAITING_COUNT;
    int currend_waiting = Well->num_waiters[g] > 0;
    int otherend_waiting = Well->num_waiters[oppositeEnd[g]] > 0;
    int currEnd = (g == end);

    if (empty || ((!otherend_waiting || !unfair) && currEnd && !full)) {
      if (currEnd) {
        Well->fairness_count++;
      }
      else {
        Well->fairness_count = 0;
      }
      entryTicker++;
      break;
    }
    else {
      if (!currEnd && !currend_waiting) {
        Well->fairness_count = 0;
      }
      Well->num_waiters[g]++;
      uthread_cond_wait(Well->end_conds[g]);
      Well->num_waiters[g]--;
    }
  }
    // Person enters Well, update occupancy
    Well->endianness = g;
    Well->num_occupants++;
    occupancyHistogram[Well->endianness][Well->num_occupants]++;
    uthread_mutex_unlock(Well->mx);
  }

  void leaveWell() {
    uthread_mutex_lock(Well->mx);
    // initiallizing Booleans for leave Well
    int currend_waiting = (Well->num_waiters[Well->endianness] > 0);
    int otherend_waiting = (Well->num_waiters[oppositeEnd[Well->endianness]] > 0);
    int unfair = Well->fairness_count >= FAIR_WAITING_COUNT;

    // Update Well occupancy
    Well->num_occupants--;

    // Signal
    if ((!currend_waiting || unfair) && otherend_waiting) {
      if (Well->num_occupants != 0 && currend_waiting) {
        uthread_cond_signal(Well->end_conds[currend_waiting]);
      }
      else if (Well->num_occupants == 0) {
        for (int i = 0; i < MAX_OCCUPANCY; i++) {
          uthread_cond_signal(Well->end_conds[oppositeEnd[Well->endianness]]);
        }
      }
    }
    uthread_mutex_unlock(Well->mx);
  }

  void recordWaitingTime(int waitingTime)
  {
    uthread_mutex_lock(waitingHistogrammutex);
    if (waitingTime < WAITING_HISTOGRAM_SIZE)
      waitingHistogram[waitingTime]++;
    else
      waitingHistogramOverflow++;
    uthread_mutex_unlock(waitingHistogrammutex);
  }

  void* endperson() {
    enum Endianness g = random() & 1;
    for (int i = 0; i < NUM_ITERATIONS; i++) {
      int entryTime = entryTicker;
      enterWell(g);
      recordWaitingTime(entryTicker - entryTime - 1);
      for (int j = 0; j < NUM_PEOPLE; j++) {
        uthread_yield();
      }
      leaveWell();
      for (int k = 0; k < NUM_PEOPLE; k++) {
        uthread_yield();
      }
    }
    return NULL;
  }

  int main(int argc, char **argv)
  {
    uthread_init(1);
    Well = createWell();
    uthread_t pt[NUM_PEOPLE];
    waitingHistogrammutex = uthread_mutex_create();

    // Create Threads to represent people
    for (int i = 0; i < NUM_PEOPLE; i++) {
      pt[i] = uthread_create(endperson, 0);
    }

    // Join Threads
    for (int j = 0; j < NUM_PEOPLE; j++) {
      uthread_join(pt[j], 0);
    }

    printf("Times with 1 little endian %d\n", occupancyHistogram[LITTLE][1]);
    printf("Times with 2 little endian %d\n", occupancyHistogram[LITTLE][2]);
    printf("Times with 3 little endian %d\n", occupancyHistogram[LITTLE][3]);
    printf("Times with 1 big endian    %d\n", occupancyHistogram[BIG][1]);
    printf("Times with 2 big endian    %d\n", occupancyHistogram[BIG][2]);
    printf("Times with 3 big endian    %d\n", occupancyHistogram[BIG][3]);
    printf("Waiting Histogram\n");
    for (int i = 0; i < WAITING_HISTOGRAM_SIZE; i++)
      if (waitingHistogram[i])
        printf("  Number of times people waited for %d %s to enter: %d\n", i, i == 1 ? "person" : "people", waitingHistogram[i]);
    if (waitingHistogramOverflow)
      printf("  Number of times people waited more than %d entries: %d\n", WAITING_HISTOGRAM_SIZE, waitingHistogramOverflow);
  }
