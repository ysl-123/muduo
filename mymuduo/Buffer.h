#pragma once
#include <vector>
#include <algorithm>
#include <string>
// 网络库底层的缓冲器类型定义
class Buffer
{
public:
    /*
   +---------------------+----------------------+----------------------+
| prependable bytes   | readable bytes       | writable bytes       |
|                     | (CONTENT)            |                      |
+---------------------+----------------------+----------------------+
0       <=       readerIndex       <=       writerIndex       <=       size
    */
    // 预留头部空间，方便在数据前面插入包头，不用整体移动数据，默认 8 字节
    static const size_t kCheapPrepend = 8;
    // 存放实际的 TCP 收发数据（正文内容）
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
    //我们底层是一个数组，然后大小是头部空间大小+initialsize
        : buffer_(kCheapPrepend + initialSize),
         readerIndex_(kCheapPrepend), // 初始读指针 
          writerIndex_(kCheapPrepend) // 初始写指针
    {
    }
    // 计算当前缓冲区内，可供上层读取的数据长度
    size_t readableBytes() const
    {
        return writerIndex_ - readerIndex_;
    }
    // 计算当前 Buffer 中还能写入多少字节数据
    size_t writableBytes() const
    {
        return buffer_.size() - writerIndex_;
    }
    // Buffer 头部的预留空间大小
    size_t prependableBytes() const
    {
        return readerIndex_;
    }
    // 返回缓冲区中可读数据的起始地址
    const char *peek() const
    {
        return begin() + readerIndex_;
    }

    // 读取 socket 数据 → 放到 Buffer ->应用层处理完这部分数据后 → 调用 retrieve(len) 进行逻辑上删除
    void retrieve(size_t len)
    {
        if (len < readableBytes())
        {
            // 只移动 readerIndex_，表示这部分数据被消费了
            readerIndex_ += len;
        }
        else
        {
            // 如果 len >= 可读数据大小，说明可读区全部消费完
            retrieveAll();
        }
    }
    // 将上面的逻辑删除变为实际的物理删除
    void retrieveAll()
    {
        readerIndex_ = kCheapPrepend; // 重置读指针到预留区后
        writerIndex_ = kCheapPrepend; // 重置写指针到预留区后
    }
    // 将onmessage的函数上报的buffer数据，转为string类型的数据返回
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes()); // 应用可读数据的长度
    }

    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len); // 就创建了result的字符串  实际就是string的构造函数
        retrieve(len);                   // 上面一句把缓冲区中可读的数据已经读出来了，现在需要将其进行复位操作
        return result;
    }
    // 把[data, data+len]内存上的数据，添加到writable缓冲区当中
    void append(const char *data, size_t len)
    {
        ensureWriteableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    char *beginWrite()
    {
        return begin() + writerIndex_;
    }

    const char *beginWrite() const
    {
        return begin() + writerIndex_;
    }

    // buffer_.size()-writeindex_ 也就是writableBytes()  和len比较
    void ensureWriteableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len); // 扩容函数
        }
    }
    // 从fd上读数据
    ssize_t readFd(int fd, int *saveErrno);
    // 通过fd发送数据
    ssize_t writeFd(int fd, int *saveErrno);

private:
    char *begin()
    {
        // it.operator*()
        return &*buffer_.begin(); // vector底层数组首元素的地址，也就是数组的起始地址
    }
    const char *begin() const
    {
        return &*buffer_.begin();
    }

    void makeSpace(size_t len)
    {
        // 如果当前剩余可写空间 + 前面已读完可复用的空间
        // 仍然不足以容纳 len + kCheapPrepend
        // ==> 说明缓冲区整体空间不够，需要扩大 buffer_
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            // 直接扩容：扩到当前写位置 + 需要写入的数据长度
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            // 否则说明：不需要扩容，只需要把可读数据往前挪
            // 通过“数据搬移”复用前面已被读过的空间

            size_t readable = readableBytes();

            // 把原来 [readerIndex_, writerIndex_) 这段可读数据
            // 统一往前挪到从 kCheapPrepend 开始的位置，留出 prepend 区
            std::copy(begin() + readerIndex_,   // 原可读起点
                      begin() + writerIndex_,   // 原可读终点
                      begin() + kCheapPrepend); // 新位置起点（前面保留kCheapPrepend）

            // 调整读写指针
            readerIndex_ = kCheapPrepend;           // 读指针挪到新位置
            writerIndex_ = readerIndex_ + readable; // 写指针在读指针后面
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};