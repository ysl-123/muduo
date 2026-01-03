#include "Channel.h"
#include "sys/epoll.h"
#include "EventLoop.h" 
//EPOLLIN  1        EPOLLPRI  2        EPOLLOUT  4     EPOLLIN | EPOLLPRI = 1 | 2 = 3
const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{
}

Channel::~Channel()
{
}
// channel 的tie方法什么时候调用过,一个tcpconnection新创建的时候，因为tcpconnnection绑定了一个channel
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}
/*
当改变channel所表示fd的events事件后，update（这是channel的函数）负责在poller里面更改fd相应的事件
eventloop包括channel和epoll
这就是为什么下面的channel要输入update，显然不可以，但是可以chanenl通过所属的eventloop调用poller相应方法
*/
void Channel::update()
{
    // 通过channel所属的EventLoop，调用poller的相应方法，注册fd的events事件
    // add code...
     loop_->updateChannel(this);
}

// 在channel所属的eventloop中，把当前的channel删除掉
void Channel::remove()
{
     loop_->removeChannel(this);
}
// fd得到poller通知以后，处理事件的
void Channel::handleEvent(Timestamp receiveTime)
{ /*
 
 tie_ 是 weak_ptr，lock() 会尝试 获取 shared_ptr：
 如果绑定对象还存在（引用计数 > 0），返回一个有效的 shared_ptr，
 引用计数 +1
 如果对象已经被销毁，返回一个空的 shared_ptr
 */
//判断这个 Channel 是否有绑定对象
 // 打印 tied_ 的值
    LOG_INFO("Channel::handleEvent - tied_: %d", tied_);
    if (tied_) 
    {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
}
// 根据poller通知的channel发生的具体事件，由channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("channel fd=%d handleEvent revents:%d\n", fd_, revents_);  // 增加 fd_ 打印//revents 在fillactivechannel写进去的
    // 这是属于异常断开的，这是内核自己会主动注册的
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_)
        {
            closeCallback_();
        }
    }
    if (revents_ & EPOLLERR)
    {
        errorCallback_();
    }
    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }
    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}
