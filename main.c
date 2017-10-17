#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <assert.h>
#include <libpq-fe.h>
#include "get_listen_socket.h"
#include "service_listen_socket_multithread.h"
#include "database_connection.h"

char *myname = "unknown";

int
main(int argc, char **argv) {
    int p, s;
    char *endp;
    /* we are passed our name, and the name is non-null.  Just give up if this isn't true */

    assert(argv[0] && *argv[0]);
    myname = argv[0];

    printf("%s", "yye");
    if (argc != 2) {
        fprintf(stderr, "%s: usage is %s port\n", myname, myname);
        exit(1);
    }
    /* same check: this should always be true */
    assert(argv[1] && *argv[1]);

    /* convert a string to a number, endp gets a pointer to the last character converted */
    p = strtol(argv[1], &endp, 10);
    if (*endp != '\0') {
        fprintf(stderr, "%s: %s is not a number\n", myname, argv[1]);
        exit(1);
    }

    /* less than 1024 you need to be root, >65535 isn't valid */
    if (p < 1024 || p > 65535) {
        fprintf(stderr, "%s: %d should be in range 1024..65535 to be usable\n",
                myname, p);
        exit(1);
    }

    s = get_listen_socket(p);
    if (s < 0) {
        fprintf(stderr, "%s: cannot get listen socket\n", myname);
        exit(1);
    }


    create_connection();
    if (PQstatus(db_connection) == CONNECTION_BAD) {

        fprintf(stderr, "Connection to database failed: %s\n",
                PQerrorMessage(db_connection));
        do_exit();
        exit(1);
    }


    PGresult *res = create_table();
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "%s: cannot create a table\n", myname);
        do_exit();
        exit(1);
    }
    clear_result(res);

    load_files_content();
    if (service_listen_socket_multithread(s) != 0) {
        fprintf(stderr, "%s: cannot process listen socket\n", myname);
        exit(1);
    }

    exit(0);
}
    
  
   
