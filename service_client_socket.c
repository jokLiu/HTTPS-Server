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
#include "service_client_socket.h"
#include "read_client_input.h"
#include "database_connection.h"
#include "util.h"

#define buffer_size 10240


int service_client_socket(const int s, const char *const tag) {
    // allocating array of pointers to the strings
    int buffSize = 20;
    char **strings = malloc(buffSize * sizeof(char *));
    char path[1000];
    char *pwd = getenv("PWD");
    strcpy(path, pwd);
    char *buffer;
    char *temp_buff;
    char *tag_name;
    char index_file[] = "index.html";

    // tokens keeping the request header info
    char *filepath;
    char *request_method;
    char *http_type;

    printf("new connection from %s\n", tag);

    int amount = 0;
    while ((amount = read_client_input(s, &strings)) > 0) {
        buffer = calloc(strlen(strings[0]) + 1, sizeof(char));
        strcpy(buffer, strings[0]);

        request_method = strtok_r(buffer, " \t\n", &temp_buff);
        if (buffer) {
            filepath = strtok_r(NULL, " \t\n", &temp_buff);
            filepath = basename(filepath);
            if (strlen(filepath) == 1) filepath = index_file;
        }
        if (buffer) {
            http_type = strtok_r(NULL, " \t\n", &temp_buff);
        }



        // if we get incorrect http request, respond to the client
        // and discard the request
        if (!http_type || !filepath || strncmp(http_type, "HTTP/1.1", 8) != 0) {
            if (http_type && strncmp(http_type, "HTTP", 4) == 0) {
                service_not_found(s, "HTTP/1.1 505 HTTP Version Not Supported\r\n", "versionnotsupported.html");
            } else {
                bad_request(s);
            }
            free(buffer);
            continue;
        }

        // handling GET request
        if (strncmp(request_method, "GET\0", 4) == 0) {
            if (check_exists(filepath) == 1) {
                service_get_request(s, filepath);
            } else {
                service_not_found(s, "HTTP/1.1 404 Not Found\r\n", "notfound.html");
            }
        } else if (strncmp(request_method, "HEAD\0", 5) == 0) {

            if (check_exists(filepath) == 1) {
                service_head_request(s, filepath);

            } else {
                service_not_found(s, "HTTP/1.1 404 Not Found\r\n", "notfound.html");
            }
        } else if (strncmp(request_method, "OPTIONS\0", 5) == 0) {
            service_options_request(s, filepath);

        } else if (strncmp(request_method, "TRACE\0", 6) == 0) {
            service_trace_request(s, strings, amount);

        } else if (strncmp(request_method, "POST\0", 5) == 0) {

            int length = find_content_length(strings, amount);
            if (length <= buffer_size) {
                tag_name = calloc(strlen(tag) + 1, sizeof(char));
                strcpy(tag_name, tag);
                service_post_request(s, filepath, length, tag_name);
                free(tag_name);
            } else {
                char *c = malloc(sizeof(char) * length);
                read(s, c, length);
                service_not_found(s, "HTTP/1.1 413 Payload Too Large\r\n", "file_too_large.html");

            }

        }
        free(buffer);
        free_memory_inside(strings, amount);

        // setting other memory to 0 for other fetchings
        memset(path + strlen(pwd), '\0', 1000 - +strlen(pwd));

    }

    free_memory(strings, amount);
    printf("connection from %s closed\n", tag);
    close(s);
    return 0;
}

void service_post_request(const int s, char *path, int length, char *tag) {
    char *content = malloc(sizeof(char) * (length + 1));
    char *str = malloc(sizeof(char) * 40);
    char *message;
    content[length] = '\0';
    read(s, content, length);

    printf("%s\n", content);
//    char* real_content = remove_unnecessary_lines(content);
//    printf("%s",real_content);
    PGresult *res = populate_table(tag, content);
    clear_result(res);

    PGresult *result1 = query_table(tag);
    char *uploaded_content = get_result_content(result1);
    if (!uploaded_content) {
        service_not_found(s, "HTTP/1.1 500 Internal Server Error\r\n", "internalerror.html");
        return;
    }

    PGresult *result2 = query_table("begin.html");
    char *begin = get_result_content(result2);
    if (!begin) {
        service_not_found(s, "HTTP/1.1 500 Internal Server Error\r\n", "internalerror.html");
        return;
    }

    PGresult *result3 = query_table("past.html");
    char *past = get_result_content(result3);
    if (!past) {
        service_not_found(s, "HTTP/1.1 500 Internal Server Error\r\n", "internalerror.html");
        return;
    }

    int size = strlen(uploaded_content);
    size += strlen(begin);
    size += strlen(past);

    sprintf(str, "Content-Length: %d\r\n", size);
    message = "HTTP/1.1 200 OK\r\n";
    write(s, message, strlen(message));
    message = "Host: localhost:1235\r\n";
    write(s, message, strlen(message));
    write(s, str, strlen(str));
    message = "Content-Type: text/html\r\n";
    write(s, message, strlen(message));
    message = "Connection: keep-alive\r\n\r\n";
    write(s, message, strlen(message));

    write(s, begin, strlen(begin));
    write(s, uploaded_content, strlen(uploaded_content));
    write(s, past, strlen(past));


    clear_result(result1);
    clear_result(result2);
    clear_result(result3);
    free(str);
    free(content);

}

void service_options_request(const int s, char *path) {
    char *message;

    if (strncmp(path, "*", 1) == 0) {
        message = "HTTP/1.1 200 OK\r\n";
        write(s, message, strlen(message));
        message = "Host: localhost:1235\r\n";
        write(s, message, strlen(message));
        message = "Allow: GET,HEAD,POST,OPTIONS,TRACE\r\n";
        write(s, message, strlen(message));
        message = "Connection: keep-alive\r\n\r\n";
        write(s, message, strlen(message));

    } else if (check_exists(path) == 1) {
        message = "HTTP/1.1 200 OK\r\n";
        write(s, message, strlen(message));
        if (strncmp(path, "upload.html", 11) == 0) {
            message = "Allow: GET,HEAD,POST\r\n";
            write(s, message, strlen(message));
        } else {
            message = "Allow: GET,HEAD\r\n";
            write(s, message, strlen(message));
        }
    } else {
        service_not_found(s, "HTTP/1.1 404 Not Found\r\n", "notfound.html");
    }
}


void service_get_request(const int s, char *path) {
    // allocate the right amount of the size
    char *str = malloc(sizeof(char) * 40);
    char *message;

    // allocate the right amount of the size
    PGresult *res = query_table(path);
    printf("%s\n", PQresultErrorMessage(res));
    char *buffer = get_result_content(res);

    if (!buffer) {
        service_not_found(s, "HTTP/1.1 500 Internal Server Error\r\n", "internalerror.html");
        return;
    }
    sprintf(str, "Content-Length: %d\r\n", (int) strlen(buffer));
    message = "HTTP/1.1 200 OK\r\n";
    write(s, message, strlen(message));
    message = "Host: localhost:1235\r\n";
    write(s, message, strlen(message));
    write(s, str, strlen(str));
    message = "Content-Type: text/html\r\n";
    write(s, message, strlen(message));
    message = "Connection: keep-alive\r\n\r\n";
    write(s, message, strlen(message));
    write(s, buffer, strlen(buffer));

    // cleanup
    clear_result(res);
    free(str);

}


void service_head_request(const int s, char *path) {
    // allocate the right amount of the size
    char *str = malloc(sizeof(char) * 40);
    char *message;

    // allocate the right amount of the size
    PGresult *res = query_table(path);
    printf("%s\n", PQresultErrorMessage(res));

    char *buffer = get_result_content(res);

    if (!buffer) return;

    sprintf(str, "Content-Length: %d\r\n", (int) strlen(buffer));
    message = "HTTP/1.1 200 OK\r\n";
    write(s, message, strlen(message));
    message = "Host: localhost:1235\r\n";
    write(s, message, strlen(message));
    write(s, str, strlen(str));
    message = "Content-Type: text/html\r\n";
    write(s, message, strlen(message));
    message = "Connection: keep-alive\r\n\r\n";
    write(s, message, strlen(message));
    // cleanup
    clear_result(res);
    free(str);
}

void service_not_found(const int s, char *message, char *page) {
    // allocate the right amount of the size
    char *str = malloc(sizeof(char) * 40);

    // allocate the right amount of the size
    PGresult *res = query_table(page);
    printf("%s\n", PQresultErrorMessage(res));


    char *buffer = get_result_content(res);
    sprintf(str, "Content-Length: %zu\r\n", strlen(buffer));
    write(s, message, strlen(message));
    message = "Host: localhost:1235\r\n";
    write(s, message, strlen(message));
    write(s, str, strlen(str));
    message = "Content-Type: text/html\r\n";
    write(s, message, strlen(message));
    message = "Connection: keep-alive\r\n\r\n";
    write(s, message, strlen(message));
    write(s, buffer, strlen(buffer));

    // cleanup
    clear_result(res);
    free(str);
}


void bad_request(const int s) {

    // allocate the right amount of the size
    char *str = malloc(sizeof(char) * 40);
    char *message;
    // allocate the right amount of the size
    PGresult *res = query_table("badrequest.html");
    printf("%s\n", PQresultErrorMessage(res));

    char *buffer = get_result_content(res);

    // write the key information to the client
    sprintf(str, "Content-Length: %d\n", (int) strlen(buffer));
    message = "HTTP/1.1 400 Bad Request\r\n";
    write(s, message, strlen(message));
    message = "Host: localhost:1235\r\n";
    write(s, message, strlen(message));
    write(s, str, strlen(str));
    message = "Connection: close\r\n\r\n";
    write(s, message, strlen(message));
    write(s, buffer, strlen(buffer));

    // cleanup
    clear_result(res);
    free(str);
}


void service_trace_request(const int s, char **strings, int amount) {
    char *str = calloc(100, sizeof(char));
    char *message;
    sprintf(str, "Content-Length: %d\r\n", content_size(strings, amount) + amount * 2 + 2);
    message = "HTTP/1.1 200 OK\r\n";
    write(s, message, strlen(message));
    message = "Host: localhost:1235\r\n";
    write(s, message, strlen(message));
    write(s, str, strlen(str));
    message = "Connection: close\r\n\r\n";
    write(s, message, strlen(message));
    for (int i = 0; i < amount; i++) {
        write(s, strings[i],
              strlen(strings[i]));
        write(s, "\r\n", 2);
    }
    write(s, "\r\n", 2);
    free(str);
}

