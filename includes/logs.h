#ifndef LOGS_H
#define LOGS_H

#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

/*
 * @enum log_type
 * @brief Enumeration of the possible log types
 */
typedef enum {
    LOG_ERROR,          /**< Errors */
    LOG_WARNING,        /**< Warnings (failures but not fatal) */
    LOG_INFO            /**< Information about the execution */
} log_type;

/*
 * @struct log_file
 * @brief Structure containing information about a log file
 */
typedef struct {
    FILE *file;                 /**< Pointer to the file */
    pthread_mutex_t mutex;      /**< Mutex for writing to the file */
} log_file;

/*
 * @brief Initializes a log file
 *
 * @param log_name Log file name
 *
 * @return Pointer to the log file
 */
log_file *logs_init_file(char *log_name);

/*
 * @brief Prints a message to the log file. Accepts a variable number of arguments
 *
 * @param type Log type
 * @param lf Pointer to the log file structure
 * @param message Message format string
 * @param ... Additional arguments included in the message according to the format
 *
 * @return 0 if successful, -1 if an error occurs
 */
int logs_print(log_type type, log_file *lf, char *message, ...);

/*
 * @brief Gets the client's IP and port from the socket descriptor and stores them in the
 * ip_port_str string in the format ip:port
 *
 * @param clientfd Client socket descriptor
 * @param ip_port_str String where the formatted IP and port will be stored
 * @param lf Pointer to the log file structure
 */
void get_client_ip_port(int clientfd, char *ip_port_str, log_file *lf);

/*
 * @brief Destroys the log file structure (closes the file, destroys the mutex, and frees the memory)
 *
 * @param lf Pointer to the log file structure
 */
void logs_destroy(log_file *lf);

#endif
