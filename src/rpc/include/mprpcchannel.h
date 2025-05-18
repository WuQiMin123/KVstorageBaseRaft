#ifndef MPRPCCHANNEL_H
#define MPRPCCHANNEL_H

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobif/service.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;

// 实现了 Protobuf 中定义的 RPC 通信接口
// 将本地调用转换为网络请求，并发送到服务端
class MprpcChannel:public google::protobuf::RpcChannel{
public:
    void CallMethod(const google::protobuf::MethodDescriptor *method, google::protobuf::RpcController *controller,
                    const google::protobuf::Message *request, google::protobuf::Message *response, 
                    google::protobuf::Closure *done) override;
    MprpcChannel(string ip, short port, bool connectNow);


private:
    int m_clientFd;                 //客户端 socket 文件描述符
    const string m_ip;         //服务器 IP 地址
    const uint16_t m_port;          //服务器端口号
    bool newConnect(const char *ip, unint16_t port, string *errMsg);        // 建立 TCP 连接 
};