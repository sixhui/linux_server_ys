#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>
#include <iostream>
using namespace std;

#define     BACKLOG     3
#define     oops(msg)     {perror(msg); exit(1);}

static const char* status_line[2] = {"200 OK", "500 Internal server error"};

int main(int argc, char const *argv[])
{
    int         ip          = INADDR_ANY;
    int         port        = 12345;
    int         listenfd, connfd;
    sockaddr_in serv_addr;

    const char* file_name   = "/root/phonebook.data";

    // socket
    if((listenfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) oops("fail socket");

    // bind
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family        = AF_INET;
    serv_addr.sin_addr.s_addr   = htonl(ip);
    serv_addr.sin_port          = htons(port);

    if(bind(listenfd, (sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) oops("fail bind");

    // listen
    if(listen(listenfd, BACKLOG) == -1) oops("fail listen");

    // accept
    if((connfd = accept(listenfd, 0, 0)) == -1) oops("fail accept");

    // file_buf
    char*       file_buf;
    struct stat file_stat;
    bool        valid       = true;                 // 目标文件是否有效

    if(stat(file_name, &file_stat) < 0){
        cout << "stat" << endl;
        valid = false;
    }
    else{
        if(S_ISDIR(file_stat.st_mode)){             // 文件类型
            cout << "isdir" << endl;
            valid = false;
        }
        else if(!(file_stat.st_mode & S_IROTH)){    // 文件权限
            cout << "irother" << endl;
            valid = false;
        }
        else{
            int fd      = open(file_name, O_RDONLY);
            file_buf    = new char[file_stat.st_size + 1];
            memset(file_buf, '\0', file_stat.st_size + 1);
            if(read(fd, file_buf, file_stat.st_size) < 0){
                cout << "read" << endl;
                valid = false;
            }
        }
    }

    // header_buf
    char    header_buf[BUFSIZ];
    memset(header_buf, '\0', BUFSIZ);
    int     len = 0;
    char    *p  = header_buf;
    if(valid){
        len = snprintf(p, BUFSIZ - 1, "%s %s\r\n", "HTTP/1.1", status_line[0]);
        p += len;
        len = snprintf(p, BUFSIZ - 1 - len, "Content-Length: %d\r\n", file_stat.st_size);
        p += len;
        snprintf(p, BUFSIZ - 1 - len, "%s", "\r\n");

        // cout << header_buf << endl;
        // cout << file_buf << endl;

        // writev
        struct iovec iv[2];
        iv[0].iov_base  = header_buf;
        iv[0].iov_len   = strlen(header_buf);
        iv[1].iov_base  = file_buf;
        iv[1].iov_len   = file_stat.st_size;
        writev(connfd, iv, 2);
    }
    else{
        len = snprintf(header_buf, BUFSIZ - 1, "%s %s\r\n", "HTTP/1.1", status_line[1]);
        p += len;
        snprintf(header_buf + len, BUFSIZ - 1 - len, "%s", "\r\n");
        send(connfd, header_buf, strlen(header_buf), 0);
    }
    close(connfd);
    delete[] file_buf;

    // close
    close(listenfd);
    
    return 0;
}
