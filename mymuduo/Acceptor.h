#pragma once
#include "noncopyable.h"
#include "Channel.h"
#include "Socket.h"
#include <functional>
class EventLoop;
class InetAddresss;

class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback &cb)
    {
        NewConnectionCallback_ = cb;
    }

    bool listening() const { return listenning_; }
    void listen();

private:

    void handleRead();
    EventLoop *loop_; //  Acceptor用的就是用户定义的哪个baseLoop,也称作mainLoop
    Socket acceptSocket_;//起到的是bind之后监听的那个fd
    Channel acceptChannel_;//封装了监听 socket fd 的那个 Channel
    NewConnectionCallback NewConnectionCallback_;
    bool listenning_;
};