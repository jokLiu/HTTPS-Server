#include "util.h"

server_info *info;

int service_client_socket(const int s, SSL *ssl, const char *const tag);

int service_get_request(PGconn *db_conn, SSL *ssl, char *path);

int service_not_found(PGconn *db_conn, SSL *ssl, char *message, char *page);

int service_head_request(PGconn *db_conn, SSL *ssl, char *path);

void service_options_request(PGconn *db_conn, SSL *ssl, char *path);

void service_trace_request(PGconn *db_conn, SSL *ssl, char **strings, int amount);

int service_post_request(PGconn *db_conn, SSL *ssl, int length, char *tag);

void redirect_http_to_https(const int s);