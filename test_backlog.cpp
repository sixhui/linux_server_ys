#include <sys/socket.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>


static bool stop = false;
static void handle_term(int sig){ // SIGTERM 信号的处理函数
    stop = true;
}

int main(int argc, char const *argv[])
{
    signal(SIGTERM, handle_term);

    if(argc < 4){
        printf("usage: %s ip port backlog\n", basename(argv[0]));
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);
    int backlog = atoi(argv[3]);

    // 1 创建 socket
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    printf("hello\n");
    // 2 bind
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    char* ip_str = inet_ntoa(address.sin_addr);
    printf("%s\n", ip_str);
    // inet_pton(AF_INET, ip, &address.sin_addr);
    printf("end\n");

    int res = bind(sock, (struct sockaddr*) &address, sizeof(address));
    assert(res != -1);

    // listen
    res = listen(sock, backlog);
    assert(res != -1);

    while(!stop){
        sleep(1);
    }

    // close
    close(sock);

    return 0;
}
