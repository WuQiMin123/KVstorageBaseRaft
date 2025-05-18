#include "rpcprovider.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <string>
#include "rpcheader.pb.h"
#include "util.h"

// 构建服务信息
void RpcProvider::NotifyService(google::protobuf::Service *service) {
    const google::protobuf::ServiceDescriptor *pserviceDesc = service->GetDescriptor();
    
    // first
    std::string service_name = pserviceDesc->name();
    std::cout << "service_name:" << service_name << std::endl;
    
    // second  
    ServiceInfo service_info;
    int methodCnt = pserviceDesc->method_count();
    for (int i = 0; i < methodCnt; ++i) {
        // 获取了服务对象指定下标的服务方法的描述（抽象描述） UserService   Login
        const google::protobuf::MethodDescriptor *pmethodDesc = pserviceDesc->method(i);
        std::string method_name = pmethodDesc->name();
        service_info.m_methodMap.insert({method_name, pmethodDesc});
    }
    service_info.m_service = service;
    
    m_serviceMap.insert({service_name, service_info});
}

// 启动rpc服务节点，开始提供rpc远程网络调用服务
void RpcProvider::Run(int nodeIndex, short port) {
    // 1.获取可用ip
    char hname[128];
    gethostname(hname, sizeof(hname));
    struct hostent *hent;
    hent = gethostbyname(hname);
    char *ipC;
    for (int i = 0; hent->h_addr_list[i]; i++) {
        ipC = inet_ntoa(*(struct in_addr *)(hent->h_addr_list[i]));  
    }
    std::string ip = std::string(ipC);

    // 2.本机信息写入test.conf
    std::string node = "node" + std::to_string(nodeIndex);
    std::ofstream outfile;
    outfile.open("test.conf", std::ios::app);
    if (!outfile.is_open()) {
        std::cout << "打开文件失败！" << std::endl;
        exit(EXIT_FAILURE);
    }
    outfile << node + "ip=" + ip << std::endl;
    outfile << node + "port=" + std::to_string(port) << std::endl;
    outfile.close();

    // 3.创建服务器
    muduo::net::InetAddress address(ip, port);
    m_muduo_server = std::make_shared<muduo::net::TcpServer>(&m_eventLoop, address, "RpcProvider"); //？
    
    //设置
    m_muduo_server->setConnectionCallback(std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1)); //？
    m_muduo_server->setMessageCallback(std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    m_muduo_server->setThreadNum(4);
    std::cout << "RpcProvider start service at ip:" << ip << " port:" << port << std::endl;
    
    // 4.启动
    m_muduo_server->start();    //启动底层网络服务并监听客户端连接的到来
    m_eventLoop.loop(); //启动事件循环，等待事件的到来并处理事件
}


void RpcProvider::OnConnection(const muduo::net::TcpConnectionPtr &conn) {
    if (!conn->connected()) {   //客户端断开了连接
        conn->shutdown();   //表示本端也不再发送数据
    }
}

void RpcProvider::OnMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buffer, muduo::Timestamp) {
    // 接收的远程rpc调用请求的字符流
    std::string recv_buf = buffer->retrieveAllAsString();
    
    // 使用protobuf的CodedInputStream来解析数据流
    google::protobuf::io::ArrayInputStream array_input(recv_buf.data(), recv_buf.size());
    google::protobuf::io::CodedInputStream coded_input(&array_input);

    // 读取头部
    uint32_t header_size{};
    coded_input.ReadVarint32(&header_size);
    google::protobuf::io::CodedInputStream::Limit msg_limit = coded_input.PushLimit(header_size);   // 设置读取限制
    std::string rpc_header_str;
    coded_input.ReadString(&rpc_header_str, header_size);
    coded_input.PopLimit(msg_limit); //恢复之前的限制，以便安全地继续读取其他数据

    // 解析头部
    RPC::RpcHeader rpcHeader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size{};
    if (rpcHeader.ParseFromString(rpc_header_str)) {
        // 数据头反序列化成功
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
    }else{
        std::cout << "rpc_header_str:" << rpc_header_str << " parse error!" << std::endl;
        return; 
    }

    // 读取参数
    std::string args_str;
    bool read_args_success = coded_input.ReadString(&args_str, args_size);
    if (!read_args_success) {           // 处理错误：参数数据读取失败
        return;
    }

    // 获取service对象和method对象
    auto it = m_serviceMap.find(service_name);
    // 服务不存在
    if (it == m_serviceMap.end()) {
        std::cout << "服务：" << service_name << " is not exist!" << std::endl;
        std::cout << "当前已经有的服务列表为:";
        for (auto item : m_serviceMap) {
            std::cout << item.first << " ";
        }
        std::cout << std::endl;
        return;  
    }
    // 服务存在
    auto mit = it->second.m_methodMap.find(method_name);
    if (mit == it->second.m_methodMap.end()) {
        std::cout << service_name << ":" << method_name << " is not exist!" << std::endl;
        return;
    }
    google::protobuf::Service *service = it->second.m_service; 
    const google::protobuf::MethodDescriptor *method = mit->second;

    // 生成rpc方法调用的请求request和响应response参数
    google::protobuf::Message *request = service->GetRequestPrototype(method).New();
    if (!request->ParseFromString(args_str)) {
        std::cout << "request parse error, content:" << args_str << std::endl;
        return;
    }
    google::protobuf::Message *response = service->GetResponsePrototype(method).New();
   
    google::protobuf::Closure *done =
        google::protobuf::NewCallback<RpcProvider, const muduo::net::TcpConnectionPtr &, google::protobuf::Message *>(
            this, &RpcProvider::SendRpcResponse, conn, response);

    //真正调用方法
    /*
    为什么下面这个service->CallMethod 要这么写？或者说为什么这么写就可以直接调用远程业务方法了
    这个service在运行的时候会是注册的service
    // 用户注册的service类 继承 .protoc生成的serviceRpc类 继承 google::protobuf::Service
    // 用户注册的service类里面没有重写CallMethod方法，是 .protoc生成的serviceRpc类 里面重写了google::protobuf::Service中
    的纯虚函数CallMethod，而 .protoc生成的serviceRpc类 会根据传入参数自动调取 生成的xx方法（如Login方法），
    由于xx方法被 用户注册的service类 重写了，因此这个方法运行的时候会调用 用户注册的service类 的xx方法
    真的是妙呀
    */
    service->CallMethod(method, nullptr, request, response, done);
}


void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr &conn, google::protobuf::Message *response) {
    std::string response_str;
    if (response->SerializeToString(&response_str)){
        conn->send(response_str);
    } else {
        std::cout << "serialize response_str error!" << std::endl;
    }
}

RpcProvider::~RpcProvider() {
    std::cout << "[func - RpcProvider::~RpcProvider()]: ip和port信息：" << m_muduo_server->ipPort() << std::endl;
    m_eventLoop.quit();
}


