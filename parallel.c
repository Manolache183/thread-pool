#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "os_list.h"

#define MAX_TASK 1000
#define MAX_THREAD 4

int sum = 0;
int n_visited = 0;

os_graph_t *graph;
os_threadpool_t *tp;

unsigned int *connex_visited;

pthread_mutex_t sum_lock;
pthread_mutex_t n_visited_lock;
pthread_mutex_t inc_mutex;

void mark_conex_component(int id) {
	connex_visited[id] = 1;
	os_node_t *node = graph->nodes[id];

	for (int i = 0; i < node->cNeighbours; i++) {
		if (connex_visited[node->neighbours[i]] == 0) {
			mark_conex_component(node->neighbours[i]);
		}
	}
}

int processingIsDone(os_threadpool_t *tp) {
	return n_visited == graph->nCount;
}

void add_to_graph_sum(void *data) {

	os_node_t *node = (os_node_t *) data;

	pthread_mutex_lock(&sum_lock);
	sum += node->nodeInfo;
	pthread_mutex_unlock(&sum_lock);

	graph->visited[node->nodeID] = 1;

	int ok;

	for (int i = 0; i < node->cNeighbours; i++) {
		ok = 0;

		pthread_mutex_lock(&n_visited_lock);

		if (graph->visited[node->neighbours[i]] == 0) {
			ok = 1;
			graph->visited[node->neighbours[i]] = 1;
		}

		pthread_mutex_unlock(&n_visited_lock);

		if (ok) {
			os_task_t *task = task_create((void *) graph->nodes[node->neighbours[i]], add_to_graph_sum);
			add_task_in_queue(tp, task);
		}
	}

	pthread_mutex_lock(&inc_mutex);
	n_visited++;
	pthread_mutex_unlock(&inc_mutex);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: ./main input_file\n");
        exit(1);
    }

    FILE *input_file = fopen(argv[1], "r");

    if (input_file == NULL) {
        printf("[Error] Can't open file\n");
        return -1;
    }

    graph = create_graph_from_file(input_file);
    if (graph == NULL) {
        printf("[Error] Can't read the graph from file\n");
        return -1;
    }

	tp = threadpool_create(MAX_TASK, MAX_THREAD);

	pthread_mutex_init(&sum_lock, NULL);
	pthread_mutex_init(&n_visited_lock, NULL);
	pthread_mutex_init(&inc_mutex, NULL);

	connex_visited = calloc(sizeof(unsigned int), graph->nCount);
	if (connex_visited == NULL) {
		printf("[ERROR] [main] Not enought memory\n");
		return -1;
	}

	// Initialize the first task of every conex component
	for (int i = 0; i < graph->nCount; i++) {
		if (connex_visited[graph->nodes[i]->nodeID] == 0) {
			os_task_t *task = task_create((void *) graph->nodes[i], add_to_graph_sum);
			add_task_in_queue(tp, task);

			graph->visited[graph->nodes[i]->nodeID] = 1;

			mark_conex_component(graph->nodes[i]->nodeID);
		}
	}

	int r;
	for (int i = 0; i < MAX_THREAD; i++) {
		r = pthread_create(&tp->threads[i], NULL, thread_loop_function, (void *) tp);
        if (r) {
            printf("[ERROR] Cannot create thread %d\n", i);
            exit(-1);
        }
	}

	// Check if the work is done
	while (1) {
		threadpool_stop(tp, processingIsDone);
		if (tp->should_stop) {
			break;
		}
	}

    printf("%d", sum);

	free(connex_visited);

	free(graph->visited);
	for(int i = 0; i < graph->nCount; i++) {
		free(graph->nodes[i]->neighbours);
		free(graph->nodes[i]);
	}
	free(graph->nodes);
	free(graph);

	free(tp->tasks);
	free(tp->threads);
	free(tp);

	pthread_mutex_destroy(&sum_lock);
	pthread_mutex_destroy(&n_visited_lock);
	pthread_mutex_destroy(&inc_mutex);

    return 0;
}
