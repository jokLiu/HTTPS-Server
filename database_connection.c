#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>
#include <string.h>
#include "database_connection.h"
#include "util.h"

#define pages (char*[14]){"versionnotsupported.html\0","badrequest.html\0", "begin.html\0","brum.html\0" , "end.html\0", "filetoolarge.html\0","index.html\0","notfound.html\0","past.html\0", "profile.html\0", "random.html\0", "uni.html\0", "upload.html\0", "internalerror.html\0"}

void do_exit(PGconn *conn) {
    PQfinish(conn);
}

PGconn* create_connection(){
    PGconn *conn = PQconnectdb("user=webserver password=webserver dbname=webserver");
    return conn;
}

PGresult* create_table(PGconn *conn){
    PGresult *res = PQexec(conn, "CREATE TABLE IF NOT EXISTS Records(Name VARCHAR(100) PRIMARY KEY," \
        " Data TEXT )");
    return res;
}

PGresult *populate_table(PGconn *conn, char *id, char *data){
    char* str = calloc((strlen(id)+ strlen(data)+ 91),sizeof(char));
    sprintf(str, "INSERT INTO Records VALUES( '%s' , '%s') ON CONFLICT " \
     "(Name) DO UPDATE SET Data = excluded.Data"
, id, data);
    PGresult *res = PQexec(conn, str);
    free(str);
    return res;
}


PGresult *query_table(PGconn *conn, char *id){
    char* str = calloc((strlen(id)+ 41),sizeof(char));

    sprintf(str, "SELECT Data FROM Records WHERE Name = '%s'", id);
    PGresult *res = PQexec(conn, str);
    printf("%s\n",PQresultErrorMessage(res));
    free(str);
    return res;
}

void load_files_content(PGconn *conn){
    char* path = calloc(100,sizeof(char));
    strcpy(path, "pages/");
    FILE *file;
    int size;
    char* content;


   // int bytes;
    PGresult *res;
    for(int i =0; i<12; i++){
        strcpy(path+6, pages[i]);
        file = fopen(path ,"r");
        if(!file) continue;

        size=count_file_length(file);
        content = calloc(size+1, sizeof(char));
        /*bytes=*/fread(content, 1, size, file);

        res = populate_table(conn, pages[i], content);
        clear_result(res);
        free(content);
    }

    free(path);
}

PGresult *delete_row(PGconn *conn, char *id){
    char* str = calloc((strlen(id)+ 37),sizeof(char));
    sprintf(str, "DELETE FROM Records WHERE Name = '%s'", id);
    PGresult *res = PQexec(conn, str);
    free(str);
    return res;
}

char *get_result_content(PGresult *res){
    return PQgetvalue(res, 0, 0);
}

void clear_result(PGresult *res){
    PQclear(res);
}

// return 1 for true and 0 for false
int check_exists(PGconn *conn, char *id){
    char* str = calloc((strlen(id)+ 53),sizeof(char));
    sprintf(str, "SELECT EXISTS(SELECT 1 FROM Records WHERE Name = '%s')", id);
    PGresult *res = PQexec(conn, str);
    char *value=PQgetvalue(res, 0, 0);
    printf("%s\n", value);
    int result = strncmp(value, "t", 1)==0 ? 1 : 0;
    clear_result(res);
    free(str);
    printf("%d\n", result);
    return result;
}