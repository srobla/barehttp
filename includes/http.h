/*
 * @file http.h
 * @brief Define las cabeceras de las funciones que gestionan las peticiones HTTP
 */
#ifndef HTTP_H
#define HTTP_H

#include <stdio.h>
#include "picohttpparser.h"
#include "server.h"
#include "logs.h"

#define METHOD_LEN 10
#define URL_LEN 255
#define MAX_HEADERS 50
#define MAX_BUFFR 8192
#define MAX_HEADER_SIZE 256
#define MAX_HEADER_CONTENT 64

/**
 * @struct http_request
 * @brief Structure that stores an HTTP request
 *
 * Made public to minimize mallocs / frees per request
 */
typedef struct _http_request{
    const char *method;                        /**< Method (GET, POST or OPTIONS)*/
    int method_size;                           /**< Length of the method string */
    const char *url;                           /**< Request URL*/
    int url_size;                              /**< Length of the URL string*/
    int version;                               /**< HTTP version (we implement 1.0)*/
    struct phr_header headers[MAX_HEADERS];    /**< Array of headers, using the phr_header structure from picohttpparser */
    int num_headers;                           /**< Number of headers */
    const char *body;                          /**< Request body*/
    int body_size;                             /**< Length of the body string*/
} http_request;



/**
 * @struct http_response
 * @brief Structure that stores an HTTP response
 */
typedef struct _http_response http_response;

/**
 * @struct response_header
 * @brief Structure that stores an HTTP header
 */
typedef struct _response_header response_header;

/*
 * @enum http_status_t
 * @brief HTTP status codes
 */
typedef enum http_status_t {
    HTTP_OK = 200,                       /**< Successful request */
    HTTP_NO_CONTENT = 204,               /**< Successful request with no content */
    HTTP_BAD_REQUEST = 400,              /**< Request with an invalid format */
    HTTP_FORBIDDEN = 403,                /**< Access denied due to insufficient permissions */
    HTTP_NOT_FOUND = 404,                /**< Resource not found */
    HTTP_NOT_ALLOWED = 405,              /**< Method not allowed */
    HTTP_INTERNAL_ERROR = 500,           /**< Internal server error while processing the request */
    HTTP_VERSION_NOT_SUPPORTED = 505,    /**< Unsupported HTTP version */
} http_status_t;

/*
 * @enum http_method_t
 * @brief Types of HTTP methods
 */
typedef enum http_method_t {
    HTTP_METHOD_UNKNOWN,                    /**< Unknown method, not supported by the server */
    HTTP_METHOD_GET,                        /**< GET method (a resource is requested) */
    HTTP_METHOD_POST,                       /**< POST method (a resource is sent) */
    HTTP_METHOD_OPTIONS                     /**< OPTIONS method (information about the server is requested) */
} http_method_t;


/*
 * @brief Parses HTTP requests and stores the contents in a structure
 *
 * @param buf Pointer to the buffer containing the request
 * @param request Pointer to the structure where the request will be stored
 * @param buff_len Number of bytes contained in the buffer
 * @param last_len Last position read from the buffer in a previous call
 *
 * @return 0 if successful, -1 if an error occurs, -2 if the header is incomplete
 */
int http_parse_request(char *buf, http_request *request, int buff_len, int last_len);

/*
 * @brief Initializes the request structure
 *
 * @return Pointer to the structure
 */
http_request *http_init_request();

/*
 * @brief Gets the length of the request body from the Content-Length header
 *
 * @param request Pointer to the structure containing the request
*/
int http_get_content_length(http_request *request);

/*
 * @brief Adds the request body to the structure
 *
 * @param request Pointer to the structure containing the request
 * @param body String containing the request body
 * @param body_size Length of the request body
 */
void http_add_body(http_request *request, char *body, int body_size);

/*
 * @brief Processes the request, determining the requested method and the action to perform
 *
 * @param request Pointer to the structure containing the request
 * @param clientfd Client socket descriptor
 * @param sc Pointer to the structure containing the server configuration
 *
 * @return 0 if successful, -1 if an error occurs
 */
int http_process_request(http_request *request, int clientfd, server_conf *sc, log_file *lf);

/*
 * @brief Sends a response with the specified error code to the client
 *
 * @param clientfd Client socket descriptor
 * @param code Error code
 *
 * @return 0 if successful, -1 if an error occurs
 */
int http_send_error(int clientfd, http_status_t code);

/*
 * @brief Prints the request to the screen
 *
 * @param r Pointer to the structure containing the request
 */
void http_debug_request(http_request r);

/*
 *
 * @brief Frees the HTTP request structure
 *
 * @param r Pointer to the structure
 */
void http_free_request(http_request *r);


#endif
