#include "EPollPoller.h"
#include <errno.h>
#include "Channel.h"
#include "Logger.h"
#include "string.h"
#include <unistd.h>

const int kNew = -1;    // 新的Channel，还未添加到epoll中
const int kAdded = 1;   // 已经添加到epoll中，处于监听状态
const int kDeleted = 2; // 曾经添加过，但被删除了（EPOLL_CTL_DEL执行过），现在需要重新监听，这个状态是只是在epoll中删除，但是channelmap中没有删除

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop),
      //::epoll_create1(EPOLL_CLOEXEC)的最后返回结果就是一个epoolfd
      
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)), 
      events_(kInitEventListSize) // 别人都是直接赋值，数组反而就是直接变成自己是几个了
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    // 实际上应该用LOG_DEBUG输出日志更为合理
    LOG_INFO("func=%s => fd total count:%lu\n",__FUNCTION__, channels_.size());
    //别疑惑为什么会有epollfd在哪里弄的，eventloop里初始化的时候有poller，
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now()); // int x(5);   // x = 5，直接初始化类似

    if (numEvents > 0)
    {
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);//如果考虑的是baseloop的监听，现在里面的都是listenfd
        if (numEvents == events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0)
    {   
        //这里不就是我们运行起服务器后如何一直不客户端连接就会一直每隔30秒发一起吗
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }
    else //=-1
    {
        if (saveErrno != EINTR) // 被迫中断所以要重新输入
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}
// channel的updatechannel和removechannel通过调用eventloop 的updatechanenl和removechannel 然后调用poller的那两个，具体实现则是到了epollpoller的
void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index(); // index就代表当前的epollpoller中的三个chanenl状态
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);

    if (index == kNew || index == kDeleted)
    {
        if (index == kNew)
        {
            int fd = channel->fd();
            // 这里需要注意是添加的时候加入到channelmap中方便快速查，但是删除的时候并不需要从channelmap中删除
            channels_[fd] = channel; // 就将fd和channel都存进去了channelmap里
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else // channel 已经在poller上注册过了
    {
        int fd = channel->fd();
        if (channel->isNoneEvent()) // channel已经对任何事件不感兴趣，所以考虑后面操作将他删除
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel); // 修改这个要监听的这个channel的事件
        }
    }
}
void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    channels_.erase(fd);
    LOG_INFO("func=%s => fd=%d \n", __FUNCTION__, fd);
    int index = channel->index();
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

// 填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (int i = 0; i < numEvents; ++i)
    {
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
        int fd = channel->fd();  // 获取当前事件对应的fd
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
        
        // 新增日志：打印事件对应的fd和事件类型
        LOG_INFO("Active event: fd=%d, events=0x%x (EPOLLIN=%d, EPOLLOUT=%d, EPOLLHUP=%d)",
                 fd,
                 events_[i].events,
                 (events_[i].events & EPOLLIN) ? 1 : 0,
                 (events_[i].events & EPOLLOUT) ? 1 : 0,
                 (events_[i].events & EPOLLHUP) ? 1 : 0);
    }
}
// 更新channel通道
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    memset(&event, 0, sizeof event);
    int fd = channel->fd();

    event.events = channel->events();
    event.data.fd = fd; 
    event.data.ptr = channel;
    
    // operation就是EPOLL_CTL_ADD这种类似的
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
}