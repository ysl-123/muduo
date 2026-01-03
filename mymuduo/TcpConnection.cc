#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include <functional>
#include "errno.h"
#include <string>
#include "unistd.h"
// 检查传进来的loop是否为空
static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d tcpconnection Loop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}
TcpConnection::TcpConnection(EventLoop *loop,
                             const std::string &nameArg,
                             int sockfd,
                             const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop)), name_(nameArg), state_(kConnecting), 
    reading_(true), socket_(new Socket(sockfd)), channel_(new Channel(loop, sockfd)),
     localAddr_(localAddr), peerAddr_(peerAddr), highWaterMark_(64 * 1024 * 1024) // 64M
{
    // poller给channel通知的感兴趣的事件发生了，channel就会发生相应的回调
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n", name_.c_str(), channel_->fd(), (int)state_);
}

void TcpConnection::send(const std::string buf)
{
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        { // c_str()返回的是某个指针
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {/*理论上只会是subloop线程的subloop的poll监听到读事件，发个客户端，不会进入else，但是比如
            聊天服务器，从map找到一方的conn，然后conn->send()就直接可以了，但是当前的tcpconnection里的conn根本不是不是当前loop线程的，要进入自己的线程去完成
            
            */
            loop_->runInLoop(std::bind(
                &TcpConnection::sendInLoop,
                this,
                buf.c_str(),
                buf.size()));
        }
    }
}
void TcpConnection::sendInLoop(const void *data, size_t len)
{
    ssize_t nwrite = 0;
    size_t remaining = len; // 剩下要写的长度
    bool faultError = false;
    // 之前调用过该connection的shutdown，不能再进行发送了
    if (state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing!");
        return;
    }
    // channel第一次开始写数据，并且缓冲区里没有待发送的数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrite = ::write(channel_->fd(), data, len);
        if (nwrite >= 0)
        {
            remaining = len - nwrite;
            if (remaining == 0 && writeCompleteCallback_)
            {
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else
        {
            nwrite = 0;
            if (errno != EWOULDBLOCK) // 资源暂时不可用
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                // — 管道断裂 对方已经关闭连接   对端强制重置了连接
                if (errno == EPIPE || errno == ECONNRESET) // SIGPIPE RESET
                {
                    faultError = true;
                }
            }
        }
    }
    // 说明当前这一次write，并没有把数据全部发送出去，剩余的数据需要保存到缓冲区当中，然后给channel
    // 注册epollout事件，poller发现tcp的发送缓冲区有空间，会通知相应的sock-channel，调用writeCallback_回调方法
    // 也就是调用TcpConnection::handleWrite方法，把发送缓冲区中的数据全部发送完成
    if (!faultError && remaining > 0)
    {
        // 目前发送缓冲区剩余的待发送数据的长度  因为你可能之前也往里面放了呀，所以肯定这一次又放的话也不能占用之前放的呀
        size_t oldLen = outputBuffer_.readableBytes();
        // highWaterMark_(64 * 1024 * 1024)  
        //高水位（highWaterMark） 的核心作用是流量控制与内存保护，用于避免输出缓冲区，但我没绑定所以不用执行
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_)
        {
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append((char *)data + nwrite, remaining);
        if (!channel_->isWriting())
        {
            channel_->enableWriting();
        }
    }
}
void TcpConnection::shutdown()
{
    if (state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting()) // 说明outputBuffer中的数据已经全部发送完成
    {
        socket_->shutdownWrite(); // 关闭写端，自动触发EPOLLHUP事件
    }
}

// 连接建立
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this()); // 将chanenl和tcpconnection绑定
    channel_->enableReading();         // 向poller注册channel的epollin事件
    // 调用连接回调（用户设置的onConnection）
    connectionCallback_(shared_from_this());
}
// 连接销毁
void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll(); // 把channel的所有感兴趣的事件，从poller中del掉
        connectionCallback_(shared_from_this());
    }
    channel_->remove(); // 把channel从poller中删除掉
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0)
    {
        // 已建立连接的用户，有可读事件发生了，调用用户传入的回调操作onMessage
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0) // 关闭
    {
        handleClose();
    }
    else
    {
        errno = savedErrno;
        LOG_ERROR("Tcpconnection:handleRead");
        handleError();
    }
}
void TcpConnection::handleWrite()
{
    if (channel_->isWriting())
    {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if (n > 0)
        {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if (writeCompleteCallback_)
                {
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this()));
                }
            }
            if (state_ == kDisconnected)
            {
                shutdownInLoop();
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing \n", channel_->fd());
    }
}
void TcpConnection::handleClose()
{
    LOG_INFO("fd=%d state=%d \n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();
    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr); // 执行连接关闭的回调
    closeCallback_(connPtr);      // 关闭连接的回调
}
void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof(optval);
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}
