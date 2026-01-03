#pragma once
#include "noncopyable.h"
#include <functional>
#include <thread>
#include <memory>
#include <unistd.h>
#include <atomic>
#include <string>
class Thread : noncopyable
{
public:
    using ThreadFunc = std::function<void()>;
    explicit Thread(ThreadFunc func, const std::string &name = std::string()); // 调用string（）生成默认的字符串
    ~Thread();

    void start();
    void join_();

    bool started() const { return started_; }
    pid_t tid() const { return tid_; }
    const std::string &name() const { return name_; }

    static int numCreated() { return numCreated_; }

private:
    void setDefaultName();//给线程设置默认的名称
    bool started_;
    // 调用 join() 的线程会被阻塞，直到目标线程跑完
    bool joined_; // 等待线程执行结束，然后再继续下面的代码
    std::shared_ptr<std::thread> thread_;//thread_成员
    pid_t tid_;
    ThreadFunc func_;
    std::string name_;
    static std::atomic<int32_t> numCreated_; // 创建线程得数量
};