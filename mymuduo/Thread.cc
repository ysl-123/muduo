#include "Thread.h"
#include "mutex"
#include <semaphore.h>
#include "CurrentThread.h"
std::atomic<int32_t> Thread::numCreated_ {0};
Thread::Thread(ThreadFunc func, const std::string &name) : 
started_(false)
, joined_(false)
, tid_(0)
, func_(std::move(func))//这里将func给了func_准备在start中调用func_()即在里面实现回调
, name_(name)
{
    setDefaultName();
}
Thread::~Thread()
{
    if (started_ && !joined_)
    {
        // detach放他独立做任务，主线程不管
        // 就是thread类结束之后释放但是里面的thread_成员并不释放继续执行因为分离线程
        thread_->detach(); // thread类提供的设置分离线程的方法
    }
}
void Thread::start()//一个thread对象记录的就是一个新线程的详细信息
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);

    // 开启线程
    thread_ = std::shared_ptr<std::thread>(new std::thread([&]()
                                                           {
    // 获取线程的tid值
    tid_ = CurrentThread::tid();
    sem_post(&sem);
    // 开启一个新线程，专门执行该线程函数
    func_(); }));

    // 这里必须等待主线程获取到子线程的信号量就直接走了 获取上面新创建的线程的tid值
    sem_wait(&sem);
}
void Thread::join_(){
   joined_=true;
   thread_->join();
}



void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}
