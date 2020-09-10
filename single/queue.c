/* queue.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h" 

//#define _QDEBUG

#ifdef _QDEBUG
#define _qprintf(fmt, args...) printf(fmt, ##args)
#else
#define _qprintf(fmt, args...) 
#endif

int squeue_init(squeue_t *pqueue, int size)
{
	pqueue->qdata = (squeue_data_t *)calloc(size, sizeof(squeue_data_t));
	if (pqueue->qdata != NULL)
	{
		pqueue->size = size;
	}
	else 
	{
		pqueue->qdata = NULL;
		_qprintf("queue init fail!\n");
		return -1;
	}
	pqueue->head = 0;
	pqueue->tail = 0;
	pqueue->count = 0;
	pthread_mutex_init(&pqueue->mutex, NULL);

	return 0;
}

void squeue_data_destroy(void *data)
{
	squeue_data_t *psdata = (squeue_data_t *)data;	
	if(psdata->pdata != NULL)
	{
		_qprintf("queue destroy one data!\n");
		free(psdata->pdata);
		psdata->pdata = NULL;
		psdata->length = 0;
	}
}

int squeue_enqueue_ext(squeue_t *pqueue, void *pdata, unsigned int length)
{
	squeue_data_t data;

	data.length = length;
	data.pdata = (unsigned char *)malloc(length);
	if (data.pdata == NULL)
	{
		_qprintf("Warn: malloc error");
		return -1;
	}
	memcpy(data.pdata, pdata, sizeof(squeue_data_t));

	pthread_mutex_lock(&pqueue->mutex);
	if(pqueue->count < pqueue->size)
	{
		memcpy(&pqueue->qdata[pqueue->tail], &data, sizeof(squeue_data_t));
		pqueue->tail = (pqueue->tail+1) % pqueue->size;
		pqueue->count++;
		pthread_mutex_unlock(&pqueue->mutex);
		return 0;
	}
	else 
	{
		_qprintf("Warn: queue is full\n");
		pthread_mutex_unlock(&pqueue->mutex);
		return -1;
	}
}

int squeue_enqueue(squeue_t *pqueue, squeue_data_t data)
{
	pthread_mutex_lock(&pqueue->mutex);
	if(pqueue->count < pqueue->size)
	{
		memcpy(&pqueue->qdata[pqueue->tail], &data, sizeof(squeue_data_t));
		pqueue->tail = (pqueue->tail+1) % pqueue->size;
		pqueue->count++;
		pthread_mutex_unlock(&pqueue->mutex);
		return 0;
	}
	else 
	{
		_qprintf("Warn: queue is full\n");
		pthread_mutex_unlock(&pqueue->mutex);
		return -1;
	}
}

static int _squeue_dequeue(squeue_t *pqueue, squeue_data_t *pdata)
{
	if(pqueue->count > 0)
	{
		memcpy(pdata, &pqueue->qdata[pqueue->head], sizeof(squeue_data_t));
		pqueue->qdata[pqueue->head].pdata = NULL;
		pqueue->qdata[pqueue->head].length = 0;
		pqueue->head = (pqueue->head + 1) % pqueue->size;
		pqueue->count--;
		return 0;
	}
	else 
	{
		_qprintf("Warn: queue is empty\n");
		return -1;
	}
}
int squeue_dequeue(squeue_t *pqueue, squeue_data_t *pdata)
{
	int ret = -1;

	pthread_mutex_lock(&pqueue->mutex);
	ret = _squeue_dequeue(pqueue, pdata);
	pthread_mutex_unlock(&pqueue->mutex);
	return ret;
}

void squeue_destroy(squeue_t *pqueue, void (*destroy)(void *data))
{
	squeue_data_t data;

	pthread_mutex_lock(&pqueue->mutex);
	while(pqueue->count > 0)
	{
		_squeue_dequeue(pqueue, &data);
		if (destroy)
		{
			destroy(&data);
		}
	}	

	if(pqueue->qdata != NULL)
	{
		free(pqueue->qdata);
		pqueue->qdata = NULL;
	}
	pthread_mutex_unlock(&pqueue->mutex);
}

bool squeue_is_full(squeue_t *pqueue)
{
	bool ret = false;

	pthread_mutex_lock(&pqueue->mutex);
	ret = (pqueue->count == pqueue->size);	
	pthread_mutex_unlock(&pqueue->mutex);

	return ret;
}

bool squeue_is_empty(squeue_t *pqueue)
{
	bool ret = false;

	pthread_mutex_lock(&pqueue->mutex);
	ret = (pqueue->count == 0);	
	pthread_mutex_unlock(&pqueue->mutex);

	return ret;
}

void squeue_info(squeue_t *pqueue)
{
	if(pqueue->qdata == NULL)
	{
		_qprintf("queue is not initialized!\n");
		return;
	}
	printf("queue count: %d, size: %d, head: %d, tail: %d\n", 
			pqueue->count, pqueue->size, pqueue->head, pqueue->tail);
	for(int i= 0; i < pqueue->size; i++)
	{
		if((pqueue->qdata+i) != NULL)
			printf("The %dth item info is [%d]:[%p]\n", 
					i, (pqueue->qdata+i)->length, (pqueue->qdata+i)->pdata);
	}
	printf("\n");
}

#if 0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"

int main(int argc, char *argv[])
{
	squeue_t sq;
	squeue_data_t data, data1;

	squeue_init(&sq, 10);
	squeue_enqueue_ext(&sq, "LOVEM", 6);
	squeue_enqueue_ext(&sq, "LOVEN", 6);
	
	data.length = 10;  
	data.pdata = (char *)malloc(5);
	if(data.pdata == NULL)
	{
		printf("malloc error");
		return -1;
	}
	memcpy(data.pdata, "LOVEA", 5);
	squeue_enqueue(&sq, data);

	memcpy(data.pdata, "LOVEB", 5);
	squeue_enqueue(&sq, data);

	memcpy(data.pdata, "LOVEC", 5);
	squeue_enqueue(&sq, data);

	memcpy(data.pdata, "LOVED", 5);
	squeue_enqueue(&sq, data);

	squeue_enqueue_ext(&sq, "LOVEE", 6);
	squeue_info(&sq);

	while(squeue_dequeue(&sq, &data1)== 0)
	{
		printf("[%d]:%s\n", data1.length, (char *)data1.pdata);
	}

//	squeue_destroy(&sq, squeue_data_destroy);
	return 0;
}
#endif

