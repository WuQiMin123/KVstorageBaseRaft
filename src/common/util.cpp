#include "util.h"
#include <chrono>
#include <cstdarg>      //提供了对 C++ 中可变参数函数的支持
#include <cstdio>
#include <ctime>
#include <iomanip>      //提供了一系列 可插入到流中的函数对象

void myAssert(bool condition, std::string message){
    if(!condition){
        std::cerr << "Error: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

std::chrono::_V2::system_clock::time_point now(){
    return std::chrono::high_resolution_clock::now();
}

std::chrono::milliseconds getRandomizedElectionTimeout(){
    std::random_device rd;  //生成种子
    std::mt19937 rng(rd()); //伪随机数生成
    std::uniform_int_distribution<int> dist(minRandomizedElectionTime, maxRandomizedElectiontime);  //定义一个均匀分布

    return std::chrono::milliseconds(dist(rng));    //包装成时间间隔
}

void sleepNMilliseconds(int N){
    std::this_thread::sleep_for(std::chrono::milliseconds(N));
}

// 从当前端口号开始，尝试最多 30 次递增，寻找一个“可释放”的端口
bool getReleasePort(short &port){
    short num = 0;
    while(!isReleasePort(port) && num < 30){
        ++port;
        ++num;
    }
    if(num >= 30){
        port = -1;
        return false;
    }
    return true;
}

//通过尝试绑定到指定端口的方式来判断该端口是否可用
bool isReleasePort(unsigned short usPort){
    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    
    // 构造地址结构体
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(usPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // 尝试绑定端口
    int ret = ::bind(s, (sockaddr *)&addr, sizeof(addr));
    if(ret != 0){
        close(s);
        return false;
    }
    close(s);
    return true;
}

//封装一个带时间戳和调试开关的打印函数，用于在调试阶段输出日志信息。
void DPrint(const char *format, ...){
    if(Dbug){
        time_t now = time(nullptr);
        tm *nowtm = localtime(&now);        // 转换为本地时间结构体 tm
        va_list args;                       // va_list 是处理可变参数的标准类型
        va_start(args, format);
        std::printf("[%d-%d-%d-%d-%d-%d] ", nowtm->tm_year + 1900, nowtm->tm_mon + 1, nowtm->tm_mady, nowtm->tm_hour,
                    nowtm->tm_min, nowtm->tm_sec);
        std::vprintf(format, args);     //根据传入的 format 字符串格式输出用户指定的内容
        std::printf("\n");
        va_end(args);       //清理参数列表，防止内存泄漏或未定义行为。
    }
}