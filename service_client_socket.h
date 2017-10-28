
// TODO remove all the simple sockets s from the methods
int service_client_socket(const int s, SSL *ssl, const char *const tag);

void service_get_request(PGconn *db_conn, SSL *ssl, char *path);

void service_not_found(PGconn *db_conn, SSL *ssl, char *message, char *page);

void service_head_request(PGconn *db_conn, SSL *ssl, char *path);

void service_options_request(PGconn *db_conn, SSL *ssl, char *path);

void service_trace_request(PGconn *db_conn, SSL *ssl, char **strings, int amount);

void service_post_request(PGconn *db_conn, SSL *ssl, int length, char *tag);

void redirect_http_to_https(const int s);