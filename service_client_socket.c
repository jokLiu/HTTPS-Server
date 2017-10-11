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

#include "service_client_socket.h"

/* why can I not use const size_t here? */
#define buffer_size 10240
void bad_request(const int s, char *path);
int readHeaders(const int sock, char ***strings_ptr);
void freeMemory(char **str, int s);
int content_size(char **strings, int s);
int find_content_length(char **strings, int s);


int service_client_socket (const int s, const char *const tag) {
  // allocating array of pointers to the strings
  int buffSize = 20;
  char** strings = malloc(buffSize * sizeof(char*));
  char *token;
  char path[1000];
  char* pwd = getenv("PWD");
  strcpy(path, pwd);
  FILE *file;
  char *buffer;
  // memset((void*)buffer, '\0', buffer_size);
  size_t bytes2;
  char buff[buffer_size];
  buff[buffer_size-1] = '\0';
  char str[100];
  char *temp_buff;
  printf ("new connection from %s\n", tag);

  int amount =0;
  while ((amount=readHeaders(s,&strings)) > 0 /*(amount=read (s, buffer, 1)) > 0*/) {
      buffer = strings[0];
      token =  strtok_r(buffer, " \t\n", &temp_buff );


      if ( strncmp(token, "GET\0", 4)==0 ){
          char* filepath = strtok_r(NULL, " \t\n",&temp_buff);
          token = strtok_r(NULL, " \t\n", &temp_buff);

          if (strncmp(token, "HTTP/1.1", 8) == 0) {
              /*if(strncmp(filepath, "/", 1) == 0)  strcpy(path+strlen(pwd), "/index.html");
              else */ strcpy(path+strlen(pwd), filepath);
              if( (file=fopen(path, "r"))){

                  fseek(file, 0L, SEEK_END);
                  int size = ftell(file);
                  fseek(file, 0L, SEEK_SET);

                  sprintf(str, "Content-Length: %d\n", size);
                  write (s, "HTTP/1.1 200 OK\n", 16);
                  // write (s, "Date: Mon, 27 Jul 2009 12:28:53 GMT\n", 36);
                  write (s, str, strlen(str));
                  write (s, "Content-Type: text/html\n", 24);
                  write (s, "Connection: Closed\n\n", 20);
                  while  ((bytes2=fread(buff, 1, buffer_size-1, file))>0 ) {
                      write(s, buff, bytes2);
                  }
                  fclose(file);
              }
              else {
                  strcpy(path+strlen(pwd), "/notfound.html");
                  file=fopen(path, "r");

                  fseek(file, 0L, SEEK_END);
                  int size = ftell(file);
                  fseek(file, 0L, SEEK_SET);

                  sprintf(str, "Content-Length: %d\n", size);
                  write(s, "HTTP/1.1 404 Not Found\n", 23);
                  // write (s, "Date: Mon, 27 Jul 2009 12:28:53 GMT\n", 36);
                  write (s, str, strlen(str));
                  write (s, "Content-Type: text/html\n", 24);
                  write (s, "Connection: Closed\n\n", 20);
                  printf("%d", size);
                  while  ((bytes2=fread(buff, 1, buffer_size-1, file))>0 ) {
                      write(s, buff, bytes2);
                  }
                  fclose(file);

              }
          } else {
              strcpy(path+strlen(pwd), "/badrequest.html");
              bad_request(s, path);
          }
      }
      else if ( strncmp(token, "HEAD\0", 5)==0 ){
          char* filepath = strtok_r(NULL, " \t\n", &temp_buff);
          token = strtok_r(NULL, " \t\n", &temp_buff);

          if (strncmp(token, "HTTP/1.1", 8) == 0) {

              /*if(strncmp(filepath, "/", 1) == 0)  strcpy(path+strlen(pwd), "/index.html");
              else */ strcpy(path+strlen(pwd), filepath);

              if( (file=fopen(path, "r"))){

                  fseek(file, 0L, SEEK_END);
                  int size = ftell(file);
                  fseek(file, 0L, SEEK_SET);

                  sprintf(str, "Content-Length: %d\n", size);
                  write (s, "HTTP/1.1 200 OK\n", 16);
                  // write (s, "Date: Mon, 27 Jul 2009 12:28:53 GMT\n", 36);
                  write (s, str, strlen(str));
                  write (s, "Content-Type: text/html\n", 24);
                  write (s, "Connection: Closed\n\n", 20);
                  fclose(file);
              }
              else
                  write(s, "HTTP/1.1 404 Not Found\n\n", 24);
          } else {
              strcpy(path+strlen(pwd), "/badrequest.html");
              bad_request(s, path);
          }
      }
      else if ( strncmp(token, "OPTIONS\0", 5)==0 ){
          char* filepath = strtok_r(NULL, " \t\n", &temp_buff);
          token = strtok_r(NULL, " \t\n", &temp_buff);

          if (strncmp(token, "HTTP/1.1", 8) == 0) {

              if( strncmp(filepath, "*", 1) == 0 || strncmp(filepath, "/", 1) == 0){

                  write (s, "HTTP/1.1 200 OK\n", 16);
                  // write (s, "Date: Mon, 27 Jul 2009 12:28:53 GMT\n", 36);
                  write (s, "Allow: GET,HEAD,POST,OPTIONS,TRACE\n",35);
                  write (s, "Content-Length: 0\n", 18);
                  write (s, "Connection: Closed\n\n", 20);
              }
              else
                  write(s, "HTTP/1.1 404 Not Found\n\n", 24);
          } else {
              strcpy(path+strlen(pwd), "/badrequest.html");
              bad_request(s, path);
          }
      }

      else if ( strncmp(token, "TRACE\0", 6)==0 ){
          strtok_r(NULL, " \t\n", &temp_buff);
          token = strtok_r(NULL, " \t\n", &temp_buff);

          if (strncmp(token, "HTTP/1.1", 8) == 0) {
                  sprintf(str, "Content-Length: %d\n", content_size(strings, amount)+amount*2+2);
                  write (s, "HTTP/1.1 200 OK\n", 16);
                  // write (s, "Date: Mon, 27 Jul 2009 12:28:53 GMT\n", 36);
                  write (s, str, strlen(str));
                  write (s, "Connection: Closed\n\n", 20);
              for(int i =0; i<s; i++){
                  write(s, strings[i], strlen(strings[i]));
                  write(s, "\r\n", 2);
              }
              write(s, "\r\n", 2);

          } else {
              strcpy(path+strlen(pwd), "/badrequest.html");
              bad_request(s, path);
          }
      }  else if( strncmp(token, "POST\0", 5)==0 ) {
          strtok_r(NULL, " \t\n", &temp_buff);
          token = strtok_r(NULL, " \t\n", &temp_buff);

          if (strncmp(token, "HTTP/1.1", 8) == 0) {
              int length = find_content_length(strings, amount);
              if(length <= buffer_size) {
                  char *content = malloc(sizeof(char)*(length+1));
                  content[length] = '\0';
                  read(s, content, length);
                  strcpy(path+strlen(pwd), "/");
                  strcpy(path+strlen(pwd)+1, tag);
                  strcpy(path+strlen(pwd)+1+strlen(tag), ".txt");
                  FILE *file = fopen(path ,"a");
                  if(file) {
                      fwrite(content, 1, strlen(content), file);
                  }
                  fclose(file);
              } else {
                  // TODO write file too large
              }

          } else {
              strcpy(path+strlen(pwd), "/badrequest.html");
              bad_request(s, path);
          }
      }

      // setting other memory to 0 for other fetchings
      memset(path+strlen(pwd),'\0', 1000-+strlen(pwd));

  }

  printf ("connection from %s closed\n", tag);
  close (s);
  return 0;
}

void bad_request(const int s, char *path){

    // open the file for reading
    FILE *file = fopen(path, "r");

    // check the size of the file
    fseek(file, 0L, SEEK_END);
    int size = ftell(file);
    fseek(file, 0L, SEEK_SET);

    // allocate the right amount of the size
    char *str = malloc(sizeof(char)*(size+1));
    char *buffer = malloc(buffer_size*sizeof(char));
    *(str+size) = '\0';

    // write the key information to the client
    sprintf(str, "Content-Length: %d\n", size);
    write(s, "HTTP/1.1 400 Bad Request\n",25  );
    write (s, str, strlen(str));
    write (s, "Connection: Closed\n\n", 20);


    // reading from the file and sending the content to the client
    int bytes;
    while  ((bytes=fread(buffer, 1, buffer_size-1, file))>0 ) {
        write(s, buffer, bytes);
    }

    // cleanup
    fclose(file);
    free(buffer);
    free(str);
}

int content_size(char **strings, int s){
    int size = 0;
    for(int i=0; i<s;i++){
        size+=strlen(strings[i]);
    }
    return size;
}


int readHeaders(const int sock, char ***strings_ptr) {
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
                    freeMemory(str, s);
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
                freeMemory(tempBuff, s);
                return -1;
            }
        }

        // if the string is empty, we reached the end, so terminate
        if(check < 0 || strncmp(buffer, "\r\n\r\n", 4) == 0 || strncmp(buffer, "\n\n", 2) == 0 ) {
            break;
        } else {
            buffer[i] = '\0';
            // add the string to the string array, and increment the string counter
            str[s] = buffer;
            s++;
        }
    }
    out:
    // free the single char
    free(c);

    *strings_ptr = str;

    // return the size of the array
    return s;

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

// freeMemory free's all the strings
void freeMemory(char **str, int s) {

    for(int i=0; i<s; i++){
        free(str[i]);
    }
    free(str);
}