# barehttp

A multithreaded HTTP server implemented in C using TCP sockets, a thread pool, configurable connection queues, HTTP request parsing, logging, and script execution.

The project also features an old personal portfolio and an online store as web application examples.

## Academic Context

This project was developed as part of the *Redes 2* course at Universidad Autónoma de Madrid. It was designed to explore network programming, focusing on low level socket and connection management.

## Features

* TCP connection handling
* Multithreaded request processing using a thread pool
* Configurable connection queue
* HTTP request and response handling
* HTTP request parsing using `picohttpparser`
* Static file serving
* Script execution
* Configurable request timeouts
* File-based logging
* Graceful shutdown on `SIGINT`
* Protection against path traversal
* Timeout-based protection against slow clients
* Functional and concurrency testing
* Doxygen code documentation

---

## Architecture

The server is divided into several components, each responsible for a specific part of the system:

1. `server.c`
   Contains the main program and handles server configuration.

2. `libhttp.a`
   Handles HTTP requests and responses, as well as script execution.

3. `libpicohttpparser.a`
   Uses [PicoHTTPParser](https://github.com/h2o/picohttpparser) from Kazuho Oku and contributors, a tiny fast HTTP parser written in C.

4. `libtcp.a`
   Handles TCP connections, including opening and closing connections and sending and receiving data.

5. `liblogs.a`
   Provides the file-based logging system.

6. `libqueue.a`
   Implements the queue used to manage accepted TCP connections waiting to be processed by worker threads.

---

## Requirements

The project requires:

* `gcc`
* `make`
* `libconfuse`
* `libconfuse-dev` on Debian-based systems

The test scripts additionally use:

* `bash`
* `curl`
* `ab` (Apache Benchmark, provided by `apache2-utils`)
* `python3`

---

## Build

The project uses `make` for compilation. It has two main targets for building the executable:

### Build the server

```bash
make develop / production
```

- `production` adds production flags (optimization)

- `develop` adds develop flags (sanitizers, lower optimization)

### Available targets

| Command           | Description                                |
| ----------------- | ------------------------------------------ |
| `make`            | Builds the libraries and server executable |
| `make develop`    | Builds with develop flags                  |
| `make production` | Builds with production flags               |
| `make libs`       | Builds only the libraries                  |
| `make docs`       | Generates Doxygen documentation            |
| `make clean`      | Removes object files, libraries, and logs  |
| `make clean_logs` | Removes log files                          |
| `make clean_libs` | Removes compiled libraries                 |
| `make clean_doc`  | Removes generated documentation            |

After building, the executable is located in the repository root:

```bash
./server
```

The server does not require command-line arguments. If it starts successfully, it does not print anything to standard output.

By default, server listens on port `8080` and accepts connections from any network interface.

---

## Configuration

The server can be configured through `server.conf`.

Configuration is implemented using the `libconfuse` library.

| Parameter          | Description                                                                                                                                   |       Default |
| ------------------ | --------------------------------------------------------------------------------------------------------------------------------------------- | ------------: |
| `listen_port`      | Port on which the server listens for incoming connections.                                                                                    |        `8080` |
| `server_root`      | Root directory containing the files served by the web server.                                                                                 |      `www` |
| `server_signature` | Server signature included in HTTP responses.                                                                                                  |      `server` |
| `log_file`         | Base name used for generated log files. Logs are stored in `logs/`.                                                                           | `server_logs` |
| `timeout_ms`       | Maximum time, in milliseconds, that a worker thread waits to receive data before closing the connection. A value of `0` disables the timeout. |        `1000` |
| `pool_size`        | Number of worker threads in the thread pool.                                                                                                  |           `8` |
| `queue_size`       | Maximum number of accepted connections waiting in the worker queue.                                                                           |          `32` |
| `backlog`          | Number of connections that can remain waiting while the connection queue is full.                                                             |          `10` |

Example configuration:

```conf
listen_port = 8080
server_root = www
server_signature = "barehttp"
log_file = "server_logs"
timeout_ms = 1000
pool_size = 8
queue_size = 32
backlog = 10
```

---

## Project Structure

```text
.
├── Makefile          # Project build system
├── server.conf       # Server configuration
├── src/              # Main program
├── srclib/           # Library source files
├── includes/         # Header files
├── obj/              # Compiled object files
├── lib/              # Static libraries (.a)
├── doc/              # Doxygen configuration and generated documentation
├── www/              # Example portfolio
├── store/            # Example web application root
├── logs/              # Server log files
└── pruebas/           # Test scripts
```

The server executable is generated in the repository root.

---

## Example Web Applications

The repository includes two web applications used to demonstrate the server in a realistic environment.

In `www` is my web development old portfolio, with some HTML, CSS and JS projects.

In `store` there is an online store containing a selection of products. Python scripts are provided to simulate shopping cart functionality, including: 
* Adding products to the cart
* Emptying the cart
* Displaying the cart

---

## Testing

The project includes several test scripts in the `pruebas/` directory.

### Apache Benchmark

`launch_ab.sh` runs Apache Benchmark against the server:

```bash
./launch_ab.sh N_REQUESTS N_CONCURRENT
```

The server port is read automatically from the configuration file.

### Curl tests

`launch_curls.sh` sends multiple requests using `curl` to `/index.html`.

### Basic functional tests

`basic_tests.py` tests several server scenarios, including:

* GET requests
* Non-existent resources
* Unsupported methods
* Other basic HTTP request scenarios

### Script execution tests

`scripts_tests.sh` verifies that script execution works correctly.

The test receives a port, a name, and a value, then performs a `POST` request and a `GET` request against `test.py`, passing the specified argument.

### Memory and concurrency testing

The server has also been tested with Valgrind to verify that resources are released correctly.

Concurrency has been tested using:

```bash
valgrind --tool=helgrind
```

These tests were used to check the concurrent execution of the server and detect potential race conditions.

Helgrind has reported a warning involving the `sigint` variable in some executions. The variable is declared as `volatile sig_atomic_t`, which provides atomic and signal-safe access according to POSIX. The warning is therefore not considered a real race condition in this implementation.

---

## Graceful Shutdown

The server shuts down when it receives `SIGINT`.

The default signal action is replaced to allow the server to perform an orderly shutdown and release its resources.

When `SIGINT` is received:

1. The parent process detects the signal and `accept()` returns with an error.
2. The parent broadcasts on the `not_empty` condition variable, waking worker threads that are waiting for connections.
3. Each worker thread is handled appropriately. If it has an active connection, the connection is shut down and the thread is joined.
4. The main listening connection is closed.
5. Allocated resources are released.

This ensures that resources are correctly released when the server exits.

### Shutting down active connections

The parent process uses `tcp_shutdown()` on active socket descriptors.

This does more than simply close the connection: it also causes worker threads blocked inside `tcp_recv()` to unblock and return `0`.

Using only `close()` was found to be insufficient in some situations, where a worker thread could remain blocked waiting for data even though its descriptor had been closed.

For this reason, the server uses `shutdown()` to explicitly unblock active connections during shutdown.

---

## Security Considerations

The server includes protections against several common HTTP server abuse scenarios.

### Path Traversal

Path traversal attempts try to access files outside the server's configured root directory by manipulating the requested path, for example with `../`.

The initial implementation only checked whether the first three characters of the requested path were `/..`:

```http
GET /../restricted/path HTTP/1.0
```

This was insufficient because a path such as the following would bypass the check:

```http
GET /media/../../restricted/path HTTP/1.0
```

The implementation was therefore changed to use `check_path_traversal()`, defined in `http.c`.

The function checks the requested path for traversal patterns and rejects matching requests with:

```text
403 Forbidden
```

This prevents the server from serving files outside the permitted path through these traversal patterns.

### Slow Clients

Slow HTTP clients can keep server threads occupied by establishing connections and sending request data very slowly.

Because the server uses a fixed-size thread pool, enough slow connections could prevent new clients from being processed.

To mitigate this, the server implements a configurable receive timeout.

The timeout defines the maximum amount of time a worker thread can wait for data from a connection. When the timeout is reached, the connection is closed.

The timeout can be configured through `server.conf`:

```conf
timeout_ms = 1000
```

A value of `0` disables the timeout.

### Symlinks

If the service being hosted by the server allows to upload files, a malicious attacker could create and upload a symlink pointing outside of the server root directory, and then get that file, gaining access to not allowed paths.

For example, a symlink could be uploaded pointing to `/etc/password`:

```bash
www
 | 
 ├── index.html
 |
 ├── secret -> /etc/password
```

Then, if not checked correctly, `GET secret` would return the content inside `/etc/password`.

A simple solution is calling `open` with `O_NOFOLLOW`. This prevents symlink files, but fails on preventing symlink directories.

To prevent all kind of symlink attacks, we changed the way in which we open files and execute CGI scripts.

When instanizating the server structure, we open the root directory and store the file descriptor. Then each the server access a file, it uses `openat2` with `RESOLVE_BENEATH` and `RESOLVE_NO_SYMLINKS` preventing symlink uses.

---

## Code Documentation

The source code is documented using Doxygen.

HTML documentation can be generated with:

```bash
make docs
```

The generated documentation is placed in the `doc/` directory.

---

## Technical Notes

The project focuses on several systems-programming concerns:

* POSIX socket programming
* TCP connection management
* Multithreading and race conditions
* Thread pools
* Producer/consumer synchronization
* HTTP request parsing
* Resource ownership and cleanup
* Signal handling
* Graceful shutdown
* File logging
* Request timeouts
* Basic HTTP security protections

The implementation also includes dedicated static libraries for the main subsystems, keeping networking, HTTP handling, logging, and connection management separated from the main server program.

### Epoll
- why is better
- Problems with implementations
- Benchmarking
- Sending big files.
- Process response splitting 


---

## Limitations

This project is intentionally focused on the functionality implemented in the current version.

The server should therefore be considered a lightweight HTTP server implementation rather than a production replacement for mature web servers.

The current repository does not aim to provide features that are not implemented and tested by the project.

---

## Future Work

Potential future improvements can be tracked separately from the current implementation.

The current priority is to keep the existing server stable, well-tested, documented, and maintainable before introducing additional functionality.

