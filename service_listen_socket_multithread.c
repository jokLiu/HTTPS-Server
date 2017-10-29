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
#include <semaphore.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <time.h>
#include "service_client_socket.h"
#include "service_listen_socket_multithread.h"
#include "get_listen_socket.h"
#include "database_connection.h"
#include "make_printable_address.h"


static pthread_mutex_t *lockarray;
static sem_t lock;


/* provide the implementation of static locking with callbacks.
   by using these callbacks, OpenSSL can manage its own internal thread safety
   and we will not have to worry about protecting calls into the library with
   our own thread-management code.
   this allows us to avoid different double after-free and
   additional overhead with OpenSSL and threading */

/* a callback function for locking and unlocking the given thread. */
static void lock_callback(int mode, int type, char *file, int line) {
    if (mode & CRYPTO_LOCK) {
        pthread_mutex_lock(&(lockarray[type]));
    } else {
        pthread_mutex_unlock(&(lockarray[type]));
    }
}

/* callback function which returns an unsigned long number
   that should uniquely identify the calling thread from all other threads. */
static unsigned long thread_id(void) {
    unsigned long ret;
    ret = (unsigned long) pthread_self();
    return ret;
}

/* helper function to initialize all the locks and callbacks */
static void init_locks(void) {
    lockarray = (pthread_mutex_t *) OPENSSL_malloc(CRYPTO_num_locks() *
                                                   sizeof(pthread_mutex_t));
    for (int i = 0; i < CRYPTO_num_locks(); i++) {
        pthread_mutex_init(&(lockarray[i]), NULL);
    }

    CRYPTO_set_id_callback((unsigned long (*)()) thread_id);
    CRYPTO_set_locking_callback((void (*)()) lock_callback);
}

/* helper function to destroy all the locks and callbacks */
static void kill_locks(void) {
    CRYPTO_set_locking_callback(NULL);
    for (int i = 0; i < CRYPTO_num_locks(); i++)
        pthread_mutex_destroy(&(lockarray[i]));

    OPENSSL_free(lockarray);
}


/* struct to pass attributes to the thread
   because thread function takes single *void pointer */
typedef struct thread_control_block {
    int client; /* socket */
    struct sockaddr_in6 their_address;
    socklen_t their_address_size;
    SSL_CTX *ctx;
} thread_control_block_t;


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

    /* we allow only certain number of requests due to limited
       amount of open file descriptors per user which is usually 1024 */
    sem_wait(&lock);

    /* call function to handle the requests from client */
    (void) service_client_socket(tcb_p->client, ssl, printable);
    sem_post(&lock);

    /* cleanup */
    SSL_free(ssl);
    close(tcb_p->client);
    free(printable);        /* this was strdup'd */
    free(data);            /* this was malloc'd */
    pthread_exit(0);
}

/* function for receiving incoming connections from
   clients and serving them in separate threads for
   faster processing */
int service_listen_socket_multithread(const int s) {


    init_locks();
    /* database lock for locking a database when
       there is a possibility to corrupt data */
    if (pthread_mutex_init(&mut, NULL) != 0) {
        printf("mutex init failed\n");
        return 1;
    }

    /* we use a semaphore to ensure that no more threads are
       serving requests because of limited amount of file
       descriptors allowed to be opened on linux systems */
    if (sem_init(&lock, 0, 50) != 0) {
        printf("mutex init failed\n");
        return 1;
    }

    /* initialise ssl and create it's context.
       create one server context per-hos,
       set the cert and private key of that context,
       and then share that context between all downstream
       SSL* sockets that act on behalf of that host. */
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

            /* we now create a thread and start it by calling
               client_thread with the tcb_p pointer.  That data was malloc'd,
               so is safe to use.  If we just have a structure on our own
               stack (ie, a variable local to this function) and passed a
               pointer to that, then there would be some exciting race
               conditions as it was destroyed (or not) before the created
               thread finished using it. */

            /* create separate thread for processing */
            if (pthread_create(&thread, 0, &client_thread, (void *) tcb_p) != 0) {
                perror("pthread_create");
                goto error_exit; /* avoid break here in of the later
				   addition of an enclosing loop */
            }

            /* we set the thread to be of detached state in order not to leak
               the memory because by default joinable thread will not be cleaned
               up until it is joined */
            int error = pthread_detach(thread);
            if (error != 0) {
                fprintf(stderr, "pthread_detach failed %s, continuing\n", strerror(error));
            }

        }
    }

    /* cleanup */
    error_exit:
    kill_locks();
    sem_destroy(&lock);
    cleanup_openssl();
    SSL_CTX_free(ctx);
    return -1;
}
