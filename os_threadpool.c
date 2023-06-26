#include "os_threadpool.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>

/* === QUEUE METHODS === */

sem_t tasks_semaphore;

// queue = head
// Any insert is in head->next

os_task_queue_t *my_queue_create(void) {
	os_task_queue_t *newQueue = malloc(sizeof(os_task_queue_t));
    if (newQueue == NULL) {
        printf("[ERROR] [my_queue_create] Not enought memory\n");
        return NULL;
    }

    newQueue->next = NULL;
	newQueue->task = NULL;

    return newQueue;
}

void my_queue_add(os_task_queue_t *queue, os_task_t *t) {
	struct _node *newNode = malloc(sizeof(struct _node));
    if (newNode == NULL) {
        printf("[ERROR] [my_queue_add] Not enought memory\n");
        return;
    }

    newNode->task = t;
	newNode->next = queue->next;

	queue->next = newNode;
}

struct _node *my_queue_get(os_task_queue_t *queue) {
	struct _node *target;

    target = queue->next;

	if (target != NULL) {
		queue->next = target->next;
	}

    return target;
}

/* === TASK === */

/* Creates a task that thread must execute */
os_task_t *task_create(void *arg, void (*f)(void *))
{
    os_task_t *t = malloc(sizeof(os_task_t));

	if (t == NULL) {
		printf("[ERROR] [task_create] Not enought memory\n");
        return NULL;
	}

	t->argument = arg;
	t->task = f;

	return t;
}

/* Add a new task to threadpool task queue */
void add_task_in_queue(os_threadpool_t *tp, os_task_t *t)
{
	// Check to see if the maximum number of tasks is reached
	sem_wait(&tasks_semaphore);

	pthread_mutex_lock(&tp->taskLock);

	my_queue_add(tp->tasks, t);

	pthread_mutex_unlock(&tp->taskLock);
}

/* Get the head of task queue from threadpool */
os_task_t *get_task(os_threadpool_t *tp)
{
	pthread_mutex_lock(&tp->taskLock);

	struct _node *target = my_queue_get(tp->tasks);

	// Update the number of available space for tasks
	sem_post(&tasks_semaphore);

	pthread_mutex_unlock(&tp->taskLock);

	if (target != NULL) {
		os_task_t *task = target->task;
		free(target);

		return task;
	} else {
		return NULL;
	}
}

/* === THREAD POOL === */

/* Initialize the new threadpool */
os_threadpool_t *threadpool_create(unsigned int nTasks, unsigned int nThreads)
{
    os_threadpool_t *tp = malloc(sizeof(os_threadpool_t));
	if (tp == NULL) {
		printf("[ERROR] [threadpool_create] Not enought memory\n");
		return NULL;
	}

	tp->tasks = my_queue_create();
	pthread_mutex_init(&tp->taskLock, NULL);

	tp->threads = malloc(sizeof(pthread_t) * nThreads);
	if (tp->threads == NULL) {
		printf("[ERROR] [threadpool_create] Not enought memory\n");
		return NULL;
	}

	tp->num_threads = nThreads;
	tp->should_stop = 0;

	sem_init(&tasks_semaphore, 0, nTasks);

    return tp;
}

/* Loop function for threads */
void *thread_loop_function(void *args)
{
	os_threadpool_t *tp = (os_threadpool_t *)args;

	while (1) {
		if (tp->should_stop == 1) {
			break;
		}

		os_task_t *task = get_task(tp);
		if (task != NULL) {
			task->task(task->argument);
			free(task);
		}
	}

    return NULL;
}

/* Stop the thread pool once a condition is met */
void threadpool_stop(os_threadpool_t *tp, int (*processingIsDone)(os_threadpool_t *))
{
    if (processingIsDone(tp)) {
		tp->should_stop = 1;

		for (int i = 0; i < tp->num_threads; i++) {
			pthread_join(tp->threads[i], NULL);
		}

		sem_destroy(&tasks_semaphore);
	}
}
