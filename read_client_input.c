
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include <memory.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <assert.h>
#include "read_client_input.h"

/* reading input coming from the client via
 * the socket provided until input is available
 * or the end of the header is reached */
int read_client_input(const int sock, char ***strings_ptr) {
    /* initial size of the string array */
    int arr_size = 20;

    /* initial size of the single line */
    int buff_size = 30;

    char **str = *strings_ptr;

    /* buffer for the single line input */
    char *buffer;

    /* number of lines read */
    int strings_size = 0;

    /* single character for reading input char by char */
    char *c = malloc(sizeof(char));

    /* reading single character from socket */
    int check = read(sock, c, 1);

    /* reading input until it is available
     * or end of the header sis reached */
    while (check > 0) {

        /* keeping track of the number of chars in a singles string */
        int i = 0;

        /* reinitialise default buffer size */
        buff_size = 30;

        /* checking whether the end of the header is reached
         * i.e. \r\n\r\n can be found
         * if so return */
        int count_ends = 0;
        while (*c == '\n' || *c == '\r') {
            if (*c == '\n') count_ends++;
            if (count_ends == 2) {
                goto out;
            }
            read(sock, c, 1);
        }

        /* allocating memory for a single string */
        buffer = (char *) malloc(buff_size * sizeof(char));

        /* if the character exists and it is not endline character
         * we add it to the string */
        while (check > 0 && *c != '\n' && *c != '\r') {

            /* we check if the current buffer size is not too small
             * and is not going to be overfilled by the next char
             * if so we reallocate it */
            if (buff_size - i <= 1) {
                buff_size *= 2;
                char *tempBuff = buffer;
                buffer = realloc(buffer, buff_size * sizeof(char *));
                if (buffer == NULL) {
                    free(c);
                    free(tempBuff);
                    free_memory(str, strings_size);
                    return -1;
                }
            }

            /* adding the character to buffer */
            buffer[i] = *c;
            i++;
            check = read(sock, c, 1);
        }

        /* we check if the current strings size  is not too small
        * and is not going to be overfilled by the next string
        * if so we reallocate it */
        if (arr_size - s <= 1) {
            arr_size *= 2;
            char **tempBuff = str;
            str = realloc(str, arr_size * sizeof(char *));
            if (str == NULL) {
                free(c);
                free(buffer);
                free_memory(tempBuff, strings_size);
                return -1;
            }
        }

        /* adding terminating byte */
        buffer[i] = '\0';
        printf("%s\n", buffer);
        /* add the string to the string array
         * and increment the string counter */
        str[s] = buffer;
        strings_size++;
    }
    out:

    /* cleanup */
    free(c);

    /* reassigning string */
    *strings_ptr = str;

    /* return the size of the array */
    return strings_size;

}


/* free memory of strings inside the array string */
void free_memory_inside(char **str, int s) {
    for (int i = 0; i < s; i++) {
        free(str[i]);
    }
}

/* free all the memory allocated */
void free_memory(char **str, int s) {
    free_memory_inside(str, s);
    free(str);
}