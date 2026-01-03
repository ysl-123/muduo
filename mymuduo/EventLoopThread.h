#pragma once
#include "noncopyable.h"
#include <functional>
#include <functional>
#include <mutex>
#include <condition_variable>
#include "Thread.h"
#include "EventLoop.h"
#include <string>

// 就是绑定了一个thread和一个loop，就是在thread里面创建一个loop
class EventLoopThread : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;
    //ThreadInitCallback() 生成一个空的 std::function 对象
    //ThreadInitCallback &cb相当于 void (*callback)(EventLoop *)()
    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const std::string &name =std::string());

    ~EventLoopThread();

    EventLoop *startLoop();
  
private:
    void threadFunc();
    EventLoop *loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};