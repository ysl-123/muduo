#include "Poller.h"
#include <stdlib.h>
#include "EPollPoller.h"
//静态类外实现的方法
Poller* Poller::newDefaultPoller(EventLoop *loop)
{
    if (::getenv("MUDUO_USE_POLL"))
    {
        return nullptr; // 假如环境变量MUDUO_USE_POLL存在 生成poll的实例
    }
    else
    {
        return new EPollPoller(loop); // 生成epoll的实例
    }
}