#include "Socket.h"
#include <unistd.h>
#include "Logger.h"
#include <sys/types.h>
#include <sys/socket.h>
#include "InetAddress.h"
#include <strings.h>
#include <netinet/tcp.h>   

Socket::~Socket()
{
    close(sockfd_);
}


void Socket::bindAddress(const InetAddress &localAddr)
{
    if (0 != ::bind(sockfd_, (sockaddr *)localAddr.getSockAddr(), sizeof(sockaddr_in)))
    {
        LOG_FATAL("bind sockfd:%d fail \n", sockfd_);
    }
}

void Socket::listen()
{
    if (0 != ::listen(sockfd_, 1024))//sockfd_就是上一步createNonblocking()得到的sockfd作为参数传给socket所以就有了sockfd_
    {
        LOG_FATAL("listen sockfd:%d fail \n", sockfd_);
    }
}
int Socket::accept(InetAddress *peerAddr)
{
    sockaddr_in addr;
    socklen_t len;
    bzero(&addr, sizeof addr);
    int connfd = ::accept(sockfd_, (sockaddr *)&addr, &len);//得到所谓的clientfd 和client的addr
    if (connfd >= 0)
    {
        peerAddr->setSockAddr(addr);//存进去的只是所谓的connfd的addr
    }
    return connfd;
}
void Socket::shutdownWrite()
{
    if(::shutdown(sockfd_,SHUT_WR)<0){
        LOG_ERROR("shutdownWrite error");
    }
}

// 设置 TCP_NODELAY，用于禁用/启用 Nagle 算法
// 让小包立刻发，不攒包，减少延迟
void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;  // 三目运算符，将布尔值转成 1/0
    //opt为1就是启用这个功能，0就是不启用
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval);
}

// 设置 SO_REUSEADDR，允许重用本地地址
// 服务器程序重启时，如果端口仍在 TIME_WAIT 状态，让你能马上再次监听同一个端口
void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}

// 设置 SO_REUSEPORT，允许多个进程/线程绑定同一个端口
// 常用于负载均衡，提高高并发服务器的性能
void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
}

// 设置 SO_KEEPALIVE，启用 TCP Keep-Alive
// 自动探测对端是否还活着，长连接常用
void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
}
