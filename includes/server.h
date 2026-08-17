#ifndef SERVER_H
#define SERVER_H

#include "logs.h"

/**
 * @struct server_conf
 * @brief Structure that stores the server configuration
 */
typedef struct _server_conf {
    char *server_root;          /**< Path to the server root directory */
    int listen_port;            /**< Port for client connections */
    char *server_signature;     /**< Server name */
    char *log_file;             /**< Base name for log files */
    int timeout_ms;             /**< Time in milliseconds that a thread can wait
                                to receive data from a connection before closing it */
    int queue_size;             /**< Number of accepted connections that can enter the queue
                                while waiting for a thread to handle them */
    int pool_size;              /**< Number of threads in the pool */
    int backlog;                /**< Number of connections that will remain waiting when the queue
                                is full, before they start being rejected */
}server_conf;

#endif
