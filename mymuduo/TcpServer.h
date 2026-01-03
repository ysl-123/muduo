#pragma once

/**
 * 用户使用muduo编写服务器程序
 */
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include <functional>
#include <string>
#include <memory>
#include "Callbacks.h"
#include <atomic>
#include <unordered_map>
#include "TcpConnection.h"
#include "Buffer.h"


// 对外的服务器编程使用的类
class TcpServer : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    enum Option {
    kNoReusePort, // 不启用 SO_REUSEPORT，默认方式,只能有一个线程/进程监听端口。

    //实现轮询算法的关键，实际上这个只是内核管理多个线程连接同一个端口，任然时一个一个上只不过内核肯定比线程阻塞等待快
    kReusePort    // 启用 SO_REUSEPORT 允许多个 SubLoop（线程）绑定到同一端口，从而让内核分配连接，提高并发性能。
};


    TcpServer(EventLoop *loop,
              const InetAddress &listenAddr,
               const std::string &nameArg,//一般就当作服务器名字
              Option option = kNoReusePort);
    ~TcpServer();

    void setThreadInitCallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }
    // 设置底层subloop的个数
    void setThreadNum(int numThreads);

    // 开启服务器监听
    void start();

private:
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
    EventLoop *loop_; // baseLoop 用户定义的loop
    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_; // 服务器监听socket的封装类

    std::shared_ptr<EventLoopThreadPool> threadPool_; // one loop per thread
    ConnectionCallback connectionCallback_;           // 有新连接的时候的回调
    MessageCallback messageCallback_;                 // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_;     // 消息发送完成之后的回调
    ThreadInitCallback threadInitCallback_;           // 线程初始化的回调
    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_; // 保存所有的连接
};