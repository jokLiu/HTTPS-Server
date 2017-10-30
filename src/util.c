#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

/* counts the size of the content received
   the content is the array of strings
   returns the sum of all strings */
int content_size(char **strings, int array_size) {
    int size = 0;
    for (int i = 0; i < array_size; i++) {
        size += strlen(strings[i]);
    }
    return size;
}

/* parses the strings to retrieve the content length
   content length allows to allocate and read the required
   amount from the socket.*/
int find_content_length(char **strings, int s) {
    for (int i = 0; i < s; i++) {
        if (strncmp("Content-Length: ", strings[i], 16) == 0) {
            char *word = strings[i];
            return atoi(word + 16);
        }
    }
    return 0;
}

/* counts the file content size */
int count_file_length(FILE *file) {
    fseek(file, 0L, SEEK_END);
    int size = ftell(file);
    fseek(file, 0L, SEEK_SET);
    return size;
}


/* removes unnecessary lines from the POST request
   to make it well formatted for storing in the database */
char *remove_unnecessary_lines(char *content) {
    char *temp = content;
    char *prev = content;
    char *before_prev = content;

    /* we seek the \r characters to the end,
       and then return the second last chunk of data
       which is the actual content */
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
