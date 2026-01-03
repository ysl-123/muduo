#pragma once
#include <vector>
#include <sys/epoll.h>
#include "Poller.h"
#include "Timestamp.h"
/**
 * epoll的使用
 * epoll_create
 * epoll_ctl    add/mod/del
 * epoll_wait
 */
class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop *loop);//相当于epoll_create  create的fd到时候记录在epollfd_
    ~EPollPoller() override;//就把epollfd  close掉

    //下面者连续三个都是重写poller的抽象方法
    // 启动poll   timeoutMs  io复用的超时时间
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;//相当于epoll_wait
    void updateChannel(Channel *channel) override;//epoll_ctl
    void removeChannel(Channel *channel) override;//epoll_ctl

private:
    //定义的是epoll_event的长度
    static const int kInitEventListSize = 16;
    //填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    //更新channel通道
    void update(int operation, Channel *channel);
    // epoll_event一个结构体记录一些东西
    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};