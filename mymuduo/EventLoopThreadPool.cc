#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"
#include <memory>
EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop), name_(nameArg), started_(false), numThreads_(0), next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
}

// ThreadInitCallback &cb相当于 void (*cb)(EventLoop *)()
void EventLoopThreadPool::start(const ThreadInitCallback &cb )
{
    started_ = true;
    for (int i = 0; i < numThreads_; i++)
    {
        char buf[name_.size() + 32];
        // c_str() 返回一个 const char*  符合 %s 的需求
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        EventLoopThread *t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(t->startLoop());
    }
    // 如果有的人根本没有设置setThreadNum，也就是说整个服务端只有一个线程，运行着baseloop
    if (numThreads_ == 0 && cb)
    {
        cb(baseLoop_); // 初始化回调可能就是对单线程设置要改一下
    }
}
// 如果工作在多线程中，baseLoop_也就是mainloop 默认以轮询的方式分配 channel 给 subloop
EventLoop *EventLoopThreadPool::getNextLoop()
{   
    EventLoop *loop =baseLoop_;
    if (!loops_.empty())//通过轮询获得下一个处理事件的loop
    {
        loop = loops_[next_];
        ++next_;
        if (next_ >= loops_.size())
        {
            next_ = 0;
        }
    }
    return loop;
}
std::vector<EventLoop *> EventLoopThreadPool::getAllLoops()
{
    if (loops_.empty())
    {
        return std::vector<EventLoop *>(1, baseLoop_);
    }
    else
    {
       return loops_;
    }
}