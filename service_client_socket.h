int service_client_socket(const int s, const char *const tag);

void service_get_request(const int s, char *path);

void service_not_found(const int s, char *message, char *page);

void service_head_request(const int s, char *path);

void service_options_request(const int s, char *path);

void service_trace_request(const int s, char **strings, int amount);

void service_post_request(const int s, char *path, int length, char *tag);

void bad_request(const int s);