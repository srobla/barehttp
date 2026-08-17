/*
 * @file queue.h
 * @brief Defines the structure and function declarations for
 * handling the client queue
 *
 * The queue is circular and stores each new connection
 * that is accepted.
 */
#ifndef QUEUE_H

/*
 * @struct queue
 * @brief Structure that stores the client queue
 * It is a circular queue of client socket descriptors
 */
typedef struct _queue queue;


/**
 * @brief Creates a new queue with the specified parameters
 *
 * @param pool_size Number of threads in the pool
 * @param queue_max Maximum number of clients that can be in the queue
 *
 * @return Pointer to the new queue
 */
queue *queue_create(int pool_size, int queue_max);

/**
 * @brief Adds a connection socket to the queue
 *
 * Blocking function; if the queue is full, it waits until a slot becomes available
 *
 * @param cq The queue
 * @param fd Connection socket descriptor
 * @param sigint Pointer to the global variable indicating whether SIGINT has been received
 */
void queue_push(queue *cq, int fd, volatile int *sigint);

/**
 * @brief Removes a socket from the queue
 *
 * @param cq The queue
 * @param sigint Pointer to the global variable indicating whether SIGINT has been received. Used to stop waiting
 *
 * @return The extracted descriptor
 */
int queue_pop(queue *cq, volatile int *sigint);

/**
 * @brief Adds a new descriptor to the list of active descriptors
 *
 * @param cq The queue
 * @param fd Descriptor to add
 * @param id Identifier of the thread adding it
 */
void queue_add_active_fd(queue *cq, int fd, int id);

/**
 * @brief Removes a descriptor from the list of active descriptors
 *
 * @param cq The queue
 * @param id Identifier of the thread removing it
 */
void queue_del_active_fd(queue *cq, int id);

/**
 * @brief Gets the active descriptor corresponding to the ID
 *
 * @param cq The queue
 * @param id Identifier
 *
 * @return The corresponding descriptor
 */
int queue_get_active_fd(queue *cq, int id);

/**
 * @brief Signals all threads waiting for an fd from the queue
 * so that they stop waiting
 *
 * @param cq The queue
 */
void queue_shutdown(queue *cq);

/**
 * @brief Destroys the queue and frees the memory
 *
 * @param cq The queue
 */
void queue_destroy(queue *cq);

#endif
