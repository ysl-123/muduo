#include "InetAddress.h"
#include <string.h>
InetAddress::InetAddress(uint16_t port, std::string ip)
{
    bzero(&addr_, sizeof addr_);
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);//htons主机字节序转换为网络字节序
    // c_str() 就是把 C++ 字符串转换成 C 字符串，以便调用只接受 const char* 的函数
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());//点分十进制的 IPv4 地址字符串 转换成 网络字节序的 32 位整数（in_addr_t）
};

std::string InetAddress::toIp() const
{
    // addr_
    char buf[64] = {0};
    //inet_ntop：转换成点分十进制字符串，存入 buf
    //::去 全局命名空间 找 inet_ntop 函数，不管当前类或命名空间里有没有同名函数。其实不加也可以
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    return buf;
};
std::string InetAddress::toIpPort() const
{
    // ip:port
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buf + end, ":%u", port);
    return buf;
};
uint16_t InetAddress::toPort() const
{
    return ntohs(addr_.sin_port);
};
