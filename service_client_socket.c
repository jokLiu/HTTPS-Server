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

#define buffer_size 10240


void bad_request(const int s, PGconn *conn);

int content_size(char **strings, int s);

int find_content_length(char **strings, int s);

int count_file_length(FILE *file);

void service_get_request(const int s, char *path, PGconn *conn);

void service_not_found(const int s, PGconn *conn, char *message, char *page);

void service_head_request(const int s, char *path, PGconn *conn);

void service_options_request(const int s, char *path, PGconn *conn);

void service_trace_request(const int s, char **strings, int amount);

void service_post_request(const int s, char *path, PGconn *conn, int length, char *tag);

int service_client_socket(const int s, const char *const tag, PGconn *conn) {
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
        filepath = strtok_r(NULL, " \t\n", &temp_buff);
        filepath = basename(filepath);
        http_type = strtok_r(NULL, " \t\n", &temp_buff);

        if (strlen(filepath) == 1) filepath = index_file;

        // if we get incorrect http request, respond to the client
        // and discard the request
        if (strncmp(http_type, "HTTP/1.1", 8) != 0) {
            if (strncmp(http_type, "HTTP", 4) == 0) {
                service_not_found(s, conn, "HTTP/1.1 505 HTTP Version Not Supported\r\n", "versionnotsupported.html");
            } else {
                bad_request(s, conn);
            }
            continue;
        }

        // handling GET request
        if (strncmp(request_method, "GET\0", 4) == 0) {
            if (check_exists(conn, filepath) == 1) {
                service_get_request(s, filepath, conn);
            } else {
                service_not_found(s, conn, "HTTP/1.1 404 Not Found\r\n", "notfound.html");
            }
        } else if (strncmp(request_method, "HEAD\0", 5) == 0) {

            if (check_exists(conn, filepath) == 1) {
                service_head_request(s, filepath, conn);

            } else {
                service_not_found(s, conn, "HTTP/1.1 404 Not Found\r\n", "notfound.html");
            }
        } else if (strncmp(request_method, "OPTIONS\0", 5) == 0) {
            service_options_request(s, filepath, conn);

        } else if (strncmp(request_method, "TRACE\0", 6) == 0) {
            service_trace_request(s, strings, amount);

        } else if (strncmp(request_method, "POST\0", 5) == 0) {

            int length = find_content_length(strings, amount);
            if (length <= buffer_size) {
                tag_name = calloc(strlen(tag) + 1, sizeof(char));
                strcpy(tag_name, tag);
                service_post_request(s, filepath, conn, length, tag_name);
                free(tag_name);
            } else {
                // TODO check if txt
                // TODO write file too large
            }

        }
        free(buffer);
        // setting other memory to 0 for other fetchings
        memset(path + strlen(pwd), '\0', 1000 - +strlen(pwd));

    }

    free_memory(strings, amount);
    printf("connection from %s closed\n", tag);
    close(s);
    return 0;
}

void service_post_request(const int s, char *path, PGconn *conn, int length, char *tag) {
    char *content = malloc(sizeof(char) * (length + 1));
    char *str = malloc(sizeof(char) * 40);
    content[length] = '\0';
    read(s, content, length);

    printf("%s\n", content);
    PGresult *res = populate_table(conn, tag, content);
    clear_result(res);

    PGresult *result1 = query_table(conn, tag);
    char *uploaded_content = get_result_content(result1);
    if (!uploaded_content) {
        service_not_found(s, conn, "HTTP/1.1 500 Internal Server Error\r\n", "internalerror.html");
        return;
    }

    PGresult *result2 = query_table(conn, "begin.html");
    char *begin = get_result_content(result2);
    if (!begin) {
        service_not_found(s, conn, "HTTP/1.1 500 Internal Server Error\r\n", "internalerror.html");
        return;
    }

    PGresult *result3 = query_table(conn, "past.html");
    char *past = get_result_content(result3);
    if (!past) {
        service_not_found(s, conn, "HTTP/1.1 500 Internal Server Error\r\n", "internalerror.html");
        return;
    }

    int size = strlen(uploaded_content);
    size += strlen(begin);
    size += strlen(past);

    sprintf(str, "Content-Length: %d\n", size);
    write(s, "HTTP/1.1 200 OK\n", 16);
    write(s, str, strlen(str));
    write(s, "Content-Type: text/html\n", 24);
    write(s, "Connection: Closed\n\n", 20);

    write(s, begin, strlen(begin));
    write(s, uploaded_content, strlen(uploaded_content));
    write(s, past, strlen(past));


    clear_result(result1);
    clear_result(result2);
    clear_result(result3);
    free(str);
    free(content);

}

void service_options_request(const int s, char *path, PGconn *conn) {
    if (strncmp(path, "*", 1) == 0) {
        write(s, "HTTP/1.1 200 OK\n", 16);
        write(s, "Allow: GET,HEAD,POST,OPTIONS,TRACE\n", 35);
        write(s, "Connection: Closed\n\n", 20);
    } else if (check_exists(conn, path) == 1) {
        write(s, "HTTP/1.1 200 OK\n", 16);
        if (strncmp(path, "upload.html", 11) == 0) {
            write(s, "Allow: GET,HEAD,POST\n", 21);
        } else {
            write(s, "Allow: GET,HEAD\n", 16);
        }
    } else {
        service_not_found(s, conn, "HTTP/1.1 404 Not Found\r\n", "notfound.html");
    }
}


void service_get_request(const int s, char *path, PGconn *conn) {
    // allocate the right amount of the size
    char *str = malloc(sizeof(char) * 40);

    // allocate the right amount of the size
    PGresult *res = query_table(conn, path);
    printf("%s\n", PQresultErrorMessage(res));
    char *buffer = get_result_content(res);

    if (!buffer) {
        service_not_found(s, conn, "HTTP/1.1 500 Internal Server Error\r\n", "internalerror.html");
        return;
    }
    sprintf(str, "Content-Length: %d\n", (int) strlen(buffer));
    write(s, "HTTP/1.1 200 OK\n", 16);
    write(s, str, strlen(str));
    write(s, "Content-Type: text/html\n", 24);
    write(s, "Connection: Closed\n\n", 20);
    write(s, buffer, strlen(buffer));

    // cleanup
    clear_result(res);
    free(str);

}


void service_head_request(const int s, char *path, PGconn *conn) {
    // allocate the right amount of the size
    char *str = malloc(sizeof(char) * 40);

    // allocate the right amount of the size
    PGresult *res = query_table(conn, path);
    printf("%s\n", PQresultErrorMessage(res));

    char *buffer = get_result_content(res);

    if (!buffer) return;

    sprintf(str, "Content-Length: %d\n", (int) strlen(buffer));
    write(s, "HTTP/1.1 200 OK\n", 16);
    write(s, str, strlen(str));
    write(s, "Content-Type: text/html\n", 24);
    write(s, "Connection: Closed\n\n", 20);

    // cleanup
    clear_result(res);
    free(str);
}

void service_not_found(const int s, PGconn *conn, char *message, char *page) {
    // allocate the right amount of the size
    char *str = malloc(sizeof(char) * 40);

    // allocate the right amount of the size
    PGresult *res = query_table(conn, page);
    printf("%s\n", PQresultErrorMessage(res));

    char *buffer = get_result_content(res);
    sprintf(str, "Content-Length: %d\n", (int) strlen(buffer));
    write(s, message, strlen(message));
    // write (s, "Date: Mon, 27 Jul 2009 12:28:53 GMT\n", 36);
    write(s, str, strlen(str));
    write(s, "Content-Type: text/html\n", 24);
    write(s, "Connection: Closed\n\n", 20);
    write(s, buffer, strlen(buffer));

    // cleanup
    clear_result(res);
    free(str);
}


void bad_request(const int s, PGconn *conn) {

    // allocate the right amount of the size
    char *str = malloc(sizeof(char) * 40);

    // allocate the right amount of the size
    PGresult *res = query_table(conn, "badrequest.html");
    printf("%s\n", PQresultErrorMessage(res));

    char *buffer = get_result_content(res);
    // write the key information to the client
    sprintf(str, "Content-Length: %d\n", (int) strlen(buffer));
    write(s, "HTTP/1.1 400 Bad Request\n", 25);
    write(s, str, strlen(str));
    write(s, "Connection: Closed\n\n", 20);
    write(s, buffer, strlen(buffer));

    // cleanup
    clear_result(res);
    free(str);
}


void service_trace_request(const int s, char **strings, int amount) {
    char *str = calloc(100, sizeof(char));
    sprintf(str, "Content-Length: %d\n", content_size(strings, amount) + amount * 2 + 2);
    write(s, "HTTP/1.1 200 OK\n", 16);
    write(s, str, strlen(str));
    write(s, "Connection: Closed\n\n", 20);
    for (int i = 0; i < amount; i++) {
        write(s, strings[i],
              strlen(strings[i]));
        write(s, "\r\n", 2);
    }
    write(s, "\r\n", 2);
    free(str);
}

int content_size(char **strings, int s) {
    int size = 0;
    for (int i = 0; i < s; i++) {
        size += strlen(strings[i]);
    }
    return size;
}


int find_content_length(char **strings, int s) {
    for (int i = 0; i < s; i++) {
        if (strncmp("Content-Length: ", strings[i], 16) == 0) {
            char *word = strings[i];
            return atoi(word + 16);
        }
    }
    return 0;
}


int count_file_length(FILE *file) {
    fseek(file, 0L, SEEK_END);
    int size = ftell(file);
    fseek(file, 0L, SEEK_SET);
    return size;
}

