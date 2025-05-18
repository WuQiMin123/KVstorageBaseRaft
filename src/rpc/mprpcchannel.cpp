#include "mprpcconfig.h"
#include <arpa/inet.h>
#include <netlinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <string>
#include "mprpccontroller.h"
#include "rpcheader.pb.h"
#include "util.h"

void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method, 
                              google::protobuf::RpcController *controller,
                              const google::protobuf::Message *request, 
                              google::protobuf::Message *response, 
                              google::protobuf::Closure *done){
    // 1.客户端连接失败后尝试重新连接
    if(m_clientFd == -1){
        std::string errMsg;
        bool rt = newConnect(m_ip.c_str(), m_port, &errMsg);
        if(!rt){
            DPrintf("[func-MprpcChannel::CallMethod]重连接ip：{%s} port{%d}失败", m_ip.c_str(), m_port);
            controller->SetFailed(errMsg);
            return;
        }else{
            DPrintf("[func-MprpcChannel::CallMethod]连接ip：{%s} port{%d}成功", m_ip.c_str(), m_port)
        }
    }

    // 2.将请求序列化（包含实际的业务逻辑参数）
    uint32_t args_size{};       //初始化为 0
    std::string args_str;
    if(request->SerializeToString(&args_str)){
        args_size = args_str.size();
    }else{
        controller->SetFailed("serialize request error!");
        return ;
    }

    // （头部,不包含完整的参数）
    const google::protobuf::ServiceDescriptor* sd = method->service();
    std::string service_name = sd->name();
    std::string method_name = method->name();

    RPC::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_size(args_size);

    std::string rpc_header_str;
    if(!RpcHeader.SerializeToString(&rpc_header_str)){
        controller->SetFailed("serialize rpc header error!");
        return ;
    }

    // 3.构建发送的数据流
    std::string send_rpc_str;
    {
        // 创建一个OutputStream用于写入send_rpc_str
        google::protobuf::io::StringOutputStream string_output(&send_rpc_str);  //绑定
        google::protobuf::io::CodedOutputStream coded_output(&string_output); //序列化为紧凑的二进制格式

        // 先写入header的长度
        coded_output.WriteVarint32(static_cast<unit32_t>(rpc_header_str.size()));

        // 写入rpc_header本身
        coded_output.WriteString(rpc_header_str);
    }
    send_rpc_str += args_str;

    // 4.发送rpc请求(发送失败会重试连接再发送，重试连接失败会直接return)
    while(-1 == send(m_clientFd, send_rpc_str.c_str(), send_rpc_str.size(), 0)){
        char errtxt[512] = {0};
        sprintf(errtxt, "send srror! erro: %d", errno);
        std::cout << "尝试重新连接, 对方ip: " << m_ip << "对方端口: " << m_port < std::endl;
        
        close(m_clientFd);
        m_clientFd = -1;        //表示“当前没有有效的连接”，重连会进行重新设置
        
        std::string errMsg;
        bool rt = newConnect(m_ip.ctr(), m_port, &errMsg);
        if(!rt){
            controller->SetFailed(errMsg);
            return ;
        }
    }

    // 5.接收rpc请求的响应值
    char recv_buf[1024] = {0};
    int recv_size = 0;
    if(-1 == (recv_size = recv(m_clientFd, recv_buf, 1024, 0))){
        char errtxt[512] = {0};
        sprintf(errtxt, "recv error! erron:%d", errno);
        
        close(m_client);
        m_clientFd = -1;
        
        controller->SetFailed(errtxt);
        return ;
    }
}

bool MprpcChannel::newConnect(const char*ip, unit16_t port, string* errMsg){
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == clientfd){
        char errtxt[512] = {0};
        sprintf(errtxt, "create socket error! errno:%d", errno);
        
        m_clientFd = -1;
        
        *errMsg = errtxt;
        return false;
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);
    
    // 连接rpc服务节点
    if (-1 == connect(clientfd, (struct sockaddr*)&server_addr, sizeof(server_addr))) {
        char errtxt[512] = {0};
        sprintf(errtxt, "connect fail! errno:%d", errno);

        close(clientfd);
        m_clientFd = -1;

        *errMsg = errtxt;
        return false;
    }

    m_clientFd = clientfd;
    return true;
}


MprpcChannel::MprpcChannel(string ip, short port, bool connectNow) : m_ip(ip), m_port(port), m_clientFd(-1) {
    if (!connectNow) {
        return;
    }  //可以允许延迟连接（不太明白）
    
    std::string errMsg;
    auto rt = newConnect(ip.c_str(), port, &errMsg);
    
    int tryCount = 3;
    while (!rt && tryCount--) {
        std::cout << errMsg << std::endl;
        rt = newConnect(ip.c_str(), port, &errMsg);
    }
}