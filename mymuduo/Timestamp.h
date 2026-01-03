#pragma once

#include <iostream>
#include <string>
#include <cstdint> // int64_t 类型

// Timestamp 类用于表示时间戳，精度为微秒（microseconds）。
// 可获取当前时间、格式化输出字符串，便于在网络库或日志中使用。
class Timestamp
{
public:
    // 默认构造函数，构造一个无效或默认的时间戳（可自行定义为 0 或当前时间）
    Timestamp();

    // 构造函数，将传入的微秒数作为时间戳
    // microSecondsSinceEpoch: 自 1970-01-01 00:00:00 UTC 起的微秒数
    explicit Timestamp(int64_t microSecondsSinceEpoch);

    // 静态方法，获取当前系统时间对应的 Timestamp 对象
    static Timestamp now();

    // 将 Timestamp 转换为字符串表示，通常用于打印或日志
    // 返回格式可以是 "秒.微秒" 或 "YYYY-MM-DD HH:MM:SS.微秒"
    std::string toString() const;

private:
    // 内部存储时间戳的成员变量，单位为微秒
    int64_t microSecondsSinceEpoch_;
};
