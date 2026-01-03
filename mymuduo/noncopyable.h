#pragma once
/*
noncopyable被继承以后，派生类对象可以正常的构造和析构，
但是派生类对象无法进行拷贝构造和赋值操作
*/
class noncopyable{
public:
//=delete就相当于直接将他删除了
    noncopyable(const noncopyable & other)=delete;
    noncopyable &operator=(const noncopyable & other)=delete;
protected:
    noncopyable()= default;
    ~noncopyable()= default;
};