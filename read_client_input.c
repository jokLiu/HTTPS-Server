
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


int read_client_input(const int sock, char ***strings_ptr) {
    int arrSize = 20;
    int buffSize = 20;
    char **str = *strings_ptr;

    // buffer for a single string
    char *buffer;

    // number of strings
    int s =0;

    // single char for reading input char by char
    char *c = malloc(sizeof(char));
    int check = read(sock, c, 1);

    // reading input until it is available

    while(check>0) {


        // keeping track of the number of chars in a singles string
        int i=0;
        buffSize = 30;


        int count_ends = 0;
        while(*c == '\n' || *c == '\r'){
            if(*c== '\n') count_ends++;
            if(count_ends == 2) {
                goto out;
            }
            read(sock, c, 1);
        }

        // allocating memory for a single string
        buffer = (char *) malloc (buffSize * sizeof(char));

        // if the character exists and it is not endline character
        // we add it to the string
        while(check >0 && *c != '\n' && *c != '\r'){

            if( buffSize-i <= 1 ){
                buffSize*=2;
                char *tempBuff = buffer;
                buffer = realloc(buffer, buffSize * sizeof(char*));
                if (buffer == NULL) {
                    free(c);
                    free(tempBuff);
                    free_memory(str, s);
                    return -1;
                }
            }

            buffer[i] = *c;
            i++;
            check = read(sock, c, 1);
        }


        if( arrSize-s <= 1 ){
            arrSize*=2;
            char **tempBuff = str;
            str = realloc(str,arrSize * sizeof(char*));
            if (str == NULL) {
                free(c);
                free(buffer);
                free_memory(tempBuff, s);
                return -1;
            }
        }


        buffer[i] = '\0';
        printf("%s\n", buffer);
        // add the string to the string array, and increment the string counter
        str[s] = buffer;
        s++;
    }
    out:
    // free the single char
    free(c);

    *strings_ptr = str;

    // return the size of the array
    return s;

}


void free_memory(char **str, int s) {

    for(int i=0; i<s; i++){
        free(str[i]);
    }
    free(str);
}