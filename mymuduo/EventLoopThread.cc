#include "EventLoopThread.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
                                 const std::string &name)
    : loop_(nullptr), exiting_(false)
      // 先执行thread_的构造函数然后执行bind，这个threadfunc就是起到后面子线程的处理就是那个start
      ,
      thread_(std::bind(&EventLoopThread::threadFunc, this), name), mutex_(), cond_() // 条件变量
      ,
      callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();
        thread_.join_();
    }
}

// 最重要的串联好多的线程的一个
EventLoop* EventLoopThread::startLoop() {
    thread_.start();           // 启动子线程，执行 threadFunc()
    
    {  //如果互斥锁mutex_被其他占用，那么当前eventloopthread线程就被阻塞
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop_ == nullptr) {   // 记住这个格式就行，防止虚假唤醒
            cond_.wait(lock);       // 阻塞在这里
        }
    }
    return loop_; // 子线程创建完成后返回有效的 EventLoop*
}


// 也非常重要串联一起了
// 就是回调的bind里传的函数，又因为回调的执行是在start的new thread(worker)即在worker中该执行的，也就是在新线程中执行
void EventLoopThread::threadFunc()
{
    // 创建一个独立的eventloop，和上面的线程是一一对应的即是 one loop per thread
    EventLoop loop;//创建了eventloop的空参构造
    if (callback_)
    {
        //一般来说这个回调一般初始化loop
        callback_(&loop); // 应该才是开始回调指向处理其中一个eventloop
    }
    {   //锁的作用好像就是只是为了让子线程先执行完，然后得到subloop，然后给主线程吧
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }
    //调用eventloop中的loop函数
    loop.loop();//EventLoop loop =>poller.poll
    std::unique_lock<std::mutex> lock(mutex_);//离开作用域会自动释放
    loop_=nullptr;//我不清楚为什么这句话一定在 return loop_; 之后执行  就是说loop.loop出问题了一会结束了
}