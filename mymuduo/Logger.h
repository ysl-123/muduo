#pragma once
#include "noncopyable.h"
#include <string>
#include <iostream>

//LOG_INFO("%s,%d,arg1,arg2")
//定义一个宏，名字叫 LOG_INFO
////logmsgFormat   LOG_INFO("Hello %s, age = %d", "Tom", 18);"Hello %s, age = %d" 就是 logmsgFormat
/*
当你调用宏时，宏会被 展开，你传的第一个参数就赋值给 logmsgFormat：
LOG_INFO("Hello %s, age = %d", "Tom", 18);
logmsgFormat → 宏传入的格式字符串，比如 "Hello %s, age = %d"
##__VA_ARGS__ → 可变参数，比如 "Tom", 18
就是说谁调用loginfo传递给它的logmsgFormat，然后sprintf里面也用logmsgFormat
*/
#define LOG_INFO(logmsgFormat, ...) \
do \
{ \
    Logger &logger = Logger::instance(); \
    logger.setLogLevel(INFO); \
    char buf[1024] = {0}; \
    snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
    logger.log(buf); \
} while(0)

#define LOG_ERROR(logmsgFormat, ...) \
do \
{ \
    Logger &logger = Logger::instance(); \
    logger.setLogLevel(ERROR); \
    char buf[1024] = {0}; \
    snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
    logger.log(buf); \
} while(0)

#define LOG_FATAL(logmsgFormat, ...) \
do \
{ \
    Logger &logger = Logger::instance(); \
    logger.setLogLevel(FATAL); \
    char buf[1024] = {0}; \
    snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
    logger.log(buf); \
    exit(-1);\
} while(0)

#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...) \
do \
{ \
    Logger &logger = Logger::instance(); \
    logger.setLogLevel(DEBUG); \
    char buf[1024] = {0}; \
    snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
    logger.log(buf); \
} while(0)
#else
   #define LOG_DEBUG(logmsgFormat, ...)
#endif
// 定义日志的级别 INFO ERROR FATAL DEBUG
enum LogLevel
{
    INFO,
    ERROR,
    FATAL,
    DEBUG,
};

// 输出一个日志类
class Logger : noncopyable
{
public:
    // 获取日志的唯一实例对象
    static Logger &instance(); // 相当于定义一个instance函数而已
    // 设置日志级别
    void setLogLevel(int Level);
    // 写日志
    void log(std::string msg);

private:
    int logLevel_;
    Logger() {}
};