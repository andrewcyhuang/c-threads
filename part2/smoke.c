#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"

#define NUM_ITERATIONS 1000

#ifdef VERBOSE
#define VERBOSE_PRINT(S, ...) printf(S, ##__VA_ARGS__);
#else
#define VERBOSE_PRINT(S, ...) ;
#endif


/**
 * You might find these declarations helpful.
 *   Note that Resource enum had values 1, 2 and 4 so you can combine resources;
 *   e.g., having a MATCH and PAPER is the value MATCH | PAPER == 1 | 2 == 3
 */
enum Resource
{
  MATCH = 1,
  PAPER = 2,
  TOBACCO = 4
};
char *resource_name[] = {"", "match", "paper", "", "tobacco"};

int signal_count[5]; // # of times resource signalled
int smoke_count[5];  // # of times smoker with resource smoked

struct Agent
{
  uthread_mutex_t mutex;
  uthread_cond_t match;
  uthread_cond_t paper;
  uthread_cond_t tobacco;
  uthread_cond_t smoke;
};

struct Agent *createAgent()
{
  struct Agent *agent = malloc(sizeof(struct Agent));
  agent->mutex = uthread_mutex_create();
  agent->paper = uthread_cond_create(agent->mutex);
  agent->match = uthread_cond_create(agent->mutex);
  agent->tobacco = uthread_cond_create(agent->mutex);
  agent->smoke = uthread_cond_create(agent->mutex);
  return agent;
}

// Smoker struct
struct Smoker
{
  uthread_cond_t match_can_smoke;
  uthread_cond_t paper_can_smoke;
  uthread_cond_t tobacco_can_smoke;
  uthread_cond_t smoked;
  struct Agent *a;
  int match;
  int paper;
  int tobacco;
};

struct Smoker *createSmoker(void *agent)
{
  struct Smoker *smoker = malloc(sizeof(struct Smoker));
  smoker->a = agent;
  smoker->match_can_smoke = uthread_cond_create(smoker->a->mutex);
  smoker->paper_can_smoke = uthread_cond_create(smoker->a->mutex);
  smoker->tobacco_can_smoke = uthread_cond_create(smoker->a->mutex);
  smoker->smoked = uthread_cond_create(smoker->a->mutex);
  smoker->match = 0;
  smoker->paper = 0;
  smoker->tobacco = 0;
  VERBOSE_PRINT("smoker created\n");
  return smoker;
}

/**
 * This is the agent procedure.  It is complete and you shouldn't change it in
 * any material way.  You can re-write it if you like, but be sure that all it does
 * is choose 2 random reasources, signal their condition variables, and then wait
 * wait for a smoker to smoke.
 */
void *agent(void *av)
{
  struct Agent *a = av;
  static const int choices[] = {MATCH | PAPER, MATCH | TOBACCO, PAPER | TOBACCO};
  static const int matching_smoker[] = {TOBACCO, PAPER, MATCH};

  uthread_mutex_lock(a->mutex);
  for (int i = 0; i < NUM_ITERATIONS; i++)
  {
    int r = random() % 3;
    signal_count[matching_smoker[r]]++;
    int c = choices[r];
    if (c & MATCH)
    {
      VERBOSE_PRINT("match available\n");
      uthread_cond_signal(a->match);
    }
    if (c & PAPER)
    {
      VERBOSE_PRINT("paper available\n");
      uthread_cond_signal(a->paper);
    }
    if (c & TOBACCO)
    {
      VERBOSE_PRINT("tobacco available\n");
      uthread_cond_signal(a->tobacco);
    }
    VERBOSE_PRINT("agent is waiting for smoker to smoke\n");
    uthread_cond_wait(a->smoke);
  }
  uthread_mutex_unlock(a->mutex);
  return NULL;
}

// Smoker procedure
void *smoker(void *sv, uthread_cond_t wait_cond, int *resource_tracker)
{
  struct Smoker *s = (struct Smoker *)sv;
  //struct Agent *a = (struct Agent *)s->a;
  VERBOSE_PRINT("smoker procedure initiated\n");
  uthread_mutex_lock(s->a->mutex);
  while (1)
  {
    uthread_cond_wait(wait_cond);
    *resource_tracker = 1;

    if (s->paper && s->tobacco)
    {
      VERBOSE_PRINT("match case\n");
      s->paper = 0;
      s->tobacco = 0;
      VERBOSE_PRINT("about to send match_can_smoke signal\n");
      uthread_cond_signal(s->match_can_smoke);
      uthread_cond_wait(s->smoked);
      VERBOSE_PRINT("match signal smoke\n");
      uthread_cond_signal(s->a->smoke);
    }
    else if (s->match && s->tobacco)
    {
      VERBOSE_PRINT("paper case\n");
      s->match = 0;
      s->tobacco = 0;
      VERBOSE_PRINT("about to send paper_can_smoke signal\n");
      uthread_cond_signal(s->paper_can_smoke);
      uthread_cond_wait(s->smoked);
      VERBOSE_PRINT("paper signal smoke\n");
      uthread_cond_signal(s->a->smoke);
    }
    else if (s->paper && s->match)
    {
      VERBOSE_PRINT("tobacco case\n");
      s->paper = 0;
      s->match = 0;
      VERBOSE_PRINT("about to send tobacco_can_smoke signal\n");
      uthread_cond_signal(s->tobacco_can_smoke);
      uthread_cond_wait(s->smoked);
      VERBOSE_PRINT("tobacco signal smoke\n");
      uthread_cond_signal(s->a->smoke);
    }
  }
  uthread_mutex_unlock(s->a->mutex);
}

void *match_smoker(void *sv)
{
  struct Smoker *s = (struct Smoker *)sv;
  //struct Agent *a = (struct Agent *)s->a;
  smoker(s, s->a->match, &s->match);
  return NULL;
}

void *paper_smoker(void *sv)
{
  struct Smoker *s = (struct Smoker *)sv;
  //struct Agent *a = (struct Agent *)s->a;
  smoker(s, s->a->paper, &s->paper);
  return NULL;
}

void *tobacco_smoker(void *sv)
{
  struct Smoker *s = (struct Smoker *)sv;
  //struct Agent *a = (struct Agent *)s->a;
  smoker(s, s->a->tobacco, &s->tobacco);
  return NULL;
}

void *smokerHandler(void *sv, uthread_cond_t wait_cond, enum Resource type)
{
  struct Smoker *s = sv;
  //struct Agent *a = s->a;
  uthread_mutex_lock(s->a->mutex);
  while (1)
  {
    uthread_cond_wait(wait_cond);
    VERBOSE_PRINT("smokeHandler wait cond signalled\n");
    smoke_count[type]++;
    uthread_cond_signal(s->smoked);
  }
  uthread_mutex_unlock(s->a->mutex);
}

void *match_smoke(void *sv)
{
  struct Smoker *s = sv;
  smokerHandler(s, s->match_can_smoke, MATCH);
  return NULL;
}

void *paper_smoke(void *sv)
{
  struct Smoker *s = sv;
  smokerHandler(s, s->paper_can_smoke, PAPER);
  return NULL;
}

void *tobacco_smoke(void *sv)
{
  struct Smoker *s = sv;
  smokerHandler(s, s->tobacco_can_smoke, TOBACCO);
  return NULL;
}

int main(int argc, char **argv)
{
  uthread_init(7);
   // Create Agent
  struct Agent *a = createAgent();
  // init smoker
  struct Smoker *s = createSmoker(a);
  // init smoker procedure for each smoker
  uthread_create(match_smoker, s);
  uthread_create(paper_smoker, s);
  uthread_create(tobacco_smoker, s);
  // init smoker handler procedure for each smoker
  uthread_create(match_smoke, s);
  uthread_create(paper_smoke, s);
  uthread_create(tobacco_smoke, s);
  // Join agent thread
  uthread_join(uthread_create(agent, a), 0);
  assert(signal_count[MATCH] == smoke_count[MATCH]);
  assert(signal_count[PAPER] == smoke_count[PAPER]);
  assert(signal_count[TOBACCO] == smoke_count[TOBACCO]);
  assert(smoke_count[MATCH] + smoke_count[PAPER] + smoke_count[TOBACCO] == NUM_ITERATIONS);
  printf("Smoke counts: %d matches, %d paper, %d tobacco\n",
         smoke_count[MATCH], smoke_count[PAPER], smoke_count[TOBACCO]);
}