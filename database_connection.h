

/* the lock */
pthread_mutex_t mut;

void do_exit(PGconn *db_connection);

PGconn *create_connection();

PGresult *create_table(PGconn *db_connection);

PGresult *populate_table(PGconn *db_connection, char *id, char *data);

PGresult *query_table(PGconn *db_connection, char *id);

char *get_result_content(PGresult *res);

void clear_result(PGresult *res);

void load_files_content(PGconn *db_connection);

PGresult *delete_row(PGconn *db_connection, char *id);

int check_exists(PGconn *db_connection, char *id);