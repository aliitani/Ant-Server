#define _GNU_SOURCE
#include <arpa/inet.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>


#define LISTENQ  1024
#define MAXLINE 1024
#define ANT_BUFSIZE 1024

typedef struct {
    int ant_fd;
    int ant_client;
    char *ant_bufptr;
    char ant_buf[ANT_BUFSIZE];
} ant_t;

typedef struct sockaddr sock_adder;

typedef struct {
    char file_name[512];
    off_t off_set;
    size_t end;
} http_request;

typedef struct {
    const char *extension;
    const char *mime_type;
} mime_map;

mime_map types [] = {
    {".jpeg", "image/jpeg"},
    {".jpg", "image/jpeg"},
    {".ico", "image/x-icon"},
    {".mp4", "video/webm"},
    {".png", "image/png"},
    {".mp3", "audio/mp3"},
    {NULL, NULL},
};

char *default_mime_type = "text/plain";
