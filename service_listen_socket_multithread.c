#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <memory.h>
#include <string.h>
#include <libpq-fe.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "service_client_socket.h"
#include "service_listen_socket_multithread.h"
#include "get_listen_socket.h"
#include "make_printable_address.h"

/* struct to pass attributes to the thread
   because thread function takes single *void pointer */
typedef struct thread_control_block {
    int client; /* socket */
    struct sockaddr_in6 their_address;
    socklen_t their_address_size;
    SSL_CTX *ctx; // TODO check if this context has to be in thread or not
} thread_control_block_t;

// TODO check if it has to be removed
pthread_mutex_t mut;

/* client thread for serving a single client */
static void *client_thread(void *data) {
    /* cast data to the thread_control_block */
    thread_control_block_t *tcb_p = (thread_control_block_t *) data;

    /* making sure that we received correct attributes */
    assert(tcb_p->their_address_size == sizeof(tcb_p->their_address));

    /* make a printable address of the client
       for the easier identification and processing */
    char buffer[INET6_ADDRSTRLEN + 32];
    char *printable;
    printable = make_printable_address(&(tcb_p->their_address),
                                       tcb_p->their_address_size,
                                       buffer, sizeof(buffer));

    /* establish a SSL connection to the client */
    SSL *ssl = SSL_new(tcb_p->ctx);
    SSL_set_fd(ssl, tcb_p->client);
    int error = SSL_accept(ssl);

    /* if connection failed, most likely client is
       connecting via simple http, so we sent a redirect to https */
    if (error <= 0) {
        redirect_http_to_https(tcb_p->client);
    }

    /* call function to handle the requests from client */
    (void) service_client_socket(tcb_p->client, ssl, printable);

    /* cleanup */
    close(tcb_p->client);
    free(printable);        /* this was strdup'd */
    free(data);            /* this was malloc'd */
    pthread_exit(0);
}

/* function for receiving incoming connections from
   clients and serving them in separate threads for
   faster processing */
int service_listen_socket_multithread(const int s) {

    /* initialise ssl and create it's context */
    // TODO check what place is the best for these
    init_openssl();
    SSL_CTX *ctx = create_context();
    configure_context(ctx);

    /* accept takes a socket in the listening state, and waits until a
       connection arrives.  It returns the new connection, and updates the
       address structure and length (if supplied) with the address of the
       caller. */
    while (1) {
        /* create space to put information specific to the thread.
           a thread can access global variables, its OWN stack (ie,
           automatic variables in functions that have been executed from the
           thread start routine downwards) and malloc'd data.  What a thread
           MUST NOT access is data from another thread's stack, as that can
           disappear, change and generally cause problems.  Any data which
           is not known to belong exclusively to the thread must be locked
           when it is being accessed, which is outside the scope of this
           example.  */
        thread_control_block_t *tcb_p = malloc(sizeof(*tcb_p));

        if (tcb_p == 0) {
            /* if the allocation fails we print and error message and continue.
               this might happen due to the fact that all memory is
               allocated, so there is not much that can be done so
               we leave it and try again */
            perror("malloc");
            continue;
        }

        /* we store the size of the struct in order to later
           verify the fact that we received the correct struct */
        tcb_p->their_address_size = sizeof(tcb_p->their_address);

        /* we call accept as before, now put the data into the
           thread control block, rather than onto our own stack */
        int client;
        if ((client = accept(s, (struct sockaddr *) &(tcb_p->their_address),
                                              &(tcb_p->their_address_size))) < 0) {
            perror("accept");
            /* may as well carry on, this is probably temporary */
        } else {

            tcb_p->client = client;
            tcb_p->ctx = ctx;
            pthread_t thread;
            pthread_attr_t pthread_attr; /* attributes for newly created thread */

            /* we now create a thread and start it by calling
               client_thread with the tcb_p pointer.  That data was malloc'd,
               so is safe to use.  If we just have a structure on our own
               stack (ie, a variable local to this function) and passed a
               pointer to that, then there would be some exciting race
               conditions as it was destroyed (or not) before the created
               thread finished using it. */

            /* we initialise attributes */
            if (pthread_attr_init(&pthread_attr)) {
                fprintf(stderr, "Creating initial thread attributes failed!\n");
                goto error_exit; /* avoid break here in of the later
				   addition of an enclosing loop */
            }

            /* we set the thread to be of detached state in order not to leak
               the memory because by default joinable thread will not be cleaned
               up until it is joined */
            if (pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED)) {
                fprintf(stderr, "setting thread attributes failed!\n");
                goto error_exit; /* avoid break here in of the later
				   addition of an enclosing loop */
            }

            /* create separate thread for processing */
            if (pthread_create(&thread, &pthread_attr, &client_thread, (void *) tcb_p) != 0) {
                perror("pthread_create");
                goto error_exit; /* avoid break here in of the later
				   addition of an enclosing loop */
            }
        }
    }
    error_exit:
    cleanup_openssl();
    SSL_CTX_free(ctx);
    return -1;
}
