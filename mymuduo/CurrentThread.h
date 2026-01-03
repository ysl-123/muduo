#pragma once

namespace CurrentThread
{
    //__thread 是 GCC/Clang 的扩展关键字，用于声明 线程局部变量   t_cachedTid → 变量名
    extern __thread int t_cachedTid;
    void cacheTid();
    //加了inline就是内联函数，就是只在当前文件生效
    inline int tid()
    {
        // 使用GCC内置函数__builtin_expect进行分支预测优化
        // 告诉编译器：t_cachedTid == 0的情况很少发生（预期为0）
        // 一开始为0，所以就要调用直接得到当前线程的id，然后后面就不是0，然后就return了
        if (__builtin_expect(t_cachedTid == 0, 0))
        {
            // 只有当线程ID未缓存时，才执行缓存线程ID的操作
            cacheTid();
        }

        //返回当前线程的线程 ID
        return t_cachedTid;
    }
}