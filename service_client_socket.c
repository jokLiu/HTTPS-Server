#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <memory.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libgen.h>
#include <assert.h>
#include <libpq-fe.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "service_client_socket.h"
#include "read_client_input.h"
#include "database_connection.h"
#include "util.h"

/* the maximum size of the payload received by POST request */
#define MAX_CONTENT_SIZE 102400

void write_to_client(SSL *ssl, char *message);

void service_upload_file(PGconn *db_conn, SSL *ssl, char *tag);


/* the main function for serving a single client.
   the received request is parsed and responded according to
   the type of request received.
   the methods implemented by the server includes:
   GET,HEAD,POST,OPTIONS,TRACE. */
int service_client_socket(const int s, SSL *ssl, const char *const tag) {
    printf("new connection from %s\n", tag);

    /* allocating array of pointers to the strings
       the strings are going to store all the data received from client.
       Each string represents a line from the request. */
    int initial_array_size = 20;
    char **strings = malloc(initial_array_size * sizeof(char *));

    PGconn *db_conn = create_connection();

    /* buffers used for the header parsing */
    char *buffer;
    char *temp_buff;

    /* the copy of the printable client tag
       which is unique among all the currently running clients */
    char *tag_name = strdup(tag);

    /* default path if null URI is received */
    char *index_file = "index.html";

    /* tokens keeping the request header info */
    char *filepath; /* the file name */
    char *request_method; /* request method e.g. GET, POST, etc. */
    char *http_type; /* type should be HTTP/1.1 */

    /* number of strings received */
    int amount = 0;

    /* we loop until requests are coming from client.
       with each loop we parse the request, and respond to
       it based on the actual request received */
    while ((amount = read_client_input(ssl, &strings)) > 0) {

        /* prepare the first line of the request for parsing */
        buffer = calloc(strlen(strings[0]) + 1, sizeof(char));
        strcpy(buffer, strings[0]);

        /* we parse the first line into 3 attributes described above */
        request_method = strtok_r(buffer, " \t\n", &temp_buff);
        if (buffer) {
            filepath = strtok_r(NULL, " \t\n", &temp_buff);

            /* we remove any leading or trailing elements */
            filepath = basename(filepath);

            /* if the filepath is equal to 1 it means we received
               null URI that is /  so we have to serve index file */
            if (strlen(filepath) == 1) filepath = index_file;
        }
        if (buffer) {
            http_type = strtok_r(NULL, " \t\n", &temp_buff);
        }



        /* if we get incorrect http request, respond to the client
           and discard the request */
        if (!http_type || !filepath || strncmp(http_type, "HTTP/1.1", 8) != 0) {
            /* if different version of HTTP was received
               indicate unsupported version to the client */
            if (http_type && strncmp(http_type, "HTTP", 4) == 0) {
                service_not_found(db_conn, ssl, "HTTP/1.1 505 HTTP Version Not Supported\r\n", "versionnotsupported.html");
            } else {
                service_not_found(db_conn, ssl, "HTTP/1.1 400 Bad Request\r\n", "badrequest.html");
            }
            free(buffer);
            continue;
        }

        /* if we received GET request, we handle it accordingly */
        if (strncmp(request_method, "GET\0", 4) == 0) {

            /* if the requested page exists, serve it */
            if (check_exists(db_conn,filepath) == 1) {
                service_get_request(db_conn, ssl, filepath);
            }
                /* if a client requests getfile, it means a client
                   wants to retrieve his file he stored with POST method */
            else if (strcmp(filepath, "getfile\0") == 0) {
                if (check_exists(db_conn,strdup(tag_name)) == 1) {
                    service_upload_file(db_conn, ssl, tag_name);
                }
                    /* if client haven't stored file, serve the appropriate page */
                else service_get_request(db_conn, ssl, "filenotuploaded.html");
            }
                /* in any other cases indicate page not found */
            else {
                service_not_found(db_conn, ssl, "HTTP/1.1 404 Not Found\r\n", "notfound.html");
            }
        }
            /* if we received HEAD request, we handle it accordingly */
        else if (strncmp(request_method, "HEAD\0", 5) == 0) {
            /* if the requested page exists, respond */
            if (check_exists(db_conn,filepath) == 1) {
                service_head_request(db_conn, ssl, filepath);

            }
                /* in any other cases indicate page not found */
            else {
                service_not_found(db_conn, ssl, "HTTP/1.1 404 Not Found\r\n", "notfound.html");
            }
        }
            /* if we received OPTIONS request, we handle it accordingly */
        else if (strncmp(request_method, "OPTIONS\0", 5) == 0) {
            service_options_request(db_conn, ssl, filepath);

        }
            /* if we received TRACE request, we handle it accordingly */
        else if (strncmp(request_method, "TRACE\0", 6) == 0) {
            service_trace_request(db_conn, ssl, strings, amount);

        }
            /* if we received POST request, we handle it accordingly */
        else if (strncmp(request_method, "POST\0", 5) == 0) {

            /* first we retrieve the size of the payload sent */
            int length = find_content_length(strings, amount);
            /* if the size is too large it we respond
               to the client with code 413 */
            if (length <= MAX_CONTENT_SIZE) {
                service_post_request(db_conn, ssl, length, tag_name);
            } else {

                char *c = "";
                /* consume the payload without storing it in user space */
                recv(s, c, length, MSG_TRUNC);

                /* indicate violated conditions to the client */
                service_not_found(db_conn, ssl, "HTTP/1.1 413 Payload Too Large\r\n", "file_too_large.html");
            }

        }
        /* single request cleanup */
        free(buffer);
        free_memory_inside(strings, amount);
    }

    /* cleanup */

    /* removing the file user uploaded */
    tag_name = strdup(tag);
    PGresult *res = delete_row(db_conn, tag_name);
    clear_result(res);
    do_exit(db_conn);

    /* fre'ing the allocated memory */
    free_memory(strings, amount);

    /* closing the SSL connection and socket */
    SSL_free(ssl);
    close(s);
    free(tag_name);


    printf("connection from %s closed\n", tag);

    return 0;
}

/* serving the POST request based on the request received */
void service_post_request(PGconn *db_conn, SSL* ssl, int length, char *tag) {

    /* allocate the right amount for storing the payload
       received from client, and then read it into buffer */
    char *content = malloc(sizeof(char) * (length + 1));
    content[length] = '\0';
    SSL_read(ssl, content, length);

    /* remove unneeded parts of the payload */
    char *real_content = remove_unnecessary_lines(content);

    /* store the payload to the database for later processing */
    PGresult *res = populate_table(db_conn,tag, real_content);

    /* send the response the the POST method */
    service_upload_file(db_conn, ssl, tag);

    /* cleanup */
    clear_result(res);
    free(content);
}

/* function for sending a client the content he uploaded using the
   POST method, it also embeds the content into html for the
   better visual representation and alignment */
void service_upload_file(PGconn *db_conn,SSL *ssl, char *tag) {
    /* string for storing the content length HTTP token */
    char *str = malloc(sizeof(char) * 40);

    /* retrieve the data from database */
    PGresult *result1 = query_table(db_conn, tag);
    char *uploaded_content = get_result_content(result1);
    if (!uploaded_content) {
        service_not_found(db_conn, ssl, "HTTP/1.1 500 Internal Server Error\r\n", "internalerror.html");
        return;
    }

    /* retrieve the head of the html for serving to the client */
    PGresult *result2 = query_table(db_conn, "begin.html");
    char *begin = get_result_content(result2);
    if (!begin) {
        service_not_found(db_conn, ssl, "HTTP/1.1 500 Internal Server Error\r\n", "internalerror.html");
        return;
    }

    /* retrieve the end of the html for serving to client */
    PGresult *result3 = query_table(db_conn, "past.html");
    char *past = get_result_content(result3);
    if (!past) {
        service_not_found(db_conn, ssl, "HTTP/1.1 500 Internal Server Error\r\n", "internalerror.html");
        return;
    }

    /* calculate the total size of the payload */
    int size = strlen(uploaded_content);
    size += strlen(begin);
    size += strlen(past);

    /* write header to the client */
    sprintf(str, "Content-Length: %d\r\n", size);
    write_to_client(ssl, "HTTP/1.1 200 OK\r\n");
    write_to_client(ssl, "Host: localhost\r\n");
    write_to_client(ssl, str);
    write_to_client(ssl, "Content-Type: text/html\r\n");
    write_to_client(ssl, "Connection: keep-alive\r\n\r\n");

    /* send the actual payload */
    write_to_client(ssl, begin);
    write_to_client(ssl, uploaded_content);
    write_to_client(ssl, past);

    /* cleanup */
    clear_result(result1);
    clear_result(result2);
    clear_result(result3);
    free(str);
}

/* serving the OPTIONS request based on the request received */
void service_options_request(PGconn *db_conn, SSL *ssl, char *path) {

    /* if the request received included '*' it means
       we have to respond with all the options used by the server */
    if (strncmp(path, "*", 1) == 0) {
        write_to_client(ssl, "HTTP/1.1 200 OK\r\n");
        write_to_client(ssl, "Host: localhost\r\n");
        write_to_client(ssl, "Allow: GET,HEAD,POST,OPTIONS,TRACE\r\n");
        write_to_client(ssl, "Connection: keep-alive\r\n\r\n");

    }
        /* else if we check weather the page exists.
           if so then we respond with appropriate methods used by that page */
    else if (check_exists(db_conn, path) == 1) {
        write_to_client(ssl, "HTTP/1.1 200 OK\r\n");
        write_to_client(ssl, "Host: localhost\r\n");

        /* only upload.html consists of POST method */
        if (strncmp(path, "upload.html", 11) == 0) {
            write_to_client(ssl, "Allow: GET,HEAD,POST\r\n");
        } else {
            write_to_client(ssl, "Allow: GET,HEAD\r\n");
        }
        write_to_client(ssl, "Connection: keep-alive\r\n\r\n");
    }
        /* otherwise we indicate that the request was not recognised */
    else {
        service_not_found(db_conn, ssl, "HTTP/1.1 404 Not Found\r\n", "notfound.html");
    }
}


/* serving the GET request based on the request received */
void service_get_request(PGconn *db_conn, SSL *ssl, char *path) {
    /* string for storing the content length HTTP token */
    char *str = malloc(sizeof(char) * 40);

    /* query table to retrieve the requested page */
    PGresult *res = query_table(db_conn, path);
    char *buffer = get_result_content(res);

    /* if we failed to retrieve the page, we are in trouble
       and we indicate that server had an internal error */
    if (!buffer) {
        service_not_found(db_conn, ssl, "HTTP/1.1 500 Internal Server Error\r\n", "internalerror.html");
        return;
    }

    /* write header to the client */
    sprintf(str, "Content-Length: %d\r\n", (int) strlen(buffer));
    write_to_client(ssl, "HTTP/1.1 200 OK\r\n");
    write_to_client(ssl, "Host: localhost\r\n");
    write_to_client(ssl, str);
    write_to_client(ssl, "Content-Type: text/html\r\n");
    write_to_client(ssl, "Connection: keep-alive\r\n\r\n");

    /* send the actual payload */
    write_to_client(ssl, buffer);

    /* cleanup */
    clear_result(res);
    free(str);

}

/* serving the GET request based on the request received */
void service_head_request(PGconn *db_conn,SSL *ssl, char *path) {
    /* string for storing the content length HTTP token */
    char *str = malloc(sizeof(char) * 40);

    /* query table to retrieve the requested page */
    PGresult *res = query_table(db_conn, path);
    char *buffer = get_result_content(res);

    /* if we failed to retrieve the page, we are in trouble
       and we indicate that server had an internal error */
    if (!buffer) {
        service_not_found(db_conn, ssl, "HTTP/1.1 500 Internal Server Error\r\n", "internalerror.html");
        return;
    }

    /* write header to the client */
    sprintf(str, "Content-Length: %d\r\n", (int) strlen(buffer));
    write_to_client(ssl, "HTTP/1.1 200 OK\r\n");
    write_to_client(ssl, "Host: localhost\r\n");
    write_to_client(ssl, str);
    write_to_client(ssl, "Content-Type: text/html\r\n");
    write_to_client(ssl, "Connection: keep-alive\r\n\r\n");

    /* cleanup */
    clear_result(res);
    free(str);
}

/* function for responding to the client with an appropriate message
   and page when client tried to retrieve data which is not available */
void service_not_found(PGconn *db_conn,SSL *ssl, char *message, char *page) {
    /* string for storing the content length HTTP token */
    char *str = malloc(sizeof(char) * 40);


    PGresult *res = query_table(db_conn, page);
    char *buffer = get_result_content(res);

    /* if we failed to retrieve the page, we are in trouble
       and we indicate that server had an internal error */
    if (!buffer) {
        service_not_found(db_conn, ssl, "HTTP/1.1 500 Internal Server Error\r\n", "internalerror.html");
        return;
    }

    /* write header to the client */
    sprintf(str, "Content-Length: %zu\r\n", strlen(buffer));
    write_to_client(ssl, message);
    write_to_client(ssl, "Host: localhost\r\n");
    write_to_client(ssl, str);
    write_to_client(ssl, "Content-Type: text/html\r\n");
    write_to_client(ssl, "Connection: keep-alive\r\n\r\n");
    write_to_client(ssl, buffer);

    /* cleanup */
    clear_result(res);
    free(str);
}


/* serving the TRACE request based on the request received */
void service_trace_request(PGconn *db_conn, SSL *ssl, char **strings, int amount) {
    /* string for storing the content length HTTP token */
    char *str = calloc(100, sizeof(char));
    sprintf(str, "Content-Length: %d\r\n", content_size(strings, amount) + amount * 2 + 2);

    /* write header to the client */
    write_to_client(ssl, "HTTP/1.1 200 OK\r\n");
    write_to_client(ssl, "Host: localhost\r\n");
    write_to_client(ssl, str);
    write_to_client(ssl, "Connection: close\r\n\r\n");

    /* write the content we received back to the client */
    for (int i = 0; i < amount; i++) {
        write_to_client(ssl, strings[i]);
        write_to_client(ssl, "\r\n");
    }
    write_to_client(ssl, "\r\n");

    /* cleanup */
    free(str);
}

/* if the client sends us simple http request we redirect client
   to use https (with SSL) for the secure communication */
void redirect_http_to_https(const int s) {
    char *message;
    PGconn *db_conn = create_connection();

    /* string for storing the content length HTTP token */
    char *str = malloc(sizeof(char) * 40);

    /* query table to retrieve the requested page */
    PGresult *res = query_table(db_conn, "redirect.html");
    char *buffer = get_result_content(res);

    sprintf(str, "Content-Length: %zu\r\n", strlen(buffer));

    /* write header to the client */
    message = "HTTP/1.1 301 Moved Permanently\r\n";
    write(s, message, strlen(message));
    message = "Location: https://localhost:1234\r\n";
    write(s, message, strlen(message));
    message = "Content-Type: text/html\r\n";
    write(s, message, strlen(message));
    write(s, str, strlen(str));
    message = "Connection: close\r\n\r\n";
    write(s, message, strlen(message));

    /* send the content of redirect page to the client */
    write(s, buffer, strlen(buffer));

    /* cleanup */
    do_exit(db_conn);
    clear_result(res);
    free(str);
}

/* helper write function to improve code readability */
void write_to_client(SSL *ssl, char *message) {
    SSL_write(ssl, message, strlen(message));
}