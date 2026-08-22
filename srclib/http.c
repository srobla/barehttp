/*
 * @file http.c
 * @brief Manages HTTP petitons and responses
 *
 * @author Samuel Robla y Ángela Horcajo
 */
#include <asm-generic/errno.h>
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "http.h"
#include "logs.h"
#include "picohttpparser.h"
#include "server.h"
#include "tcp.h"

#define _POSIX_C_SOURCE 200809L

#define CHUNK_SIZE 8192

#define MAX_HEADER_VALUE_LEN 16
#define MAX_PATH 132

/* HTTP response header */
struct _response_header {
    const char *name;  /**< header name */
    const char *value; /**< header value */
};

/* HTTP response */
struct _http_response {
    http_status_t status;                 /**< status code */
    response_header headers[MAX_HEADERS]; /**< headers array */
    int n_headers;                        /**< number of headers */
};

/* ========== private functions declaration =========== */

/* Gets request type */
enum http_method_t request_method_type(const char *method, size_t method_len);

/* Process GET request */
int process_get(http_request *request, int clientfd, server_conf *sc, log_file *lf);

/* Given a code, returns a descriptive text */
char *status_text(http_status_t code);

/* Gives full path given relative path and root */
int build_path(http_request *request, char path[MAX_PATH]);

/* Gets size and last modification date given a path */
int get_file_info(int root_fd, const char *requested_path, int *file_size, time_t *last_modified);

/* Gets mime type from a given path file. Assumes filenames cannot contain "."
 */
char *get_mime_type(const char *path);

/* Sends a file via given socket */
int send_file(int root_fd, const char *requested_path, int clientfd);

/* Adds a header to response headers structure */
int add_header(http_response *response, const char *name, const char *value);

/* Serializes response in a buffer */
int serialize_response(const http_response res, char *buffer, size_t size);

/* Process OPTIONS request */
int process_options(int clientfd, log_file *lf);

/* Process POST request */
int process_post(http_request *request, int clientfd, server_conf *sc, log_file *lf);

/* Executes and sends script with given args */
int process_script(const char *script_args, int args_len, char *path, int clientfd, server_conf *sc, log_file *lf);

/* Checks if url acces non allowed paths (path traversal) */
int check_path_traversal(const char *url, int url_size);

/* Opens file safely, preventing symlinks */
int open_safe(int root_fd, const char *requested_path);

/* ========== Public function implementation =========== */

int http_parse_request(char *buff, http_request *request, int buff_len, int last_len) {
    request->num_headers = MAX_HEADERS;
    return phr_parse_request(
        buff, buff_len, &request->method, (size_t *)&request->method_size,
        &request->url, (size_t *)&request->url_size, &request->version,
        request->headers, (size_t *)&request->num_headers, last_len);
}

void http_add_body(http_request *request, char *body, int body_size) {
    request->body = body;
    request->body_size = body_size;
}

http_request *http_init_request() {
    int i;
    http_request *r;

    r = malloc(sizeof(http_request));
    if (!r)
        return NULL;

    r->method = NULL;
    r->method_size = 0;
    r->url = NULL;
    r->url_size = 0;
    r->version = 0;
    for (i = 0; i < MAX_HEADERS; i++) {
        r->headers[i].name = NULL;
        r->headers[i].name_len = 0;
        r->headers[i].value = NULL;
        r->headers[i].value_len = 0;
    }
    r->num_headers = 0;
    r->body = NULL;
    r->body_size = 0;

    return r;
}

void http_free_request(http_request *r) { free(r); }

int http_process_request(http_request *request, int clientfd, server_conf *sc,
                         log_file *lf) {
    int ret = 0;
    char ip_port_str[64];
    get_client_ip_port(clientfd, ip_port_str, lf);

    switch (request_method_type(request->method, request->method_size)) {
    case HTTP_METHOD_GET:
        logs_print(LOG_INFO, lf, "Received GET petition %.*s - client: %s",
                   request->url_size, request->url, ip_port_str);
        ret = process_get(request, clientfd, sc, lf);
        break;
    case HTTP_METHOD_POST:
        logs_print(LOG_INFO, lf, "Received POST petition %.*s - client: %s",
                   request->url_size, request->url, ip_port_str);
        ret = process_post(request, clientfd, sc, lf);
        break;
    case HTTP_METHOD_OPTIONS:
        logs_print(LOG_INFO, lf, "Received OPTIONS petition %.*s - client: %s",
                   request->url_size, request->url, ip_port_str);
        ret = process_options(clientfd, lf);
        break;
    default:
        logs_print(LOG_ERROR, lf, "Not supported method %d - client: %s",
                   request->method, ip_port_str);
        ret = http_send_error(clientfd, HTTP_NOT_ALLOWED);
        break;
    }

    return ret;
}

int http_send_error(int clientfd, http_status_t code) {
    int ret, body_len, header_len;
    char body[512], headers[256];
    char *text = status_text(code);

    body_len = snprintf(body, sizeof(body),
                        "<html>"
                        "<head><title>%d %s</title></head>"
                        "<body>"
                        "<h1>%d %s</h1>"
                        "</body>"
                        "</html>",
                        code, text, code, text);

    header_len = snprintf(headers, sizeof(headers),
                          "HTTP/1.0 %d %s\r\n"
                          "Content-Type: text/html\r\n"
                          "Content-Length: %d\r\n"
                          "Connection: close\r\n"
                          "\r\n",
                          code, text, body_len);

    ret = tcp_send(clientfd, headers, header_len);
    if (ret < 0)
        return -1;
    tcp_send(clientfd, body, body_len);

    return 0;
}

int http_get_content_length(http_request *request) {
    size_t len, content_length;
    char temp_val[MAX_HEADER_VALUE_LEN];
    int i;

    for (i = 0; i < request->num_headers; i++) {
        if (request->headers[i].name_len == 14 &&
            memcmp(request->headers[i].name, "Content-Length", 14) == 0) {
            len = request->headers[i].value_len;

            if (len >= MAX_HEADER_VALUE_LEN)
                len = MAX_HEADER_VALUE_LEN - 1;

            memcpy(temp_val, request->headers[i].value, len);
            temp_val[len] = '\0';
            content_length = atoi(temp_val);

            return content_length;
        }
    }
    return 0;
}

void http_debug_request(http_request r) {
    int i;
    printf("-------------------\n");
    printf("Method: %.*s\n", r.method_size, r.method);
    printf("Path: %.*s\n", r.url_size, r.url);
    printf("Version: 1.%d\n", r.version);
    printf("Headers (%d): \n", r.num_headers);
    for (i = 0; i < r.num_headers; i++) {
        printf("\t%.*s: %.*s\n", (int)r.headers[i].name_len, r.headers[i].name,
               (int)r.headers[i].value_len, r.headers[i].value);
    }
    printf("Body: %.*s\n", r.body_size, r.body);
    printf("--------------------\n");
}

/* ========== private function implementation =========== */

int process_options(int clientfd, log_file *lf) {
    http_response response;
    char buffer[MAX_BUFFR], ip_port_str[64];
    int ret;

    response.status = HTTP_NO_CONTENT;
    response.n_headers = 0;

    add_header(&response, "Allow", "GET, POST, OPTIONS");
    /* Options has no body */
    add_header(&response, "Content-Length", "0");

    if (serialize_response(response, buffer, MAX_BUFFR) == -1) {
        logs_print(LOG_ERROR, lf, "Response too long to fit in %d", MAX_BUFFR);
        http_send_error(clientfd, HTTP_INTERNAL_ERROR);
        return 0;
    }

    ret = tcp_send(clientfd, buffer, strlen(buffer));
    get_client_ip_port(clientfd, ip_port_str, lf);

    if (ret < 0) {
        return -1;
    }

    logs_print(LOG_INFO, lf, "Response sent - client: %s", ip_port_str);

    return 0;
}

int process_post(http_request *request, int clientfd, server_conf *sc,
                 log_file *lf) {
    char path[MAX_PATH], *mime_type, ip_port_str[64];
    int i, ret;

    /* Get client ip and port */
    get_client_ip_port(clientfd, ip_port_str, lf);

    /* path traversal */
    if (check_path_traversal(request->url, request->url_size)) {
        logs_print(LOG_WARNING, lf,
                   "Trying to access not allowed direction - client: %s",
                   ip_port_str);
        http_send_error(clientfd, HTTP_FORBIDDEN);
        return 0;
    }

    /* searchs for arguments */
    for (i = 0; i < request->url_size; i++) {
        if (request->url[i] == '?') {
            request->url_size = i;
        }
    }

    /* build path */
    ret = build_path(request, path);
    if (ret == -2) {
        http_send_error(clientfd, HTTP_BAD_REQUEST);
        return 0;
    } else if (ret < 0) {
        return -1;
    }

    mime_type = get_mime_type(path);
    if (strcmp(mime_type, "text/x-python") == 0 ||
        strcmp(mime_type, "text/x-php") == 0) {
        ret = process_script(request->body, request->body_size, path, clientfd, sc,
                             lf);
        if (ret == -1) {
            return -1;
        }
        if (ret == -2) {
            return 0;
        }
        logs_print(LOG_INFO, lf, "Executing POST script - client: %s", ip_port_str);
    }

    logs_print(LOG_INFO, lf, "POST response sent - client: %s", ip_port_str);

    return 0;
}

int send_file(int root_fd, const char *requested_path, int clientfd) {
    int bytes_sent;
    off_t offset;
    int fd;
    struct stat st;

    fd = open_safe(root_fd, requested_path);
    if (fd < 0) {
        return -1;
    }

    if (fstat(fd, &st) < 0) {
        close(fd);
        return -1;
    }

    offset = 0;

    while (offset < st.st_size) {
        printf("Sending file\n");
        bytes_sent = sendfile(clientfd, fd, &offset, st.st_size - offset);
        if (bytes_sent <= 0){
            if(errno==EWOULDBLOCK)printf("EWOULDBLOCK. ");
            if(errno==EPIPE)printf("EPIPE. ");
            perror("Error sending big file");
            break;
        }
    }
    if (offset < st.st_size)
        return -1;

    close(fd);
    return 0;
}

int serialize_response(const http_response res, char *buffer, size_t size) {
    int len = 0, i;

    /* Status line */
    len += snprintf(buffer + len, size - len, "HTTP/1.0 %d %s\r\n", res.status,
                    status_text(res.status));

    if (len < 0 || (size_t)len >= size)
        return -1;

    for (i = 0; i < res.n_headers; i++) {
        len += snprintf(buffer + len, size - len, "%s: %s\r\n", res.headers[i].name,
                        res.headers[i].value);

        if (len < 0 || len >= size)
            return -1;
    }

    /* headers end */
    len += snprintf(buffer + len, size - len, "\r\n");

    if (len < 0 || (size_t)len >= size)
        return -1;

    return len;
}

int add_header(http_response *response, const char *name, const char *value) {

    if (response->n_headers == MAX_HEADERS)
        return -1;

    response->headers[response->n_headers].name = name;
    response->headers[response->n_headers].value = value;
    response->n_headers++;

    return 0;
}

int process_script(const char *script_args, int args_len, char *path,
                   int clientfd, server_conf *sc, log_file *lf) {
    int inpipe[2];
    int outpipe[2];
    int script_fd;
    pid_t pid;
    char buffer[MAX_BUFFR], script_out_buffer[MAX_BUFFR],
        date_str[MAX_HEADER_CONTENT], content_size_str[MAX_HEADER_CONTENT],
        ip_port_str[64], full_path[MAX_PATH];
    http_response response;
    int ret, n_bytes, total_bytes;
    time_t now = time(NULL);
    struct tm *time;

    get_client_ip_port(clientfd, ip_port_str, lf);

    ret = get_file_info(sc->root_fd, path, NULL, NULL);
    switch (ret) {
    case -2:
        logs_print(LOG_WARNING, lf, "Requested script not exists - client: %s",
                   ip_port_str);
        http_send_error(clientfd, HTTP_NOT_FOUND);
        return -2;
    case -3:
        logs_print(LOG_WARNING, lf, "Requested script not allowed - client: %s",
                   ip_port_str);
        http_send_error(clientfd, HTTP_NOT_ALLOWED);
        return -2;
    case -1:
        return -1;
    default:
        break;
    }

    /* Securely open and get fd */
    script_fd = open_safe(sc->root_fd, path);

    snprintf(full_path, sizeof(full_path), "/proc/self/fd/%d", script_fd);

    if (pipe(inpipe) < 0) {
        return -1;
    }
    if (pipe(outpipe) < 0) {
        close(inpipe[0]);
        close(inpipe[1]);
        return -1;
    }

    pid = fork();
    if (pid < 0) {
        close(inpipe[0]);
        close(inpipe[1]);
        close(outpipe[0]);
        close(outpipe[1]);
        return -1;
    }

    if (pid == 0) {

        close(inpipe[1]);
        dup2(inpipe[0], STDIN_FILENO);
        close(inpipe[0]);

        close(outpipe[0]);
        dup2(outpipe[1], STDOUT_FILENO);
        dup2(outpipe[1], STDERR_FILENO);
        close(outpipe[1]);

        const char *ext = strrchr(path, '.');

        if (ext && strcmp(ext, ".py") == 0) {
            execl("/usr/bin/python3", "python3", full_path, (char *)NULL);
        } else if (ext && strcmp(ext, ".php") == 0) {
            execl("/usr/bin/php", "php", full_path, (char *)NULL);
        } else {
            execl(full_path, full_path, (char *)NULL);
        }
        exit(1);
    }

    close(inpipe[0]);
    close(outpipe[1]);

    /* Sends body */
    total_bytes = 0;
    while (total_bytes < args_len) {
        n_bytes =
            write(inpipe[1], script_args + total_bytes, args_len - total_bytes);
        if (n_bytes <= 0)
            break;
        total_bytes += n_bytes;
    }
    close(inpipe[1]);

    total_bytes = 0;
    while (1) {
        if (total_bytes >= MAX_BUFFR) {
            close(outpipe[0]);
            waitpid(pid, NULL, 0);
            return -1;
        }

        n_bytes = read(outpipe[0], script_out_buffer + total_bytes,
                       MAX_BUFFR - total_bytes);
        if (n_bytes <= 0)
            break;

        total_bytes += n_bytes;
    }

    close(outpipe[0]);
    /* wait for process */
    waitpid(pid, NULL, 0);

    response.status = HTTP_OK;
    response.n_headers = 0;
    time = gmtime(&now);
    ret = strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", time);
    if (ret == 0) {
        http_send_error(clientfd, HTTP_INTERNAL_ERROR);
        return -1;
    }

    snprintf(content_size_str, MAX_HEADER_CONTENT, "%d", total_bytes);
    add_header(&response, "Date", date_str);
    add_header(&response, "Server", sc->server_signature);
    add_header(&response, "Content-Length", content_size_str);
    add_header(&response, "Content-Type", "text/plain");
    add_header(&response, "Connection", "Close");

    if (serialize_response(response, buffer, MAX_BUFFR) == -1) {
        logs_print(LOG_ERROR, lf, "Response too long to fit in %d", MAX_BUFFR);
        http_send_error(clientfd, HTTP_INTERNAL_ERROR);
        return 0;
    }

    ret = tcp_send(clientfd, buffer, strlen(buffer));
    if (ret < 0) {
        return -1;
    }

    ret = tcp_send(clientfd, script_out_buffer, total_bytes);
    if (ret < 0) {
        return -1;
    }

    return 0;
}

int process_get(http_request *request, int clientfd, server_conf *sc,
                log_file *lf) {
    char path[MAX_PATH], buffer[MAX_BUFFR], *mime_type, ip_port_str[64];
    const char *script_args;
    int file_size, ret, args_len = 0, i;
    time_t last_modified;
    char date_str[MAX_HEADER_CONTENT], last_mod_str[MAX_HEADER_CONTENT],
        content_size_str[MAX_HEADER_CONTENT];
    time_t now = time(NULL);
    struct tm *time;
    http_response response;

    get_client_ip_port(clientfd, ip_port_str, lf);

    if (check_path_traversal(request->url, request->url_size)) {
        logs_print(LOG_WARNING, lf,
                   "Trying to access not allowed direction - client: %s",
                   ip_port_str);
        http_send_error(clientfd, HTTP_FORBIDDEN);
        return 0;
    }

    for (i = 0, script_args = NULL; i < request->url_size; i++) {
        if (request->url[i] == '?') {
            script_args = request->url + i + 1;
            args_len = request->url_size - (i + 1);
            request->url_size = i;
        }
    }

    ret = build_path(request, path);
    if (ret == -2) {
        http_send_error(clientfd, HTTP_BAD_REQUEST);
        return 0;
    } else if (ret < 0) {
        return -1;
    }

    mime_type = get_mime_type(path);

    if (strcmp(mime_type, "text/x-python") == 0 ||
        strcmp(mime_type, "text/x-php") == 0) {
        ret = process_script(script_args, args_len, path, clientfd, sc, lf);
        if (ret == -2)
            return 0;
        if (ret == 0) {
            logs_print(LOG_INFO, lf, "Executing GET script - client: %s",
                       ip_port_str);
            logs_print(LOG_INFO, lf, "GET response sent - client: %s", ip_port_str);
        }
        return ret;
    }

    /* read file */
    ret = get_file_info(sc->root_fd, path, &file_size, &last_modified);
    if (ret == -2) {
        /* Not found */
        logs_print(LOG_WARNING, lf, "File %s not found - client: %s", path, ip_port_str);
        http_send_error(clientfd, HTTP_NOT_FOUND);
        return 0;
    } else if (ret == -3) {
        /* Not allowed */
        logs_print(LOG_WARNING, lf, "File %s not allowed - client: %s", path, ip_port_str);
        http_send_error(clientfd, HTTP_FORBIDDEN);
        return 0;
    } else if (ret < 0) {
        perror("Get file info:");
        return -1;
    }

    response.status = HTTP_OK;

    response.n_headers = 0;
    time = gmtime(&now);
    ret = strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", time);
    if (ret == 0) {
        logs_print(LOG_ERROR, lf, "Error formatting actual date - client: %s",
                   ip_port_str);
        http_send_error(clientfd, HTTP_INTERNAL_ERROR);
        return 0;
    }

    time = gmtime(&last_modified);
    ret = strftime(last_mod_str, sizeof(last_mod_str),
                   "%a, %d %b %Y %H:%M:%S GMT", time);
    if (ret == 0) {
        logs_print(LOG_ERROR, lf,
                   "Error formatting last modified date - client: %s", ip_port_str);
        http_send_error(clientfd, HTTP_INTERNAL_ERROR);
        return 0;
    }
    snprintf(content_size_str, MAX_HEADER_CONTENT, "%d", file_size);
    add_header(&response, "Date", date_str);
    add_header(&response, "Server", sc->server_signature);
    add_header(&response, "Last-Modified", last_mod_str);
    add_header(&response, "Content-Length", content_size_str);
    add_header(&response, "Content-Type", mime_type);
    add_header(&response, "Connection", "Close");

    if (serialize_response(response, buffer, MAX_BUFFR) == -1) {
        logs_print(LOG_ERROR, lf, "Response too long to fit in %d", MAX_BUFFR);
        http_send_error(clientfd, HTTP_INTERNAL_ERROR);
        return 0;
    }

    ret = tcp_send(clientfd, buffer, strlen(buffer));
    if (ret < 0) {
        logs_print(LOG_ERROR, lf, "Error sending HTTP response header - client: %s",
                   ip_port_str);
        http_send_error(clientfd, HTTP_INTERNAL_ERROR);
        return 0;
    }

    ret = send_file(sc->root_fd, path, clientfd);
    if (ret < 0) {
        return -1;
    }

    logs_print(LOG_INFO, lf, "Response HTTP, with file %s, sent - client: %s",
               path, ip_port_str);

    return 0;
}

enum http_method_t request_method_type(const char *method, size_t method_len) {
    if (method_len == 3 && memcmp(method, "GET", 3) == 0) {
        return HTTP_METHOD_GET;
    } else if (method_len == 4 && memcmp(method, "POST", 4) == 0) {
        return HTTP_METHOD_POST;
    } else if (method_len == 7 && memcmp(method, "OPTIONS", 7) == 0) {
        return HTTP_METHOD_OPTIONS;
    }
    return HTTP_METHOD_UNKNOWN;
}

char *status_text(http_status_t code) {
    switch (code) {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 204:
        return "No Content";
    case 500:
        return "Internal Error";
    case 505:
        return "Version Not Supported";
    default:
        return "Error";
    }
}

int build_path(http_request *request, char path[MAX_PATH]) {
    char index[] = "/index.html";
    int index_size;

    index_size = strlen(index);

    if (request->url_size > MAX_PATH - 1)
        return -2;

    if (request->url[0] == '/' && request->url_size == 1) {
        memcpy(path, index, index_size);
        path[index_size] = '\0';
    } else {
        memcpy(path, request->url, request->url_size);
        path[request->url_size] = '\0';
    }

    return 0;
}

int get_file_info(int root_fd, const char *requested_path, int *file_size, time_t *last_modified) {
    int ret, fd;
    struct stat st;

    fd = open_safe(root_fd, requested_path);
    if (fd < 0) {
        switch (errno) {
        case EACCES:
            return -3;
        case ELOOP:
            return -3;
        case ENOENT:
            return -2;
        default:
            return -1;
        }
        return 0;
    }

    if (!file_size && !last_modified) {
        close(fd);
        return 0;
    }

    ret = fstat(fd, &st);
    if (ret < 0) {
        perror("fstat error");
        close(fd);
        return -1;
    }

    *file_size = st.st_size;
    *last_modified = st.st_mtime;

    close(fd);
    return 0;
}

char *get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext)
        return "text/plain";

    ext++;

    if (strcmp(ext, "txt") == 0)
        return "text/plain";

    if (strcmp(ext, "html") == 0)
        return "text/html";

    if (strcmp(ext, "gif") == 0)
        return "image/gif";

    if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0)
        return "image/jpeg";

    if (strcmp(ext, "mpeg") == 0 || strcmp(ext, "mpg") == 0)
        return "video/mpeg";

    if (strcmp(ext, "doc") == 0 || strcmp(ext, "docx") == 0)
        return "application/msword";

    if (strcmp(ext, "pdf") == 0)
        return "application/pdf";

    if (strcmp(ext, "py") == 0)
        return "text/x-python";

    if (strcmp(ext, "php") == 0)
        return "text/x-php";

    if (strcmp(ext, "css") == 0)
        return "text/css";

    return "text/plain";
}

int check_path_traversal(const char *url, int url_size) {
    int i;

    for (i = 0; i < url_size - 1; i++) {
        if (url[i] == '\\')
            return 1;
        if (url[i] == '.' && url[i + 1] == '.')
            return 1;
        if (url[i] == '/' && url[i + 1] == '/')
            return 1;
    }

    return 0;
}

int open_safe(int root_fd, const char *requested_path) {
    struct open_how how;
    memset(&how, 0, sizeof(how));

    how.flags = O_RDONLY;
    how.resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS;

    /* +1 to skip first "/" */
    return syscall(SYS_openat2, root_fd, requested_path + 1, &how, sizeof(struct open_how));
}
