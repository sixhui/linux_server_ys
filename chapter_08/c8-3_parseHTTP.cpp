#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#define     BUF_SIZE    4096
#define     oops(msg)   {perror(msg); exit(1);}

// 主状态机 状态转移 - 请求的解析状态：当前正在分析请求行；当前正在分析请求头
enum        CHECK_STATE {CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER};

// 从状态机 - 行的读取状态：完整的行；行出错；行不完整
enum        LINE_STATE  {LINE_OK = 0, LINE_BAD, LINE_OPEN};

// 服务器处理 HTTP 请求的结果：
// 请求不完整，需要继续读取客户数据；获得了完整的客户请求；
// 客户请求有语法错误；客户对资源没有足够的访问权限；服务器内部错误；客户端关闭连接
enum        HTTP_CODE   {NO_REQUEST, GET_REQUEST, BAD_REQUEST, 
FORBIDDEN_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION};

const char* szret[]     = {"I get a correct result\n", "something wrong\n"};

HTTP_CODE   parse_content(char* buf, int& checked_index, CHECK_STATE& check_state, int& read_index, int& line_index);
LINE_STATE  parse_line(char* buf, int& checked_index, int& read_index);
HTTP_CODE   parse_requestline(char* temp, CHECK_STATE& check_state);
HTTP_CODE   parse_headers(char* temp);

int main(int argc, char const *argv[])
{
    int                 port = 12345;
    int                 sockfd, clnt_fd, backlog = 3;
    struct sockaddr_in  addr, clnt_addr;
    socklen_t           clnt_addr_len;
    char                buf[BUF_SIZE];


    // socket
    if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) oops("fail socket");

    // bind
    bzero(&addr, sizeof(addr));
    addr.sin_family     = PF_INET;
    addr.sin_addr.s_addr= htonl(INADDR_ANY);
    addr.sin_port       = htons(port); 

    if(bind(sockfd, (struct sockaddr*) &addr, sizeof(addr)) == -1) oops("fail bind");

    // listen
    if(listen(sockfd, backlog) == -1) oops("");

    // accept
    if((clnt_fd = accept(sockfd, (struct sockaddr*) &clnt_addr, &clnt_addr_len)) == -1) oops("fail accept")

    memset(buf, '\0', BUF_SIZE);
    int         n_read       = 0;       // 单次读取的长度
    int         read_index      = 0;    // buf 当前存储数据指针
    int         checked_index   = 0;    // 已分析完的数据指针
    int         line_index      = 0;    // 行在 buf 中的起始位置
    CHECK_STATE check_state      = CHECK_STATE_REQUESTLINE;

    while(true){
        if((n_read = recv(clnt_fd, buf + read_index, BUF_SIZE - read_index, 0)) == -1) oops("fail read");
        if(n_read == 0){
            printf("remote client has closed the connection\n");
            break;
        }
        read_index += n_read;

        // 分析 HTTP 请求 - ！！！主要逻辑: 
        HTTP_CODE result = parse_content(buf, checked_index, check_state, read_index, line_index);
        
        // 解析请求结果，决定下一步
        if(result == NO_REQUEST){       // 不完整
            continue;
        }
        else if(result == GET_REQUEST){ // 完整
            send(clnt_fd, szret[0], strlen(szret[0]), 0);
        }
        else{                           // 其他
            send(clnt_fd, szret[1], strlen(szret[1]), 0);
        }
        
        close(clnt_fd);
    }
    
    close(sockfd);
    return 0;
}

/* 分析 HTTP 请求的入口函数 */
HTTP_CODE   parse_content(char* buf, int& checked_index, CHECK_STATE& check_state, int& read_index, int& line_index){
    LINE_STATE  line_state  = LINE_OK;      // 当前行的读取状态
    HTTP_CODE   res_code    = NO_REQUEST;   // HTTP 请求的处理结果
    
    // 从状态机 - 分离出一个行
    while((line_state = parse_line(buf, checked_index, read_index)) == LINE_OK){
        char*   temp        = buf + line_index;
        line_index          = checked_index;

        /* 主状态机 - check_state 维护状态 */
        switch(check_state){
            case CHECK_STATE_REQUESTLINE:{
                return parse_requestline(temp, check_state);
                break;
            }
            case CHECK_STATE_HEADER:{
                return parse_headers(temp);
                break;
            }
            default:
                return INTERNAL_ERROR;
        }
    }

    // 如果没有分离出完整行
    if(line_state == LINE_OPEN){
        return NO_REQUEST;
    }
    
    return BAD_REQUEST;
}

/* 从状态机，分离出一行：[checked_index, read_index) 划定了数据的首尾；检查 '\r\n' */
LINE_STATE  parse_line(char* buf, int& checked_index, int& read_index){
    char cur_char;
    for(; checked_index < read_index; ++checked_index){
        cur_char = buf[checked_index];
        if(cur_char == '\r'){           // buf[checked_index] == '\r' && buf[checked_index + 1] == '\n'
            if((checked_index + 1) == read_index) return LINE_OPEN;
            if(buf[checked_index + 1] == '\n'){
                buf[checked_index++] = '\0';
                buf[checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(cur_char == '\n'){      // buf[checked_index - 1] == '\r' && buf[checked_index] == '\n'
            if(checked_index > 1 && buf[checked_index - 1] == '\r'){
                buf[checked_index++] = '\0';
                buf[checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }

    return LINE_OPEN;
}