#ifndef SERVER_H
#define SERVER_H

/**
 * @file server.h
 * @brief Exposes the core function to start the C++ web server.
 *
 * This header provides the declaration for the `start_web_server` function,
 * which is the main entry point for initializing and launching the backend
 * HTTP server. The use of `extern "C"` ensures C-style linkage, allowing
 * this function to be called from other parts of the application, potentially
 * written in C or other languages, without C++ name mangling issues.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and starts the web server in a background thread.
 *
 * This function sets up all the API routes (/api/...) and the static file
 * serving logic. It then launches the server's listening loop on
 * http://localhost:1234 in a separate thread, which prevents it from
 * blocking the main application thread.
 *
 * @return int Returns 0 upon successful launch of the server thread.
 *         Non-zero values would indicate an error (though not currently implemented).
 */
int start_web_server();

#ifdef __cplusplus
}
#endif

#endif // SERVER_H