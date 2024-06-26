#include "rpcprovider.h"
#include "mprpcapplication.h"
#include "rpcheader.pb.h"
#include "zookeeperutil.h"
#include "logger.h"
// 这里是框架提供给外部使用的， 可以发布rpc方法的函数接口
void RpcProvider::NotifyService(google::protobuf::Service *service)
{
    ServiceInfo service_info;

    // 获取服务对象的描述信息
    const google::protobuf::ServiceDescriptor *pserviceDesc = service->GetDescriptor();
    // 获取服务的名字
    std::string service_name = pserviceDesc->name();
    // 获取服务对象service 方法的数量
    int methodCnt = pserviceDesc->method_count();

    // std::cout << "service_name: " << service_name << std::endl;
    LOG_INFO("service_name:%s", service_name.c_str());

    for(int i = 0; i < methodCnt; i++){     // 遍历服务对象的所有方法
        // 获取服务对象指定下标的服务方法的描述（抽象描述）
        const google::protobuf::MethodDescriptor* pmethodDesc = pserviceDesc->method(i);
        std::string method_name = pmethodDesc->name();
        // std::cout << "method_name: " << method_name << std::endl;
        LOG_INFO("method_name:%s", method_name.c_str());
        service_info.m_methodMap.insert({method_name, pmethodDesc});
    }
    service_info.m_service = service;
    m_serviceMap.insert({service_name, service_info});

}
//启动rpc服务节点，开始提供rpc远程网络调用服务
void RpcProvider::Run()
{
    std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    muduo::net::InetAddress address(ip, port);
    // 创建TcpServer对象
    muduo::net::TcpServer server(&m_eventLoop, address, "RpcProvider");
    // 绑定连接回调和消息读写回调方法  分离了网络代码和业务代码
    server.setConnectionCallback(std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1));
    server.setMessageCallback(std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // 设置muduo库的线程数量
    server.setThreadNum(4);

    // 把当前rpc节点上要发布的服务全部注册到zk上面，让rpc client 可以从zk上发现服务
    ZkClient zkCli;
    zkCli.Start();
    // service_name 作为永久性节点， method_name 作为临时性节点
    for(auto &sp : m_serviceMap){
        // service_name
        std::string service_path = "/" + sp.first;
        zkCli.Create(service_path.c_str(), nullptr, 0);
        for(auto &mp : sp.second.m_methodMap){
            // /service_name/method_name
            std::string method_path = service_path + "/" + mp.first;
            char method_path_data[128] = {0};
            sprintf(method_path_data, "%s:%d", ip.c_str(), port);
            // ZOO_EPHEMERAL表示znode 临时性节点 
            zkCli.Create(method_path.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL);
        }
    }
    // std::cout << "RpcProvider start service at ip:" << ip << " port:" << port << std::endl;
    LOG_INFO("RpcProvider start service at ip:%s port:%d",ip, port);
    // 启动网络服务
    server.start();
    m_eventLoop.loop();
}

void RpcProvider::OnConnection(const muduo::net::TcpConnectionPtr& conn)
{
    if(!conn->connected())
    {
        // 和rpc client 的连接断开
        conn->shutdown();
    }
}

/*
在框架内部，RpcProvider 和 RpcConsumer协商好 之间通信用的protobuf数据类型
service_name method_name args 定义proto的message类型，进行数据头的序列号和反序列化

header_size(4B)[service_name method_name的长度] + header_str()

*/

// 已建立连接用户的读写事件回调。如果远程有一个rpc服务的调用请求，那么OnMessage方法就会响应
void RpcProvider::OnMessage(const muduo::net::TcpConnectionPtr& conn,
                                muduo::net::Buffer* buffer,
                                muduo::Timestamp)
{
    // 网络上接收的远程rpc调用请求的字符流
    std::string recv_buf = buffer->retrieveAllAsString();
    // 从字符流中读取前4个字节的内容
    uint32_t header_size = 0;
    // 将recv_buf中的内容从第0字节开始，拷贝4个字节 到header_size中
    recv_buf.copy((char*)&header_size, 4, 0);
    //根据header_size 读取数据头的原始字符流 service_name method_name args 三部分内容
    std::string rpc_header_str = recv_buf.substr(4, header_size);
    mprpc::RpcHeader rpcHeader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size;
    if(rpcHeader.ParseFromString(rpc_header_str))
    {
        // 数据头反序列化成功
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
    }else{
        // 数据头反序列化失败
        // std::cout << "rpc_header_str:" << rpc_header_str << " parse error!" << std::endl;
        LOG_INFO("rpc_header_str:%s parse error!", rpc_header_str.c_str());
        return ;
    }
    // 获取rpc方法参数的字节流数据
    std::string args_str = recv_buf.substr(4 + header_size, args_size);

    // 打印调试信息
    // std::cout << "======================================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl;
    LOG_INFO("header_size: %d", header_size);
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
    LOG_INFO("rpc_header_str: %s", rpc_header_str.c_str());
    std::cout << "service_name: " << service_name << std::endl;
    LOG_INFO("service_name: %s", service_name.c_str());
    std::cout << "method_name: " << method_name << std::endl;
    LOG_INFO("method_name: %s", method_name.c_str());
    std::cout << "args_str: " << args_str << std::endl;
    LOG_INFO("args_str: %s", args_str.c_str());
    // std::cout << "======================================" << std::endl;

    // 获取service 对象和 method对象
    auto it = m_serviceMap.find(service_name);
    if(it == m_serviceMap.end()){   // 请求的服务对象不存在
        std::cout << service_name << " is not exits!" << std::endl;
        return;
    }
    auto mit = it->second.m_methodMap.find(method_name);
    if(mit == it->second.m_methodMap.end()){        // 请求service 的方法不存在
        std::cout << service_name << ":" << method_name << " is not exits!" << std::endl;
        return;
    }
    // 获取service 对象
    google::protobuf::Service *service = it->second.m_service;
    // 获取 method 对象
    const google::protobuf::MethodDescriptor *method = mit->second;
    
    // 生成rpc方法调用的请求request 和 响应response参数
    google::protobuf::Message *request = service->GetRequestPrototype(method).New();
    // 反序列化
    if(!request->ParseFromString(args_str)){
        std::cout << "request parse error, content: " << args_str << std::endl;
        return;
    }
    google::protobuf::Message *response = service->GetResponsePrototype(method).New();
    //给下面的method 方法的调用，绑定一个Closure的回调函数  conn, response 是传入SendRpcResponse的参数
    google::protobuf::Closure *done = google::protobuf::NewCallback<RpcProvider, 
                                                                    const muduo::net::TcpConnectionPtr&, 
                                                                    google::protobuf::Message*>
                                                                    (this, 
                                                                    &RpcProvider::SendRpcResponse, 
                                                                    conn, 
                                                                    response);
    // 在框架上根据远端rpc请求， 调用当前rpc节点上发布的方法
    service->CallMethod(method, nullptr, request, response, done);

}

// Closure 的回调操作，用于序列号rpc的响应和网络发送
void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr& conn, google::protobuf::Message* response)
{
    std::string response_str;
    if((response->SerializeToString(&response_str)))
    {
        // 序列号成功后，通过网络把rpc方法执行的结果发送回 rpc的调用方
        conn->send(response_str);
        conn->shutdown();       // 模拟http的短链接服务，由rpcprovider主动断开连接
    }else{
        std::cout << "serialize response_str error!" << std::endl;
    }
    conn->shutdown();       
}