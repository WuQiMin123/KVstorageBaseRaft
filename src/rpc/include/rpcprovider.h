#pragma once
#include <google/protobuf/descriptor.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <mudoo/net/TcpServer.h>
#include <functional>
#include <string>
#include <unordered_map>
#include "google/protobuf/service.h"

class RpcProvider{
public:
    // 提供给外部使用，可以发布rpc方法的接口
    void NotifyService(google::protobuf::Service *service);

    // 启动rpc服务节点，开始提供rpc远程网络调用服务
    void Run(int nodeIndex, short port);

private:
    muduo::net::EventLoop m_eventLoop;      
    std::shared_ptr<muduo::net::TcpServer> m_muduo_server; // 安全地持有并管理一个 Muduo TCP 服务器对象

    struct ServiceInfo{
        google::protobuf::Service *m_service;   // 保存服务对象
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor *> m_methodMap;    // 保存服务方法
    };

    // 存储注册成功的服务对象和其他服务方法的所有信息
    std::unordered_map<std::string, ServiceInfo> m_serviceMap;

    // 新的socket连接回调,处理 TCP 连接事件
    void OnConnection(const muduo::net::TcpConnection &);
    // 已建立连接用户的读写事件回调, 负责*接收*来自客户端的数据
    void OnMessage(const muduo::net::TcpConnectionPtr &, muduo:::net::Buffer *, muduo::Timestamp);
    // Closure的回调操作，将处理完成的*响应*消息通过 TCP 回发给客户端
    void SendRpcResponse(const muduo::net::TcpConnectionPtr &, google::protobuf::Message *);

public:
    ~RpcProvider();

    
}