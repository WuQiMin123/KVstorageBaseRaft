#ifndef UTIL_H
#define UTIL_H      // 避免同一个头文件被多次包含进源文件

#include <arpa/inet.h>      // POSIX标准中的头文件，通常用于网络编程和系统调用
#include <sys/socket.h>
#include <unistd.h>
#include <boost/archive/text_iarchive.hpp>          //序列化相关
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/access.hpp>
#include <condition_variable>                       //多线程
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <random>
#include <sstream>
#include <thread>
#include "config.h"

template<calss F>
class DeferClass{       // 用于在对象生命周期结束时自动执行某个函数
public:  
DferClass(F&& f) : m_func(std::forward<F>(f)) {}
    DferClass(const F& f) : m_func(f) {}
    ~DferClass() {m_func();}

    // 避免管理多个对象尝试执行同一个函数
    DeferClass(const DeferClass& e) = delete;
    DeferClass& operator=(const DferClass& e) = delete;

private:
    F m_func;       // 存储一个可调用对象
}

//
#define _CONCAT(a, b) a##b   
#define _MAKE_DEFER_(line)  DeferClass _CONCAT(defer_placeholder, line) = [&]() 

#undef DEFER        
#define DEFER _MAKE_DEFER_(__LINE__)    

void DPrintf(const char* format, ...);      // ... 用于表示可变参数列表

void myAssert(bool condition, std::string message = "Assertion failed!");

// 返回一个格式化后的 std::string
template<typename... Args>
std::string format(const char* format_str, Args... args){
    int size_s = std::snprintf(nullptr, 0, format_str, args...) + 1;        // 计算格式化后字符串所需缓冲区的大小
    if(size_s <= 0) {throw std::runtime_error("Error during formatting.")};
    auto size = static_cast<size_t>(size_s);  
    std::vector<char> buf(size);
    std::snprintf(buf.data(), size, format_str, args...);
    return std::string(buf.data(), buf.data() + size - 1);      // remove '\0'
}

std::chrono::_V2::system_clock::time_point now();
std::chrono::milliseconds getRandomizedElectionTimeout();
void sleepNMilliseconds(int N);

// 异步写日志的日志队列
template <typename T>
class LockQueue{
public:
    void Push(const T& data){
        std::lock_guard<std::mutex> lock(m_mutex);      //遵循RAII原则，在构造时自动锁定互斥量，在析构时自动释放互斥量,避免死锁和其他并发问题
        m_queue.push(data);
        m_condvariable.notify_one();        //通知一个等待的线程
    }

    T Pop(){
        std::unique_lock<std::mutex> lock(m_mutex);
        while(q_queue.empty()){
            m_condvariable.wait(lock); //这里用unique_lock是因为lock_guard不支持解锁，而unique_lock支持
        }
        T data = m_queue.front();
        m_queue.pop();
        return data;
    }

    bool timeOutPop(int timeout, T* ResData){
        std::unique_lock<std::mutex> lock(mutex);

        // 获取当前时间点，并计算超时时刻
        auto now = std::chrono::system_clock::now();
        auto timeout_time = now + std::chrono::milliseconds(timeout);

        // 不断检查队列是否为空,直到发生超时
        while(m_queue.empty()){
            if(m_condvariable.wait_until(lock, timeout_time) == std::cv_status::timeout){
                return false
            }else{
                continue;
            }
        }

        T data = m_queue.front();
        m_queue.pop();
        *ResData = data;
        return true;
    }


private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_condvariable;
}


// op是kv传递给ratf的command
class Op{
public:
    std::string Operation;      //操作类型
    std::string Key;            
    std::string Value;          
    std::string ClientId;       //客户端 ID
    int RequestId;              //请求编号


public:
    std::string assString() const {
        std::stringstream ss;
        boost::archive::text_oarchive oa(ss);

        oa << *this;

        return ss.str();
    }

    bool parseFromString(std::string str){
        std::stringstream iss(str);
        boost::archive::text_iarchive ia(iss);

        ia >> *this;
        return true;
    }

public:
    // 实现自定义的输出格式
    friend std::ostream& operator<<(std::ostream& oa, const Op& obj){
        os << "[MyClass:Operation{" + obj.Operation + 
              "}, key{" + obj.Key + 
              "}, Value{" + obj.Value +
              "}, ClientId{" + obj.ClientId +
              "}, RequestId{" + std::to_string(obj.RequestId) +
              "}";
        return os;
    }

private:
    // boost::serialization::access 类可以访问 Op 类的私有成员和保护成员
    friend class boost::serialization::access;
    
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version){
        ar& Operation;
        ar& Key;
        ar& Value;
        ar& ClientId;
        ar& RequestId;
    }
};

const std::string OK = "OK";
const std::string ErrNoKey = "ErrNoKey";
const std::string ErrWrongLeader = "ErrWrongLeader";

// 获取可用端口
bool isReleasePort(unsigned short usPort);
bool getReleasePort(short& port);


// 实例
// int main() {
//     // 创建一个 Op 对象
//     Op op;
//     op.Operation = "set";
//     op.Key = "exampleKey";
//     op.Value = "exampleValue";
//     op.ClientId = "client123";
//     op.RequestId = 1;

//     // 序列化
//     std::string serializedOp = op.assString();
//     std::cout << "Serialized Op: " << serializedOp << std::endl;

//     // 反序列化
//     Op newOp;
//     newOp.parseFromString(serializedOp);

//     // 输出反序列化后的对象
//     std::cout << "Deserialized Op: " << newOp << std::endl;

//     return 0;
// }



#endif