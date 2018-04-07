/*
Ali Itani

A web Server for pictures, music, and videos

I also tried to handle directory requests for some reason i dont know 
the video works on firefox but doesnt work on chrome??
*/


#include "ant_server.h"

void ant_readinitb(ant_t *ant, int fd){
    ant->ant_fd = fd;
    ant->ant_client = 0;
    ant->ant_bufptr = ant->ant_buf;
}

ssize_t writen(int fd, void *usrbuf, size_t n){
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = usrbuf;

    while (nleft > 0){
        if ((nwritten = write(fd, bufp, nleft)) <= 0){
            if (errno == EINTR){
                nwritten = 0;
              } else {
                return -1;
              }
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
    return n;
}

static ssize_t ant_read(ant_t *ant, char *usrbuf, size_t n){
    int client;
    while (ant->ant_client <= 0){

        ant->ant_client = read(ant->ant_fd, ant->ant_buf, sizeof(ant->ant_buf));
        if (ant->ant_client < 0){
            if (errno != EINTR) {
                return -1;
              }
        }else if (ant->ant_client == 0) {
            return 0;
        }else {
            ant->ant_bufptr = ant->ant_buf;
          }
    }


    client = n;
    if (ant->ant_client < n) {
        client = ant->ant_client;
      }
    memcpy(usrbuf, ant->ant_bufptr, client);
    ant->ant_bufptr += client;
    ant->ant_client -= client;
    return client;
}


ssize_t ant_readlineb(ant_t *ant, void *usrbuf, size_t maxlen){
    int n, rc;
    char c, *bufp = usrbuf;

    for (n = 1; n < maxlen; n++){
        if ((rc = ant_read(ant, &c, 1)) == 1){
            *bufp++ = c;
            if (c == '\n') {
                break;
              }
        } else if (rc == 0){
            if (n == 1) {
                return 0;
              }
            else {
                break;
              }
        } else {
            return -1;
          }
    }
    *bufp = 0;
    return n;
}

void format_size(char* buf, struct stat *stat){
    if(S_ISDIR(stat->st_mode)){
        sprintf(buf, "%s", "[DIRECTORY]");
    } else {
        off_t size = stat->st_size;
	unsigned long i = 1024;
        if(size < i){
            sprintf(buf, "%lu", size);
        } else if (size < i * i){
            sprintf(buf, "%.1fK", (double) size / 1024);
        } else if (size < i * i * i){
            sprintf(buf, "%.1fM", (double) size / 1024 / 1024);
        } else if(size < i * i * i * i) {
            sprintf(buf, "%.1fG", (double) size / 1024 / 1024 / 1024);
        } else {
            sprintf(buf, "%.1fT", (double) size / 1024 / 1024 / 1024 / 1024);
        }
    }
}

void handle_directory_request(int out_fd, int dir_fd, char *filename){
    char buf[MAXLINE];
    char size[16];
    struct stat statbuf;
    sprintf(buf, "HTTP/1.2 200 OK\r\n%s%s%s%s%s",
            "Content-Type: text/html\r\n\r\n",
            "<html lang='en'><head><style>",
            "body{font-family: monospace; font-size: 13px;}",
            "td {padding: 1.5px 6px;}",
            "</style></head><body><h1>Hello World, this is Antecedent Server!</h1><br><hr><br>\r\n<table>\r\n");
    writen(out_fd, buf, strlen(buf));
    DIR *d = fdopendir(dir_fd);
    struct dirent *dp;
    int ffd;
    while ((dp = readdir(d)) != NULL){
        if(!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")){
            continue;
        }
        if ((ffd = openat(dir_fd, dp->d_name, O_RDONLY)) == -1){
            perror(dp->d_name);
            continue;
        }

        fstat(ffd, &statbuf);
        format_size(size, &statbuf);

        if(S_ISREG(statbuf.st_mode) || S_ISDIR(statbuf.st_mode)){
            char *d = S_ISDIR(statbuf.st_mode) ? "/" : "";

            sprintf(buf, "<tr><td><a href=\"%s%s\">%s%s</a></td><td>%-10s</td><td>%s</td></tr>\n", dp->d_name, d, dp->d_name, d, " ", size);

            writen(out_fd, buf, strlen(buf));
        }
        close(ffd);
    }
    sprintf(buf, "</table></body></html>");
    writen(out_fd, buf, strlen(buf));
    closedir(d);
}

static const char* get_mime_type(char *filename){
    char *dot = strrchr(filename, '.');
    if(dot){
        mime_map *map = types;
        while(map->extension){
            if(strcmp(map->extension, dot) == 0){
                return map->mime_type;
            }
            map++;
        }
    }
    return default_mime_type;
}


void url_decode(char* src, char* dest, int max) {
    char *p = src;
    char code[3] = { 0 };
    while(*p && --max) {
        if(*p == '%') {
            memcpy(code, ++p, 2);
            *dest++ = (char)strtoul(code, NULL, 16);
            p += 2;
        } else {
            *dest++ = *p++;
        }
    }
    *dest = '\0';
}


void parse_request(int fd, http_request *req){
    ant_t ant;
    char buf[MAXLINE];
    char method[MAXLINE];
    char uri[MAXLINE];

    req->off_set = 0;
    req->end = 0;

    ant_readinitb(&ant, fd);
    ant_readlineb(&ant, buf, MAXLINE);
    sscanf(buf, "%s %s", method, uri);

    while(buf[0] != '\n' && buf[1] != '\n') {
        ant_readlineb(&ant, buf, MAXLINE);

        if(buf[0] == 'R' && buf[1] == 'a' && buf[2] == 'n'){
            sscanf(buf, "Range: bytes=%ld-%lu", &req->off_set, &req->end);
            if( req->end != 0) req->end ++;
        }

    }
    char* filename = uri;
    if(uri[0] == '/'){
        filename = uri + 1;
        int length = strlen(filename);
        if (length == 0){
            filename = ".";
        } else {
            for (int i = 0; i < length; ++ i) {
                if (filename[i] == '?') {
                    filename[i] = '\0';
                    break;
                }
            }
        }
    }
    url_decode(filename, req->file_name, MAXLINE);
}


void log_access(int status, struct sockaddr_in *c_addr, http_request *req){
    printf("Client:%s:%d\r\nStatus: %d\r\nFile: %s\r\n", inet_ntoa(c_addr->sin_addr), ntohs(c_addr->sin_port), status, req->file_name);
}

void send_error(int fd, int status, char *msg, char *longmsg){
    char buf[MAXLINE];
    sprintf(buf, "HTTP/1.2 %d %s\r\n", status, msg);
    sprintf(buf + strlen(buf), "Content-length: %lu\r\n\r\n", strlen(longmsg));
    sprintf(buf + strlen(buf), "%s", longmsg);

    printf("HTTP/1.2 %d %s\r\n", status, msg);
    printf("Content-length: %lu\r\n\r\n", strlen(longmsg));
    printf("%s\n", longmsg);

    writen(fd, buf, strlen(buf));
}

void serve_static(int out_fd, int in_fd, http_request *req, size_t total_size){

    char buf[256];
    if (req->off_set > 0){
        sprintf(buf, "HTTP/1.2 206 Partial\r\n");
        sprintf(buf + strlen(buf), "Content-Range: bytes %lu-%lu/%lu\r\n", req->off_set, req->end, total_size);
        printf("HTTP/1.2 206 Partial\r\n");
        printf("Content-Range: bytes %lu-%lu/%lu\r\n", req->off_set, req->end, total_size);
    } else {
        sprintf(buf, "HTTP/1.2 200 OK\r\nAccept-Ranges: bytes\r\n");
        printf("HTTP/1.2 200 OK\r\nAccept-Ranges: bytes\r\n");
    }
    sprintf(buf + strlen(buf), "Content-length: %lu\r\n", req->end - req->off_set);
    sprintf(buf + strlen(buf), "Content-type: %s\r\n\r\n", get_mime_type(req->file_name));

    printf("Content-length: %lu\r\n", req->end - req->off_set);
    printf("Content-type: %s\r\n\r\n", get_mime_type(req->file_name));

    writen(out_fd, buf, strlen(buf));
    off_t offset = req->off_set;

    while(offset < req->end){
        if(sendfile(out_fd, in_fd, &offset, req->end - req->off_set) <= 0) {
            break;
        }
        // printf("offset: %ld\r\n\r\n", offset); not needed
        close(out_fd);
        break;
    }
}

void process(int fd, struct sockaddr_in *clientaddr){

    http_request req;
    parse_request(fd, &req);

    struct stat sbuf;
    int status = 200;
    int ffd = open(req.file_name, O_RDONLY, 0);

    if(ffd <= 0){
        status = 404;
        char *msg = "File not found\r\n";
        log_access(status, clientaddr, &req);
        send_error(fd, status, "Not found\r\n", msg);
    } else {
        fstat(ffd, &sbuf);
        if(S_ISREG(sbuf.st_mode)){
            if (req.end == 0){
                req.end = sbuf.st_size;
            }
            if (req.off_set > 0){
                status = 206;
            }
            log_access(status, clientaddr, &req);
            serve_static(fd, ffd, &req, sbuf.st_size);
        } else if(S_ISDIR(sbuf.st_mode)){
            status = 200;
            log_access(status, clientaddr, &req);
            handle_directory_request(fd, ffd, req.file_name);
        } else {
            status = 400;
            char *msg = "Unknown Error";
            log_access(status, clientaddr, &req);
            send_error(fd, status, "Error", msg);
        }
        close(ffd);
    }
}

int main(int argc, char** argv){
    struct sockaddr_in clientaddr;
    int port;
    int listenfd;
    int connfd;

    if(argv[1] != NULL) { // assumming user will put a valid argv[1]
        port = atoi(argv[1]);
    } else {
      printf("Usage: ./ant_server [port-number]\r\n");
      exit(1);
    }

    socklen_t clientlen = sizeof clientaddr;

    int optval = 1;

    struct sockaddr_in serveraddr;
    // set up socket

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
      }

    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,(const void *)&optval , sizeof(int)) < 0){
        return -1;
    }

    if (setsockopt(listenfd, 6, TCP_CORK, (const void *)&optval , sizeof(int)) < 0) {
        return -1;
    }

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);

    if (bind(listenfd, (sock_adder *)&serveraddr, sizeof(serveraddr)) < 0) {
        return -1;
    }

    if (listen(listenfd, LISTENQ) < 0) {
        return -1;
    }

    // check if my socket works
    if (listenfd > 0) {
        printf("Server is connected to port %d\r\n", port);
    } else {
        perror("ANT_ERROR");
        exit(listenfd);
    }

    signal(SIGPIPE, SIG_IGN);
    // threads here
    while(1){
        connfd = accept(listenfd, (sock_adder *)&clientaddr, &clientlen); // wait for a client
        process(connfd, &clientaddr); // finish user parse_request
        close(connfd); // leave and loop back
    }

    return 0;
}
