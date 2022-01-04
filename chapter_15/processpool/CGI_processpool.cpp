#include "processpool.h"

class cgi_conn{
public:
    cgi_conn(){}
    ~cgi_conn(){}
    /* 初始化客户连接 */
    void init(int epollfd, int sockfd, const sockaddr_in& clnt_addr){
        m_epollfd   = epollfd;
        m_sockfd    = sockfd;
        m_addr      = clnt_addr;
        memset(m_buf, '\0', sizeof(BUFSIZ));
        m_read_idx  = 0;
    }

    void process(){
        int idx     = 0;
        int n_read  = 0;
        int ret     = -1;// 可删？

        // 循环读取、分析客户数据
        while(true){
            idx     = m_read_idx;
            n_read  = recv(m_sockfd, m_buf + idx, BUFFER_SIZE - 1 - idx, 0);
            if(n_read < 0){
                if(errno == EAGAIN){                    // 暂时没有数据可读
                    break;
                }
                else{
                    removefd(m_epollfd, m_sockfd);      // 发生错误
                    break;
                }
            }
            else if(n_read == 0){                       // 对方关闭连接
                removefd(m_epollfd, m_sockfd);
                break;
            }
            else{                                       // 读数据
                m_read_idx += n_read;
                printf("user content: %s\n", m_buf);
                for(; idx < m_read_idx; ++idx){
                    if((idx >= 1) && (m_buf[idx - 1] == '\r') && (m_buf[idx] == '\n')) break;
                }
                printf("idx %d - m_read_idx %d\n", idx, m_read_idx);
                // 没有遇到 \r\n
                if(idx == m_read_idx) continue;
                
                // 遇到了 \r\n
                m_buf[idx - 1] = '\0';
                char* file_name = m_buf; // 可删？
                // 判断客户要运行的 CGI 程序是否存在
                if(access(file_name, F_OK) == -1){
                    removefd(m_epollfd, m_sockfd);
                    break;
                }
                ret = fork();
                if(ret == -1){
                    removefd(m_epollfd, m_sockfd);
                    break;
                }
                else if(ret > 0){                       // 父进程
                    removefd(m_epollfd, m_sockfd);      // 父进程close sockfd，子进程还没关；父进程负责从epoll移除sockfd
                    break;
                }
                else{                                   // 子进程
                    close(STDOUT_FILENO);               // 可 dup2？
                    dup(m_sockfd);
                    execl(m_buf, m_buf, 0);
                    exit(0);
                }
            }
        }
    }


private:
    static const int    BUFFER_SIZE = 1024;
    static int          m_epollfd;
    int                 m_sockfd;
    sockaddr_in         m_addr;
    char                m_buf[BUFFER_SIZE];
    int                 m_read_idx;
};

int cgi_conn::m_epollfd = -1;

#define         BACKLOG         5

int main(int argc, char const *argv[])
{
    const int   ip              = INADDR_ANY;
    const int   port            = 12345;
    int         listenfd;
    sockaddr_in serv_addr;

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

    // process pool
    processpool<cgi_conn>* pool = processpool<cgi_conn>::create(listenfd);

    if(pool){
        pool->run();
        delete pool;        // ?
    }
    
    close(listenfd);
    return 0;
}
