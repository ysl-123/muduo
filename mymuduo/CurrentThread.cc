#include "CurrentThread.h"

#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread
{
    __thread int t_cachedTid = 0;

    void cacheTid()
    { 
        //t_cachedTid == 0表示当前线程的局部存储变量 t_cachedTid 尚未初始化（即尚未缓存当前线程的 tid）。
        if (t_cachedTid == 0)
        {   //   ::syscall就是linux系统调用。
            //通过linux系统调用，获取当前线程的tid值
            //SYS_gettid 是系统调用编号，对应 获取线程 ID 的系统调用
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}