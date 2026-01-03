#include "Timestamp.h"
#include <time.h>

/*
Timestamp::Timestamp() {
    microSecondsSinceEpoch_ = 0;
}
Timestamp::Timestamp(int64_t microSecondsSinceEpoch) {
    microSecondsSinceEpoch_ = microSecondsSinceEpoch;
}
*/
Timestamp::Timestamp():microSecondsSinceEpoch_(0){};
Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
   :microSecondsSinceEpoch_(microSecondsSinceEpoch){}
Timestamp Timestamp::now(){
    //time() 是 C 标准库函数，用来获取当前时间。nullptr 表示不需要将时间保存到传入的变量中，函数只返回时间值。
    time_t ti=time(nullptr);
    return Timestamp(ti);
}
std::string Timestamp::toString() const{
    char buf[128]={0};
    //localtime把 time_t 类型（秒数）转换为一个 tm 结构体，里面包含了分解后的年月日时分秒等本地时间信息。
    tm *tm_time =localtime(&microSecondsSinceEpoch_);
    snprintf(buf,128,"%4d/%02d/%02d %02d:%02d:%02d",
    tm_time->tm_year+1900,
    tm_time->tm_mon+1,
    tm_time->tm_mday,
    tm_time->tm_hour,
    tm_time->tm_min,
    tm_time->tm_sec);
    return buf;
}
int main(){

}