#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include "database_connection.h"
#include "util.h"


/* close the database connection */
void do_exit(PGconn *db_connection) {
    PQfinish(db_connection);
}

/* create a database connection to the webserver */
PGconn *create_connection() {

    PGconn *db_connection = PQconnectdb("user=jxl706 "\
            "password=cisuphek dbname=dbteach2");
    return db_connection;
}

/* creating table if one is not already present from previous runs */
PGresult *create_table(PGconn *db_connection) {
    PGresult *res = PQexec(db_connection,
        "CREATE TABLE IF NOT EXISTS Records "\
        "(Name VARCHAR(100) PRIMARY KEY, Data TEXT )");
    return res;
}


/* populate table inserts data into the table or
   updates the current record if one already exists with the specific name */
PGresult *populate_table(PGconn *db_connection,char *id, char *data) {

    /* we escape a string before putting it into a table
       to avoid SQL injection attacks */
    char *escaped_str = PQescapeLiteral(db_connection, data, strlen(data));

    /* prepare string for storing the insert query */
    char *str = calloc((strlen(id) + strlen(escaped_str) + 91), sizeof(char));
    sprintf(str, "INSERT INTO Records VALUES( '%s' , %s ) ON CONFLICT " \
     "(Name) DO UPDATE SET Data = excluded.Data", id, escaped_str);

    /* lock exclusive access to variable isExecuted */
    pthread_mutex_lock(&mut);

    /* execute the query */
    PGresult *res = PQexec(db_connection, str);

    /* release the lock */
    pthread_mutex_unlock(&mut);

    /* cleanup */
    free(str);
    PQfreemem(escaped_str);
    return res;
}

/* querying the table to retrieve the data based on the key */
PGresult *query_table(PGconn *db_connection,char *id) {
    /* prepare string for storing the insert query */
    char *str = calloc((strlen(id) + 41), sizeof(char));
    sprintf(str, "SELECT Data FROM Records WHERE Name = '%s'", id);

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

/* loading files to the database when server is started*/
void load_files_content(PGconn *db_connection) {

    /* specifying the path where the files are located */
    char *path = calloc(100, sizeof(char));
    strcpy(path, "pages/");

    /* for holding a single file during the iteration */
    FILE *file;

    /* content size of the file */
    int size;

    /* content of a single file */
    char *content;

    /* database result */
    PGresult *res;

    /* directory where files are located */
    DIR *dir;

    /* struct which holds a single entry in a directory */
    struct dirent *ent;

    /* open a directory */
    if ((dir = opendir("pages/")) != NULL) {
        /* read all the files within directory
           and store their content into the db table. */
        while ((ent = readdir (dir)) != NULL) {

            /* path to the file */
            strcpy(path + 6,ent->d_name);

            /* open the file */
            file = fopen(path, "r");

            /* if file failed to open or file is either '.' or '..' skip */
            if (!file || !strncmp(ent->d_name, ".",1)) continue;

            /* get the size of the file */
            size = count_file_length(file);

            /* read the content of the file */
            content = calloc(size + 1, sizeof(char));
            fread(content, 1, size, file);

            /* insert the content to the database table */
            res = populate_table(db_connection,ent->d_name, content);

            /* cleanup */
            clear_result(res);
            free(content);
        }
        closedir (dir);
    } else {
        /* could not open directory */
        perror("");
    }

    /* cleanup */
    free(path);
}

/* deleting the row from records when it is no longer necessary
   in order to keep it clean.
   it is usually called when the client exists to remove
   any data that was stored by the client. */
PGresult *delete_row(PGconn *db_connection,char *id) {
    /* prepare string for storing the insert query */
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
    if(PQresultStatus(res) == PGRES_TUPLES_OK) return PQgetvalue(res, 0, 0);
    return NULL;
}

/* clearing the result */
void clear_result(PGresult *res) {
    PQclear(res);
}

/* checking whether the table contains certain record
   returns 1 for true and 0 for false */
int check_exists(PGconn *db_connection, char *id) {
    /* prepare string for storing the exists query */
    char *str = calloc((strlen(id) + 53), sizeof(char));
    sprintf(str, "SELECT EXISTS(SELECT 1 FROM Records WHERE Name = '%s')", id);

    /* lock exclusive access to variable isExecuted */
    pthread_mutex_lock(&mut);

    /* execute the query */
    PGresult *res = PQexec(db_connection, str);
    /* release the lock */
    pthread_mutex_unlock(&mut);

    /* get the value of the result */
    char *value = NULL;
    if(PQresultStatus(res) == PGRES_TUPLES_OK)
        value = PQgetvalue(res, 0, 0);

    /* translating the result to an int */
    int result=0;
    if(value)
        result = strncmp(value, "t", 1) == 0 ? 1 : 0;

    /* cleanup */
    clear_result(res);
    free(str);

    return result;
}