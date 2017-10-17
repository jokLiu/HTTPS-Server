#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

/* counts the size of the content received */
int content_size(char **strings, int s) {
    int size = 0;
    for (int i = 0; i < s; i++) {
        size += strlen(strings[i]);
    }
    return size;
}

/* parses the strings to retrieve the content length */
int find_content_length(char **strings, int s) {
    for (int i = 0; i < s; i++) {
        if (strncmp("Content-Length: ", strings[i], 16) == 0) {
            char *word = strings[i];
            return atoi(word + 16);
        }
    }
    return 0;
}

/* counts the file size */
int count_file_length(FILE *file) {
    fseek(file, 0L, SEEK_END);
    int size = ftell(file);
    fseek(file, 0L, SEEK_SET);
    return size;
}


/* removes unncessary lines from the POST method
 * to make it well formated */
char *remove_unnecessary_lines(char *content) {
    char *temp = content;
    char *prev = content;
    char *before_prev = content;
    while (*content != '\0') {
        if (*content == '\r') {
            *content = '\0';
            before_prev = prev;
            prev = temp;
            temp = content + 1;
        }
        content++;
    }

    return before_prev;

}