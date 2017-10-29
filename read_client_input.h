void free_memory(char **str, int s);

int read_client_input(SSL *ssl, char ***strings_ptr);

void free_memory_inside(char **str, int s);

void free_main_string(char **str);