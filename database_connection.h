
PGconn *db_connection;

/* the lock */
pthread_mutex_t mut;


void do_exit();

void create_connection();

PGresult *create_table();

PGresult *populate_table(char *id, char *data);

PGresult *query_table(char *id);

char *get_result_content(PGresult *res);

void clear_result(PGresult *res);

void load_files_content();

PGresult *delete_row(char *id);

int check_exists(char *id);