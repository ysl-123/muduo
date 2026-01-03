#include "Buffer.h"
#include "error.h"
#include <sys/uio.h>
#include <unistd.h>
/*从fd上读取数据，默认工作在lt模式
buffer缓冲区是有大小的，但是从fd上读数据的时候，却不知道tcp数据最终的大小
*/
ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    // 在栈上开辟一个 64KB 的临时缓冲区
    // 当底层 buffer 写不下时，数据会先写到这里
    char extrabuf[65536] = {0};

    struct iovec vec[2];  // readv 的分散读结构，最多两个缓冲区

    // 当前 Buffer 可写空间大小
    const size_t writable = writableBytes();

    // 第一个缓冲区：指向 buffer_ 内部当前写指针的位置
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    // 第二个缓冲区：指向 extrabuf
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    // 如果 Buffer 内部剩余空间 < sizeof extrabuf，则使用两个缓冲区
    // 否则只用内部 buffer（extrabuf 不需要）
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;

    // readv：从 fd 读取数据到 vec 指定的多个缓冲区中
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if (n < 0)
    {
        // 读取失败，保存 errno 供上层处理
        *saveErrno = errno;
    }
    else if (n <= writable)
    {
        // 数据量没超过 Buffer 剩余的可写空间
        // 说明 extrabuf 没被用到
        writerIndex_ += n;
    }
    else // n > writable，说明 extrabuf 也写入了数据
    {
        // buffer_ 的可写部分全部写满
        writerIndex_ = buffer_.size();

        // extrabuf 中多余的数据 append 到 buffer 后面
        append(extrabuf, n - writable);
    }

    return n;  // 返回总共读取的字节数
}

ssize_t Buffer::writeFd(int fd, int *saveErrno)
{   //
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}