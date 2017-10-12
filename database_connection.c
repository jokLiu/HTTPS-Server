#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>
#include "database_connection.h"

void do_exit(PGconn *conn) {
    PQfinish(conn);
}

PGconn* create_connection(){
    PGconn *conn = PQconnectdb("user=webserver password=webserver dbname=webserver");
    return conn;
}
