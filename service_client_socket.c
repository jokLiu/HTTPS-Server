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
#include <libpq-fe.h>
#include "service_client_socket.h"
#include "read_client_input.h"
#define buffer_size 10240


void bad_request(const int s, char *path);
int content_size(char **strings, int s);
int find_content_length(char **strings, int s);
int count_file_length(FILE *file);

int service_client_socket (const int s, const char *const tag, PGconn *conn) {
  // allocating array of pointers to the strings
  int buffSize = 20;
  char** strings = malloc(buffSize * sizeof(char*));
  char path[1000];
  char* pwd = getenv("PWD");
  strcpy(path, pwd);
  FILE *file;
  char *buffer;
  size_t bytes2;
  char buff[buffer_size];
  buff[buffer_size-1] = '\0';
  char str[100];
  char* temp_buff;

  // tokens keeping the request header info
  char* filepath;
  char* request_method;
  char* http_type;
  printf ("new connection from %s\n", tag);

  int amount =0;
  while ((amount=read_client_input(s,&strings)) > 0) {
      buffer = strings[0];
      request_method =  strtok_r(buffer, " \t\n", &temp_buff );
      filepath = strtok_r(NULL, " \t\n",&temp_buff);
      http_type = strtok_r(NULL, " \t\n", &temp_buff);

      // if we get incorrect http request, respond to the client
      // and discard the request
      if (strncmp(http_type, "HTTP/1.1", 8) != 0) {
          strcpy(path+strlen(pwd), "/badrequest.html");
          bad_request(s, path);
          continue;
      }


      if ( strncmp(request_method, "GET\0", 4)==0 ){
          strcpy(path+strlen(pwd), filepath);
              if( (file=fopen(path, "r"))){

                  int size = count_file_length(file);
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

                  int size = count_file_length(file);

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
      }
      else if ( strncmp(request_method, "HEAD\0", 5)==0 ){

              strcpy(path+strlen(pwd), filepath);

              if( (file=fopen(path, "r"))){

                  int size = count_file_length(file);

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
      }
      else if ( strncmp(request_method, "OPTIONS\0", 5)==0 ){


              if( strncmp(filepath, "*", 1) == 0 || strncmp(filepath, "/", 1) == 0){

                  write (s, "HTTP/1.1 200 OK\n", 16);
                  // write (s, "Date: Mon, 27 Jul 2009 12:28:53 GMT\n", 36);
                  write (s, "Allow: GET,HEAD,POST,OPTIONS,TRACE\n",35);
                  write (s, "Content-Length: 0\n", 18);
                  write (s, "Connection: Closed\n\n", 20);
              }
              else
                  write(s, "HTTP/1.1 404 Not Found\n\n", 24);
      }

      else if ( strncmp(request_method, "TRACE\0", 6)==0 ){

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

      }  else if( strncmp(request_method, "POST\0", 5)==0 ) {

              int length = find_content_length(strings, amount);
              if(length <= buffer_size) {
                  char *content = malloc(sizeof(char)*(length+1));
                  content[length] = '\0';
                  read(s, content, length);
                  printf("%s\n", content);
                  strcpy(path+strlen(pwd), "/");
                  strcpy(path+strlen(pwd)+1, tag);
                  strcpy(path+strlen(pwd)+1+strlen(tag), ".txt");
                  FILE *file = fopen(path ,"w");
                  if(file) {
                      fwrite(content, 1, strlen(content), file);
                  }
                  fclose(file);

                  file = fopen(path ,"r");
                  int size = count_file_length(file);

                  strcpy(path+strlen(pwd), "/begin.html\0");
                  FILE *file1 = fopen(path ,"r");
                  size+=count_file_length(file1);

                  strcpy(path+strlen(pwd), "/past.html\0");
                  FILE *file2 = fopen(path ,"r");
                  size+=count_file_length(file2);



                  sprintf(str, "Content-Length: %d\n", size);
                  write (s, "HTTP/1.1 200 OK\n", 16);
                  // write (s, "Date: Mon, 27 Jul 2009 12:28:53 GMT\n", 36);
                  write (s, str, strlen(str));
                  write (s, "Content-Type: text/html\n", 24);
                  write (s, "Connection: Closed\n\n", 20);

                  bytes2=fread(buff, 1, buffer_size-1, file1);
                  write(s, buff, bytes2);
                  bytes2=fread(buff, 1, buffer_size-1, file);
                  write(s, buff, bytes2);
                  bytes2=fread(buff, 1, buffer_size-1, file2);
                  write(s, buff, bytes2);

                  fclose(file);
                  fclose(file2);
                  fclose(file1);
              } else {
                  // TODO write file too large
              }

      }

      // setting other memory to 0 for other fetchings
      memset(path+strlen(pwd),'\0', 1000-+strlen(pwd));

  }

  free_memory(strings, amount);
  printf ("connection from %s closed\n", tag);
  close (s);
  return 0;
}


void service_get_request(const int s, char *path){

}

void bad_request(const int s, char *path){

    // open the file for reading
    FILE *file = fopen(path, "r");

    // check the size of the file
    int size = count_file_length(file);

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

