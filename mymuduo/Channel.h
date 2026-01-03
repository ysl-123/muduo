#pragma once
#include"Logger.h"
#include "noncopyable.h"
#include"functional"
#include"Timestamp.h"
#include"memory"
//eventloop channnel poller在reactor模型上对应demultiplex
//一个线程里有一个eventloop，一个eventloop里有一个poller，一个poller可以监听很多的channel
class EventLoop;//类的前置声明，就是跟函数声明差不多，就是为了现在用


/*
channel理解为通道，封装了sockfd和其感兴趣的event，如EPOLLIN,EPOLLOUT事件
还绑定了poller监听返回的具体事件
*/
class Channel:noncopyable
{
public:
//实际的作用就是EventCallback就相当于std::function<void()>
using EventCallback=std::function<void()>;
using ReadEventCallback=std::function<void(Timestamp)>;

Channel(EventLoop  *Loop,int fd);
~Channel();

//fd得到poller通知之后，处理事件
void handleEvent(Timestamp receiveTime);

//设置回调函数对象
/*
std::move(cb) 把 cb 标记为右值，表示“我不再关心原来的 cb，可以把它的内部资源直接搬过来”。
赋值时会触发 移动赋值：
readCallback_ 直接接管 cb 内部存储的数据（比如 lambda 的捕获对象、绑定的参数等）。
避免了不必要的内存分配和复制，效率更高。
赋值后：
readCallback_ 拥有原来的回调内容。
cb 变为空（状态安全，但不可再使用原来的回调）
*/
void setReadCallback(ReadEventCallback cb){
    readCallback_=std::move(cb);//这cb类似例子里的callback，然后readcallback保存了之后，后面别的地方调用readcallback（）就能回调
}
void setWriteCallback(EventCallback cb){
    writeCallback_=std::move(cb);
}
void setCloseCallback(EventCallback cb){
    closeCallback_=std::move(cb);
}
void setErrorCallback(EventCallback cb){
    errorCallback_=std::move(cb);
}
//防止当channel被手动remove掉，channel还在执行回调操作
void tie(const std::shared_ptr<void>&);

int fd() const{return fd_;}
int events() const{return events_;;}
void set_revents(int revt){revents_=revt;}

// 设置fd相应的事件状态
void enableReading() { events_ |= kReadEvent; update(); }
void disableReading() { events_ &= ~kReadEvent; update(); }
void enableWriting() { events_ |= kWriteEvent; update(); }
void disableWriting() { events_ &= ~kWriteEvent; update(); }
void disableAll() { events_ = kNoneEvent; update(); }
bool isNoneEvent() const{return events_ ==kNoneEvent;}//判断底层的fd是否注册了感兴趣的事件

bool isWriting() const { return events_ & kWriteEvent; }
bool isReading() const { return events_ & kReadEvent; }

int index() { return index_; }
void set_index(int idx) { index_ = idx; }

EventLoop* ownerLoop(){return loop_;}
void remove();

private:

void update();
//根据自己接收到的相应的事件执行回调的操作
void handleEventWithGuard(Timestamp receiveTime);

static const int kNoneEvent;
static const int kReadEvent;
static const int kWriteEvent;

EventLoop *loop_; // 事件循环
const int fd_;    // fd, Poller监听的对象
int events_; // 注册fd感兴趣的事件
int revents_; // poller返回的具体发生的事件
int index_;

//弱引用（weak_ptr），指向被绑定对象
std::weak_ptr<void> tie_;
//标记当前 Channel 是否绑定了某个对象（通常是 EventLoop
bool tied_;

//因为channel通道里能够绘制fd最终发生的具体的事件 revents_，所以它负责具体时间的回调操作
ReadEventCallback readCallback_;
EventCallback writeCallback_;
EventCallback closeCallback_;
EventCallback errorCallback_;
};