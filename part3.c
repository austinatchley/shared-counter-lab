#define _GNU_SOURCE

#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sched.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/sysinfo.h>

volatile int64_t *counter;
int64_t max_counter;
int64_t *inc_counters;

int64_t *load_difference;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int cores;
bool pin_on_one_core;

void *worker_thread(void *idx);
void Create(pthread_t *thread, const pthread_attr_t *attr,
           void *(*start_routine) (void *), void *arg);
void Join(pthread_t thread, void **retval);
void timer_handler(void);
void do_counter(int num_workers);
void Set_affinity(pthread_t thread, size_t cpusetsize,
    const cpu_set_t *cpuset);

int main(int argc, char** argv)
{
  if(argc < 4)
  {
    printf("lab1 --counter --workers --pin-on-one-core --optional:iterations\n");
    return 1;
  }

  counter = malloc(sizeof(int));
  *counter = 0;
 
  max_counter = atoi(argv[1]);
  printf("Max counter: %li\n", max_counter);
 
  int num_workers = atoi(argv[2]);
  printf("Num workers: %d\n", num_workers);
  inc_counters = malloc(num_workers * sizeof(int64_t));

  pin_on_one_core = atoi(argv[3]) != 0;
  printf("Pinned to one core %s\n", pin_on_one_core ? "ENABLED" : "DISABLED");
 
  int iterations = 1;
  if(argc > 4)
    iterations = atoi(argv[4]);

  printf("Num iterations: %d\n", iterations);

  load_difference = malloc(num_workers * sizeof(int64_t));

  cores = get_nprocs();
  if(cores < 1)
    fprintf(stderr, "Error with cores");
  printf("Num cores: %d\n\n", cores);

  struct timeval start, stop;
  gettimeofday(&start, NULL);
  
  for(int i = 0; i < iterations; i++)
    do_counter(num_workers);

  gettimeofday(&stop, NULL);

  double diff = stop.tv_usec - start.tv_usec;
  double avg = diff / ((double) iterations);

  printf("Total Time: %f microseconds\nAverage: %f microseconds\n",
         diff, avg);
}

void do_counter(int num_workers)
{ 
  // Create an array to store thread ptrs
  pthread_t *threads = malloc(num_workers * sizeof(pthread_t));
  
  // Create threads to execute worker_thread()
  for(int i = 0; i < num_workers; i++)
  {
    int *j = malloc(sizeof(int));
    if(j == NULL)
      exit(0);
    
    *j=i;
    Create(&threads[i], NULL, worker_thread, (void *) j);
  }

  double sum = 0;

  // Join threads
  for(int i = 0; i < num_workers; i++)
  {
    Join(threads[i], NULL);
    printf("Increments %d:\t%li\n", i, inc_counters[i]);
    sum += inc_counters[i];
    load_difference[i] = abs((max_counter / num_workers) - inc_counters[i]);
  }

  double average_lost_ratio = sum / ((double)max_counter);
  printf("Ratio of lost updates:\t\t%f\n", average_lost_ratio);

  int64_t load_diff_sum = 0;
  for(int i = 0; i < num_workers; i++)
    load_diff_sum += load_difference[i];

  double average_load_diff = ((double) load_diff_sum  / (double) num_workers);
  printf("Average load difference:\t%f\n", average_load_diff);
}

void *worker_thread(void *idx)
{
  int index = *((int *) idx);
  
  cpu_set_t *cpuset = malloc(sizeof(cpu_set_t));	
  CPU_ZERO(cpuset);
  if(pin_on_one_core)
	CPU_SET(0, cpuset);
  else
    CPU_SET(index % cores, cpuset);

  Set_affinity(pthread_self(), sizeof(cpu_set_t), cpuset);

  while(*counter < max_counter)
  {
	pthread_mutex_lock(&lock);
    (*counter)++;
    inc_counters[index]++;
    pthread_mutex_unlock(&lock);
  }
  return NULL;
}

void Create(pthread_t *thread, const pthread_attr_t *attr,
           void *(*start_routine) (void *), void *arg)
{
  int result = pthread_create(thread, attr, start_routine, arg);
  
  if(result)
    //TODO: ERROR HANDLING
    fprintf(stderr, "ERROR: Creating thread\n");
}

void Join(pthread_t thread, void **retval)
{
  int result = pthread_join(thread, retval);
  
  if(result)
    //TODO: ERROR HANDLING
    fprintf(stderr, "ERROR: Joining thread\n");
}

void Set_affinity(pthread_t thread, size_t cpusetsize,
    const cpu_set_t *cpuset) {
  int result = pthread_setaffinity_np(thread, cpusetsize, cpuset);

  if(result)
    fprintf(stderr, "ERROR: Setting affinity");
}
