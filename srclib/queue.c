#include <pthread.h>
#include <stdlib.h>
#include "queue.h"

/* Client queue structure */
struct _queue {
    int *fds;                   /**< Descriptors array */
    int max_size;               /**< maximum queue size */
    int size;                   /**< number of clients in queue */
    int front;                  /**< start of queue */
    int rear;                   /**< end of queue */

    int *active_fds;            /**< array with the descriptor each thread is handling */

    pthread_mutex_t mutex;      /**< queue mutex */
    pthread_cond_t not_emtpy;   /**< not empty condition */
    pthread_cond_t not_full;    /**< not full condition */
};

queue *queue_create(int pool_size, int queue_max){
    queue *cq;
    
    cq = malloc(sizeof(queue));
    if(!cq)
        return NULL;

    cq->max_size = queue_max;
    cq->size = 0;
    cq->front = 0;
    cq->rear = 0;
    cq->fds = calloc(sizeof(int), cq->max_size);
    if(!cq->fds){
        free(cq);
        return NULL;
    }
    cq->active_fds = calloc(sizeof(int), pool_size);
    if(!cq->active_fds){
        free(cq->fds);
        free(cq);
        return NULL;
    }

    pthread_cond_init(&cq->not_emtpy, NULL);
    pthread_cond_init(&cq->not_full, NULL);
    pthread_mutex_init(&cq->mutex, NULL);

    return cq;
}

void queue_push(queue *cq, int fd, volatile int *sigint){

    pthread_mutex_lock(&cq->mutex);
    while(cq->size == cq->max_size){
        /* waits till queue not full */
        if(*sigint) break;
        pthread_cond_wait(&cq->not_full, &cq->mutex);
    }

    /* Check for sigint */
    if(*sigint){
        pthread_mutex_unlock(&cq->mutex);
        return;
    }

    cq->fds[cq->rear] = fd;
    cq->rear = (cq->rear + 1) % cq->max_size;
    cq->size ++;

    pthread_cond_signal(&cq->not_emtpy);
    pthread_mutex_unlock(&cq->mutex);
}

int queue_pop(queue *cq, volatile int *sigint){
    int fd;

    pthread_mutex_lock(&cq->mutex);

    while(cq->size == 0){
        /* waits for queue not empty */
        if(*sigint) break;
        pthread_cond_wait(&cq->not_emtpy, &cq->mutex);
    }

    /* Check for sigint */
    if(*sigint){
        pthread_mutex_unlock(&cq->mutex);
        return -1;
    }

    fd = cq->fds[cq->front];
    cq->front = (cq->front + 1) % cq->max_size;
    cq->size--;


    pthread_cond_signal(&cq->not_full);
    pthread_mutex_unlock(&cq->mutex);

    return fd;
}

void queue_add_active_fd(queue *cq, int fd, int id){
    pthread_mutex_lock(&cq->mutex);
    cq->active_fds[id] = fd;
    pthread_mutex_unlock(&cq->mutex);
}

void queue_del_active_fd(queue *cq, int id){
    pthread_mutex_lock(&cq->mutex);
    cq->active_fds[id] = 0;
    pthread_mutex_unlock(&cq->mutex);
}

int queue_get_active_fd(queue *cq, int id){
    int fd;

    pthread_mutex_lock(&cq->mutex);
    fd = cq->active_fds[id];
    pthread_mutex_unlock(&cq->mutex);
    return fd;
}

void queue_shutdown(queue *cq){
    pthread_mutex_lock(&cq->mutex);
    pthread_cond_broadcast(&cq->not_emtpy);
    pthread_mutex_unlock(&cq->mutex);
}

void queue_destroy(queue *cq){
    free(cq->fds);
    free(cq->active_fds);
    pthread_mutex_destroy(&cq->mutex);
    pthread_cond_destroy(&cq->not_full);
    pthread_cond_destroy(&cq->not_emtpy);
    free(cq);
}
