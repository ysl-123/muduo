#include "EventLoop.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"
// 线程局部变量：每个线程有独立的t_loopInThisThread
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认的Poller IO复用接口的超时时间
const int kPollTimeMs = 10000;

// 创建wakeupfd，用来notify唤醒subreactor处理新来的channel
int createEventfd()
{
    /*
    0

初始计数值为 0（eventfd 内部的计数器初始为 0）。
EFD_NONBLOCK非阻塞模式，如果你去读 eventfd，但计数器为 0，不会阻塞，直接返回 -1 并设置 errno = EAGAIN。

EFD_CLOEXEC设置 close-on-exec，fork 后 exec 新程序时自动关闭这个 fd，防止泄露给子进程。

::eventfd(...)返回一个文件描述符（fd），你可以用 read() 和 write() 来操作计数器。
创建一个初始为 0 的事件计数器，非阻塞、exec 时自动关闭，返回一个 fd，用来线程/进程通知事件。
*/
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d \n", errno);
    }
    // 新增日志：记录 wakeupfd 的值
    LOG_INFO("wakeupfd created: fd=%d", evtfd);
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false), quit_(false), callingPendingFunctors_(false), threadId_(CurrentThread::tid()), 
    poller_(Poller::newDefaultPoller(this)), wakeupFd_(createEventfd()), 
    wakeupChannel_(new Channel(this, wakeupFd_)), currentActiveChannel_(nullptr)
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if (t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }
    // 设置wakeupfd的事件类型以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 每一个eventloop都将监听wakeupchannel的EPOLLIN读事件了
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping \n", this);

    while (!quit_)
    {
        activeChannels_.clear();
        // 监听clientfd  wakeupfd
        /*这里讨论下baseloop的epoll监听，上面有一个while循环，所以即使有多个客户端同时要建立连接，完全可以通过这个while
        poll多次实际上我们的epollfd都是水平触发的，所以没事
        */
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel *channel : activeChannels_)
        {
            // Poller监听哪些channel发生事件了，然后上报给EventLoop，通知channel处理相应的事件
            ////如果考虑的是baseloop的监听，现在channel就是listenfd，然后后面调用回调的时候只会是listenfd注册的handleread不会是connection的
            channel->handleEvent(pollReturnTime_);
        }
        // 执行当前的eventloop得回调操作
        /*
        io线程也就是mainreactor主要是accept连接操作，客户端返回fd，已连接fd要给subreactor
        wakeup subractor之后，通知mianloop注册的回调
        */
       doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping. \n", this);
    looping_ = false;
}

// 退出事件循环 1.loop在自己的线程中调用quit 2.在非loop的线程中，调用loop的quit
void EventLoop::quit()
{
    quit_ = true;

    // 如果是在其它线程中，调用的quit  在一个subloop(woker)中，调用了mainLoop(IO)的quit
    if (!isInLoopThread())
    {
        wakeup();
    }
}

// 在当前loop中执行cb
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) // 在当前的loop线程中，执行cb
    {
        cb();
    }
    else // 在非当前loop线程中执行cb，就需要唤醒loop所在线程，执行cb
    {
        queueInLoop(cb);
    }
}

//跨线程一般都是调用
// 把cb放入队列中，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);//类似于push_back不过是就地构造
    }

    // 唤醒相应的，需要执行上面回调操作的loop的线程了
    /*
    callingPendingFunctors_为true（当前 Loop 正在执行回调）此时EventLoop线程正在执行doPendingFunctors()，且已经将pendingFunctors_中的回调交换到局部变量functors中
    新加入的回调会存留在pendingFunctors_中，不会被当前正在执行的doPendingFunctors()处理（因为局部变量functors已经脱离队列
    若不唤醒，doPendingFunctors()执行完毕后，EventLoop会再次进入poller_->poll()阻塞，新回调会延迟到下一次poll触发。因此需要唤醒，让poll立即返回，进入下一次循环处理新回
    */
    if (!isInLoopThread() || callingPendingFunctors_) // 构造函数的时候为false
    {
        wakeup(); // 唤醒loop所在线程 
    }
}

// 用来唤醒loop所在的线程的 向wakeupFd_写一个数据，wakeupChannel就发生读事件，当前loop线程就会被唤醒
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
    }
}

void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}
void EventLoop::doPendingFunctors() // 执行回调
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (const Functor &functor : functors)
    {
        functor(); // 执行当前loop需要执行的回调操作
    }

    callingPendingFunctors_ = false;
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
{
    LOG_ERROR("EventLoop::handleRead() reads %zd bytes instead of %zd", n, sizeof one);
}

}
