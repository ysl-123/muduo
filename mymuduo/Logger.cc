#include "Logger.h"
#include <time.h>
#include <iostream>
#include <thread>

// 1. 修改点：函数名 instance -> GetInstance
Logger &Logger::GetInstance()
{
    static Logger logger;
    return logger;
}

Logger::Logger()
{
    // 启动专门的写日志线程
    std::thread writeLogTask([&]() {
        for (;;)
        {
            // 1. 从队列中获取一条日志
            std::string msg = m_lckQue.Pop(); 

            // 2. 获取时间
            time_t now = time(nullptr);
            tm *nowtm = localtime(&now);

            char file_name[128];
            sprintf(file_name, "%d-%d-%d-log.txt", nowtm->tm_year + 1900, nowtm->tm_mon + 1, nowtm->tm_mday);

            // 3. 写入文件
            FILE *pf = fopen(file_name, "a+");
            if (pf != nullptr)
            {
                char time_buf[128] = {0};
                // 这里直接格式化时间即可，[INFO]标签已经在宏里加过了
                sprintf(time_buf, "%02d:%02d:%02d => ", 
                        nowtm->tm_hour, nowtm->tm_min, nowtm->tm_sec);
                
                msg.insert(0, time_buf);
                msg.append("\n");
                
                fputs(msg.c_str(), pf);
                fclose(pf); 
            }
        }
    });

    writeLogTask.detach();
}

// 2. 修改点：函数名 setLogLevel -> SetLogLevel，参数 int -> LogLevel
void Logger::SetLogLevel(LogLevel level)
{
    m_loglevel = level;
}

// 3. 修改点：函数名 log -> Log
void Logger::Log(std::string msg)
{
    m_lckQue.Push(msg);
}