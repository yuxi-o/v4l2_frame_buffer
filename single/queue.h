#ifndef __QUEUE_H__
#define __QuEUE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <pthread.h>

typedef struct squeue_data 
{
	void *pdata;
	unsigned int length;
} squeue_data_t;

// head 出列，tail 入列
typedef struct squeue
{
	int head;
	int tail;
	int count;
	int size;
	pthread_mutex_t mutex;
	squeue_data_t *qdata;
} squeue_t;

int squeue_init(squeue_t *pqueue, int size);
int squeue_enqueue(squeue_t *pqueue, squeue_data_t data);
int squeue_dequeue(squeue_t *pqueue, squeue_data_t *pdata);
void squeue_destroy(squeue_t *pqueue, void (*destroy)(void *data));
bool squeue_is_full(squeue_t *pqueue);
bool squeue_is_empty(squeue_t *pqueue);
int squeue_enqueue_ext(squeue_t *pqueue, void* pdata, unsigned int length);
void squeue_info(squeue_t *pqueue);
void squeue_data_destroy(void *data);

#ifdef __cplusplus
}
#endif

#endif
