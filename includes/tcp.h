/*
 * @file tcp.h
 * @brief Defines the function declarations for handling TCP sockets and connections
 */
#ifndef TCP_H
#define TCP_H

/*
 * @brief Initializes and binds the TCP socket to the given port
 * Encapsulates the socket, bind, and listen functions
 *
 * @param sockfd Address of the socket descriptor
 * @param port Port to bind the socket to
 * @param max_conns Maximum number of connections for the listen function
 *
 * @return 0 if successful, -1 if an error occurs (uses errno)
 */
int tcp_start_connection(int *sockfd, int port, int max_conns);

/*
 * @brief Accepts an incoming TCP connection. This function is blocking.
 *
 * @param connfd Address of the connection descriptor
 * @param sockfd Socket descriptor
 *
 * @return 0 if successful, -1 if an error occurs (uses errno)
 */
int tcp_accept_connection(int *connfd, int sockfd);

/*
 * @brief Sends data through a socket
 *
 * @param fd Socket descriptor
 * @param buf Data buffer
 * @param len Length of the data buffer
 *
 * @return Length of the data sent. Negative if an error occurs
 */
int tcp_send(int fd, char *buf, int len);

/*
 * @brief Receives data from a socket
 *
 * @param fd Socket descriptor
 * @param buf Data buffer
 * @param len Length of the data buffer
 *
 * @return Length of the data received, 0 if the connection was closed. Negative if an error occurs
 */
int tcp_recv(int fd, char *buf, int len);

/*
 * @brief Shuts down the socket descriptor, releases its resources, and sends FIN to the other side.
 *
 * Uses shutdown() from sys/socket.h so that threads do not remain blocked
 * in accept() when SIGINT is received
 *
 * @param fd Socket descriptor
 *
 * @return 0 if successful, -1 if an error occurs
 */
int tcp_shutdown(int fd);

#endif
