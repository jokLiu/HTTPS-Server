int get_listen_socket(const int port);

void init_openssl();

void cleanup_openssl();

SSL_CTX *create_context();

void configure_context(SSL_CTX *ctx);

