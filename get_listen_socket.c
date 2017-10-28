#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <memory.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <assert.h>

/* initialise the openSSL */
void init_openssl()
{
    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_ssl_algorithms();
}

/* do the cleanup when server exits */
void cleanup_openssl()
{
    ERR_free_strings();
    EVP_cleanup();
}

/* create the SSL context to work
   for a server */
SSL_CTX *create_context()
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = SSLv23_server_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

/* configure the context with correct certificate and key */
void configure_context(SSL_CTX *ctx)
{

    SSL_CTX_set_options(ctx, SSL_OP_SINGLE_DH_USE);

    /* Set the key and cert */
    if (SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM) <= 0 ) {
        ERR_print_errors_fp(stderr);
    }

    if (!SSL_CTX_check_private_key(ctx)) {
        ERR_print_errors_fp(stderr);
    }
}


int get_listen_socket(const int port) {
    fprintf(stderr, "binding  to port %d\n", port);
    struct sockaddr_in6 my_address;

    /* zero the structure out.  This is not strictly necessary, but costs
       almost nothing to do.  Note that some references use '0' (ie, bit
       pattern 0x30): they are wrong, it is '\0' (bit pattern 0x0, all
       zero bits). */

    memset(&my_address, '\0', sizeof(my_address));
    my_address.sin6_family = AF_INET6; /* this is an ipv6 address */
    my_address.sin6_addr = in6addr_any;  /* bind to all interfaces */
    my_address.sin6_port = htons(port); /* network order, for  historical reasons */


    /* get a socket to use for listening.  Note that many references will
       use AF_INET6 (or AF_INET) here.  In practice, AF_XXX and PF_XXX are
       then same value, but you are asking for a socket in a given
       protocol family here.  In theory, you could have two protocol
       families with the same address format */

    int s = socket(PF_INET6, SOCK_STREAM, 0);

    if (s < 0) {
        perror("socket");
        return -1;
    }

    /* as we are debugging and playing with code, this allows us to
       reliably kill the server and immediately restart it on the same
       port number */

    const int one = 1;

    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0) {
        perror("setsocket");
        /* but carry on, as it is only a 'nice to have' */
    }

    if (bind(s, (struct sockaddr *) &my_address, sizeof(my_address)) != 0) {
        perror("bind");
        return -1;
    }

    if (listen(s, 5) != 0) {
        perror("listen");
        return -1;
    }

    return s;
}


  
  


