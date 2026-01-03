#pragma once
#include <functional>
#include "noncopyable.h"
#include "Channel.h"
#include "Poller.h"
#include <vector>
#include "atomic"
#include "mutex"
#include "CurrentThread.h"
// 时间循环类 主要包含了两个大模块  channel poller（epoll的抽象）
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;
    EventLoop();
    ~EventLoop();
    void loop(); // 开启事件循环
    void quit(); // 退出事件循环

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    void runInLoop(Functor cb);   // 在当前loop中执行cb  cb就是回调
    void queueInLoop(Functor cb); // 把cb放入队列中，唤醒loop所在的线程，执行cb
    
    void wakeup(); // 用来唤醒loop所在的线程
    //eventloop的方法调用的就是poller的方法
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);
   //看当前的eventloop是不是在自己创建的线程里面如果在就true
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
   void handleRead();//主要用唤醒
  void doPendingFunctors(); // 执行回调;//执行回调
    using ChannelList = std::vector<Channel *>;

    // 实际作用类似于bool looping_
    //事件循环是否正在运行
    std::atomic_bool looping_; // 原子操作，通过cas实现的
    std::atomic_bool quit_;    // 标识退出loop循环

    const pid_t threadId_;     // 记录当前loop所在线程的id
    Timestamp pollReturnTime_; // poller返回发生事件的channels的时间点
    std::unique_ptr<Poller> poller_;
    int wakeupFd_; // 主要作用，当mainLoop获取一个新用户的channel，通过轮询算法选择一个subloop，通过该成员唤醒subloop处理channel
    //每一个subreacot监听了一个wakeupchannel，mainreactor可以通过给wakeupchannel发送消息，让subreactor接收到反应做事情
    std::unique_ptr<Channel> wakeupChannel_;//本质就是channel的一个指针而已呀
    ChannelList activeChannels_;
    Channel *currentActiveChannel_; // 只是用于断言

    std::atomic_bool callingPendingFunctors_; // 标识当前loop是否有需要执行的回调操作
    std::vector<Functor> pendingFunctors_;    // 存储loop需要执行的所有的回调操作
    std::mutex mutex_;                        // 互斥锁，用来保护上面vector容器的线程安全操作
};