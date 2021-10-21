#include <arpa/inet.h>
#include <stdlib.h>
#include <iostream>
using namespace std;
#include <unistd.h>
#include <assert.h>
#include <string.h>

int main(int argc, char const *argv[])
{
    assert(argc == 1);

    const char* ip = "";
    int port = 12345;
    int res;

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    // socket
    int sid = socket(PF_INET, SOCK_STREAM, 0);
    assert(sid >= 0);

    // connect
    res = connect(sid, (struct sockaddr*) &addr, sizeof(addr));
    assert(res >= 0);

    // send
    const char* oob_data = "abc";
    const char* nom_data = "123";
    send(sid, nom_data, strlen(nom_data), 0);
    send(sid, oob_data, strlen(oob_data), MSG_OOB);
    send(sid, nom_data, strlen(nom_data), 0);
    
    // close
    close(sid);
    
    
    return 0;
}
