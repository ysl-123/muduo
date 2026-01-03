#include "Poller.h"
#include "Channel.h"
// 在这就是将eventloop所属的对象记录下来
Poller::Poller(EventLoop *loop)
    : ownerLoop_(loop)
{
}

bool Poller::hasChannel(Channel *channel) const
{
    auto it = channels_.find(channel->fd());//channel->fd()实际上就是监听的sockfd
    if (it != channels_.end())
    {
        if (it->second == channel)
        {
            return true;
        }
    }
        return false;
}