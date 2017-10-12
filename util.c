#include <stdio.h>
#include <stdlib.h>
#include "util.h"


int content_size(char **strings, int s){
    int size = 0;
    for(int i=0; i<s;i++){
        size+=strlen(strings[i]);
    }
    return size;
}


int find_content_length(char **strings, int s){
    for(int i=0; i<s; i++){
        if(strncmp("Content-Length: ", strings[i],16) == 0){
            char* word = strings[i];
            return atoi(word+16);
        }
    }
    return 0;
}


int count_file_length(FILE *file){
    fseek(file, 0L, SEEK_END);
    int size = ftell(file);
    fseek(file, 0L, SEEK_SET);
    return size;
}

