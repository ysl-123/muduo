#pragma once
#include "lockqueue.h"
#include <string>

// 日志级别定义
enum LogLevel
{
    INFO,  // 普通信息
    ERROR, // 错误信息
    FATAL, // 致命错误
    DEBUG, // 调试信息
};

// 异步日志系统
class Logger
{
public:
    // 获取日志的单例
    static Logger& GetInstance();
    // 设置日志级别
    void SetLogLevel(LogLevel level);
    // 供业务线程调用的接口，将日志推入缓冲队列
    void Log(std::string msg);
    // 获取当前级别
    int GetLogLevel() const { return m_loglevel; }

private:
    int m_loglevel;                  // 记录日志级别
    LockQueue<std::string> m_lckQue; // 日志缓冲队列

    Logger(); // 构造函数中启动写磁盘线程
    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;
};

// --- 定义宏 ---

// INFO级别宏
#define LOG_INFO(logmsgformat, ...) \
do \
{ \
    Logger &logger = Logger::GetInstance(); \
    char c[1024] = {0}; \
    snprintf(c, 1024, logmsgformat, ##__VA_ARGS__); \
    logger.Log(std::string("[INFO] ") + c); \
} while (0)

// ERROR级别宏
#define LOG_ERROR(logmsgformat, ...) \
do \
{ \
    Logger &logger = Logger::GetInstance(); \
    char c[1024] = {0}; \
    snprintf(c, 1024, logmsgformat, ##__VA_ARGS__); \
    logger.Log(std::string("[ERROR] ") + c); \
} while (0)

// FATAL级别宏
#define LOG_FATAL(logmsgformat, ...) \
do \
{ \
    Logger &logger = Logger::GetInstance(); \
    char c[1024] = {0}; \
    snprintf(c, 1024, logmsgformat, ##__VA_ARGS__); \
    logger.Log(std::string("[FATAL] ") + c); \
    exit(EXIT_FAILURE); \
} while (0)

// DEBUG级别宏（只有定义了MUDEBUG时才生效）
#ifdef MUDEBUG
#define LOG_DEBUG(logmsgformat, ...) \
do \
{ \
    Logger &logger = Logger::GetInstance(); \
    char c[1024] = {0}; \
    snprintf(c, 1024, logmsgformat, ##__VA_ARGS__); \
    logger.Log(std::string("[DEBUG] ") + c); \
} while (0)
#else
#define LOG_DEBUG(logmsgformat, ...)
#endif