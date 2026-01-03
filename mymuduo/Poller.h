#pragma once
#include "noncopyable.h"
#include <vector>
#include <unordered_map>
#include "Timestamp.h"
class Channel;
class EventLoop;
// muduo库中多路事件分发器的核心io复用模块
class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel *>;

    Poller(EventLoop *loop);
    virtual ~Poller()=default;
    // 给所有IO复用保留统一的接口

    //启动epoll
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
   
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    // 判断参数channel是否在当前Poller当中
    bool hasChannel(Channel *channel) const;
    /*
    Poller（基类）
├─ PollPoller（使用 poll 实现）
└─ EPollPoller（使用 epoll 实现）

    */
    //现在poller类实际上就是想使用epoll或者poll，所以下面写那种类似工厂模式的实现，然后后面可以根据具体情况选择用哪个
    //获取到eventloop中默认的一个poller，就像单例中获取一个具体的实例
    static Poller *newDefaultPoller(EventLoop *loop);

protected:
//map的key：sockfd    value：sockfd所属的channel通道类型
    using ChannelMap = std::unordered_map<int, Channel *>;
    ChannelMap channels_;
    
private:
    EventLoop *ownerLoop_; // 定义Poller所属的事件循环EventLoop的对象
};
