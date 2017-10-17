#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>
#include <string.h>
#include <pthread.h>
#include "database_connection.h"
#include "util.h"

/* pages defines the web page files to be displayed for the client.
 * These web pages are loaded to the database when server is started. */
#define pages (char*[14]){"versionnotsupported.html\0",\
"badrequest.html\0", "begin.html\0","brum.html\0" , \
"end.html\0", "index.html\0",\
"notfound.html\0","past.html\0", "profile.html\0", \
 "random.html\0", "uni.html\0", "upload.html\0", \
"internalerror.html\0", "file_too_large.html\0"}


/* close the database connection */
void do_exit() {
    PQfinish(db_connection);
}

/* create a database connection to the webserver */
void create_connection() {
    db_connection = PQconnectdb("user=webserver "\
            "password=webserver dbname=webserver");
}

/* creating table if one is not already present from previous runs */
PGresult *create_table() {
    PGresult *res = PQexec(db_connection, "CREATE TABLE IF NOT EXISTS Records(Name VARCHAR(100) PRIMARY KEY," \
        " Data TEXT )");
    return res;
}


/* populate table inserts data into the table or
 * updates the current record if one already exists with the specific name */
PGresult *populate_table(char *id, char *data) {
    // TODO fix!!!!!!!
    char *escaped_str = PQescapeLiteral(db_connection, data, strlen(data));
    printf("%s\t%zu\n", escaped_str, strlen(escaped_str));
    char *str = calloc((strlen(id) + strlen(escaped_str) + 91), sizeof(char));

    sprintf(str, "INSERT INTO Records VALUES( '%s' , '%s' ) ON CONFLICT " \
     "(Name) DO UPDATE SET Data = excluded.Data", id, escaped_str);


    /* lock exclusive access to variable isExecuted */
    pthread_mutex_lock(&mut);

    /* execute the query */
    PGresult *res = PQexec(db_connection, str);

    // TODO remove
    printf("%s", PQresultErrorMessage(res));
    /* release the lock */
    pthread_mutex_unlock(&mut);

    /* cleanup */
    free(str);
    PQfreemem(escaped_str);
    return res;
}

/* querying the table to retrieve the data based on the key */
PGresult *query_table(char *id) {
    char *str = calloc((strlen(id) + 41), sizeof(char));

    sprintf(str, "SELECT Data FROM Records WHERE Name = '%s'", id);

    /* lock exclusive access to variable isExecuted */
    pthread_mutex_lock(&mut);

    /* execute the query */
    PGresult *res = PQexec(db_connection, str);

    /* release the lock */
    pthread_mutex_unlock(&mut);

    printf("%s\n", PQresultErrorMessage(res));

    /* cleanup */
    free(str);
    return res;
}

/* loading files to the database when server is started*/
void load_files_content() {

    /* specifying the path where the files are located */
    char *path = calloc(100, sizeof(char));
    strcpy(path, "pages/");
    FILE *file;
    int size;
    char *content;


    // TODO finish commenting and check errors
    // int bytes;
    PGresult *res;
    for (int i = 0; i < 14; i++) {
        strcpy(path + 6, pages[i]);
        file = fopen(path, "r");
        if (!file) continue;

        size = count_file_length(file);
        content = calloc(size + 1, sizeof(char));
        /*bytes=*/fread(content, 1, size, file);

        res = populate_table(pages[i], content);
        clear_result(res);
        free(content);
    }

    /* cleanup */
    free(path);
}

/* deleting the row from records when it is no longer necessary
 * in order to keep it clean */
PGresult *delete_row(char *id) {
    char *str = calloc((strlen(id) + 37), sizeof(char));
    sprintf(str, "DELETE FROM Records WHERE Name = '%s'", id);

    /* lock exclusive access to variable isExecuted */
    pthread_mutex_lock(&mut);

    /* execute the query */
    PGresult *res = PQexec(db_connection, str);

    /* release the lock */
    pthread_mutex_unlock(&mut);

    /* cleanup */
    free(str);
    return res;
}

/* get the content of the result */
char *get_result_content(PGresult *res) {
    return PQgetvalue(res, 0, 0);
}

/* clearing the result */
void clear_result(PGresult *res) {
    PQclear(res);
}

/* checking whether the table contains certain record
 * returns 1 for true and 0 for false */
int check_exists(char *id) {
    char *str = calloc((strlen(id) + 53), sizeof(char));
    sprintf(str, "SELECT EXISTS(SELECT 1 FROM Records WHERE Name = '%s')", id);

    /* lock exclusive access to variable isExecuted */
    pthread_mutex_lock(&mut);

    /* execute the query */
    PGresult *res = PQexec(db_connection, str);

    /* release the lock */
    pthread_mutex_unlock(&mut);

    /* get the value of the result */
    char *value = PQgetvalue(res, 0, 0);

    /* translating the result to an int */
    int result = strncmp(value, "t", 1) == 0 ? 1 : 0;

    /* cleanup */
    clear_result(res);
    free(str);

    return result;
}