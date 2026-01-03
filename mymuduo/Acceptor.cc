#include "Acceptor.h"
#include "Logger.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>  // sockaddr_in, htons
#include <netinet/tcp.h> // IPPROTO_TCP, TCP_NODELAY
#include <unistd.h>      // close
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "InetAddress.h"
static int createNonblocking()
{ // 创建一个 TCP 套接字（socket 文件描述符），并在创建时设置非阻塞和进程关闭时自动关闭的属性
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    // 新增日志：记录 listenfd 的值
    LOG_INFO("listenfd created: fd=%d", sockfd);
     return sockfd; //  添加返回值
};

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop), acceptSocket_(createNonblocking()) // 创建socket，也就是listenfd
      ,
      acceptChannel_(loop, acceptSocket_.fd()) // channel的构造函数
      ,
      listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);//理论上muduo库应该不开这个，因为只有一个baseloop一个listenfd

    acceptSocket_.bindAddress(listenAddr); // bind
    // TcpServer::start() Acceptor.listen 有新用户的连接, 要执行一个回调 (connfd=> channel=> subloop)
    // baseLoop => acceptChannel_(listenfd) =>
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this)); // 就是channel存回调
}
Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove(); 
}


void Acceptor::listen()
{
   listenning_=true;
   acceptSocket_.listen();
   //channel告诉poller我关心这个fd 的 可读事件（EPOLLIN / POLLIN）也就是
   //acceptChannel_(loop, acceptSocket_.fd())说明这个channel放进了baseloop和listenfd
   acceptChannel_.enableReading();//在acceptor中创建于listenfd也就是acceptChannel_设置为-1
}

void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);//将调用之后的connfd也就是clientfd的addr存进去
    if (connfd >= 0)
    {
        if (NewConnectionCallback_)
        {
             //这时已经接收到客户端的连接，得到了connfd，connfd封装在channel里，也就是现在这些channel要给subloop处理了
            //轮询找到subloop，唤醒，分发当前的新客户端的channel
            NewConnectionCallback_(connfd, peerAddr);
        }
        else
        {
            /* code */
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        if (errno == EMFILE)
        {
            LOG_ERROR("%s:%s:%d sockfd reached limit \n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}