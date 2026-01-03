#include "TcpServer.h"
#include "Logger.h"
#include "functional"
#include "strings.h"
#include "TcpConnection.h"
// 检查传进来的loop是否为空
static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
  if (loop == nullptr)
  {
    LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
  }
  return loop;
}
TcpServer::TcpServer(EventLoop *loop,
                     const InetAddress &listenAddr,//ip+端口
                     const std::string &nameArg,
                     Option option)
    : loop_(CheckLoopNotNull(loop)) // baseloop
      ,
      ipPort_(listenAddr.toIpPort()), name_(nameArg), 
      acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
       threadPool_(new EventLoopThreadPool(loop, name_)) // 默认不会开启额外的线程只有主线程运行所谓的baseloop
      ,
      connectionCallback_(), messageCallback_(), nextConnId_(1),started_(0)
{
  // 当有先用户连接时，会执行TcpServer::newConnection回调
  acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
                                                std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    for (auto &item : connections_)
    {
        // 这个局部的shared_ptr智能指针对象，出右括号，可以自动释放new出来的TcpConnection对象资源了
        TcpConnectionPtr conn(item.second);
        item.second.reset();

        // 销毁连接
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn)
        );
    }
}

// 设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads)
{
  threadPool_->setThreadNum(numThreads);
}

// 开启服务器监听 loop.loop()
void TcpServer::start()
{
  if (started_++ == 0) // 因为mymuduo的接口是对外暴露的，防止有傻逼调用了两次start
  {
    threadPool_->start(threadInitCallback_); // 启动底层的loop线程池
    loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
  }
}

// 有一个新的客户端连接，acceptor会执行这个回调操作
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
  // 轮询算法，选择一个subloop，来管理channel
  EventLoop *ioLoop = threadPool_->getNextLoop();
  char buf[64] = {0};
  snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
  ++nextConnId_;
  std::string connName = name_ + buf;

  LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
           name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());
  // 通过sockfd获取其对应的本机的ip地址和端口信息
  sockaddr_in local;
  ::bzero(&local, sizeof local);
  socklen_t addrlen = sizeof local;
  // getsockname 调用后，获取socket 服务器地址信息（IP + 端口）
  if (::getsockname(sockfd, (sockaddr *)&local, &addrlen) < 0)
  {
    LOG_ERROR("sockets::getLocalAddr");
  }
  InetAddress localAddr(local);
  // 根据连接成功的sockfd，创建TcpConnection连接对象
  TcpConnectionPtr conn(new TcpConnection(
      ioLoop,
      connName,//Muduo 内部用来标识这条 TCP 连接的唯一字符串
      sockfd, // connfd
      localAddr,//本地服务器的ip+端口
      peerAddr//这是connfd的也就是客户端的ip+端口
    ));
  connections_[connName] = conn;
  // 下面的回调都是用户设置给TcpServer=>TcpConnection=>Channel=>Poller=>notify channel调用回调

  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  // 设置了如何关闭连接的回调
  conn->setCloseCallback(
      std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));
  // 直接调用TcpConnection::connectEstablished
  //这里的ioloop就不在自己的线程中所以需要在runinloop中的另一个逻辑中
  //不能理解ioloop为什么不在当前线程你就想一想ioloop怎末出来的t->startloop()创建的子线程里创建的ioloop
  ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
  loop_->runInLoop(
      std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
  LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n",
           name_.c_str(), conn->name().c_str());

  connections_.erase(conn->name());
  EventLoop *ioLoop = conn->getLoop();
  ioLoop->queueInLoop(
      std::bind(&TcpConnection::connectDestroyed, conn));
}
