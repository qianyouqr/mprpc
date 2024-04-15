#include "mprpcchannel.h"
#include "rpcheader.pb.h"
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include "mprpcapplication.h"
#include "mprpccontrlller.h"
#include "zookeeperutil.h"
// 所有通过stub代理对象调用的rpc方法，都走到这里了，统一做rpc方法调用的数据序列化和网络发送
void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                          google::protobuf::RpcController* controller, const google::protobuf::Message* request,
                          google::protobuf::Message* response, google::protobuf::Closure* done)
{
    const google::protobuf::ServiceDescriptor* sd = method->service();
    std::string service_name = sd->name();
    std::string method_name = method->name();

    // 获取参数的序列化字符串长度 args_size
    int args_size = 0;
    std::string args_str;
    if(request->SerializeToString(&args_str)){
        args_size = args_str.size();
    }else{
        // std::cout << "serialize request error!" << std::endl;
        controller->SetFailed("serialize request error!");
        return;
    }
    // 定义rpc的请求header
    mprpc::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_size(args_size);

    uint32_t header_size = 0;
    std::string rpc_header_str;
    if(rpcHeader.SerializeToString(&rpc_header_str)){
        // 序列化成功
        header_size = rpc_header_str.size();
    }else{
        // std::cout << "serialize rpc header error!" << std::endl;
        controller->SetFailed("serialize rpc header error!");
        return;
    }
    // 组织待发送的rpc请求的字符串
    std::string send_rpc_str;
    send_rpc_str.insert(0, std::string((char*)&header_size, 4));        // header_size
    send_rpc_str += rpc_header_str;
    send_rpc_str += args_str;    
        // 打印调试信息
    std::cout << "======================================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl;
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
    std::cout << "service_name: " << service_name << std::endl;
    std::cout << "method_name: " << method_name << std::endl;
    std::cout << "args_str: " << args_str << std::endl;
    std::cout << "======================================" << std::endl;   

    // tcp 编程，完成rpc远程方法的调用
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == clientfd){
        // std::cout << "create socket error! errno: " << errno << std::endl;
        char errTex[512] = {0};
        sprintf(errTex, "create socket error! errno: %d", errno);
        controller->SetFailed(errTex);
        exit(EXIT_FAILURE);
    }

    // std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    // uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());

    ZkClient zkCli;
    zkCli.Start();
    std::string method_path = "/" + service_name + "/" + method_name;
    std::string host_data = zkCli.GetData(method_path.c_str());
    if(host_data == ""){        // zk中没查到
        controller->SetFailed(method_path + " is not exist!");
        return ;
    }
    int idx = host_data.find(":");
    if(idx == -1){
        controller->SetFailed(method_path + " address is invalid!");
        return ;
    }
    std::string ip = host_data.substr(0,idx);
    uint16_t port = atoi(host_data.substr(idx+1, host_data.size()-idx).c_str());
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    if(-1 == connect(clientfd,(struct sockaddr*)&server_addr,sizeof(server_addr))){
        // std::cout << "connect error! errno: " << errno << std::endl;
        close(clientfd);
        char errTex[512] = {0};
        sprintf(errTex, "connect error! errno: %d", errno);
        controller->SetFailed(errTex);
        exit(EXIT_FAILURE);
    }
    // 发送rpc 请求
    if(-1 == send(clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0))
    {
        // std::cout << "send error! errno: " << errno << std::endl;
        close(clientfd);
        char errTex[512] = {0};
        sprintf(errTex, "send error! errno: %d", errno);
        controller->SetFailed(errTex);
        return;
    }
    // 接收rpc请求的响应
    char recv_buf[1024] = {0};
    int recv_size = 0;
    if(-1 == (recv_size=recv(clientfd, recv_buf, 1024, 0)))
    {
        close(clientfd);
        // std::cout << "recv error! errno: " << errno << std::endl;
        char errTex[512] = {0};
        sprintf(errTex, "recv error! errno: %d", errno);
        controller->SetFailed(errTex);
        return;
    }
    // std::string response_str(recv_buf,0, recv_size);
    // if(response->ParseFromString(response_str))
    if(!response->ParseFromArray(recv_buf, recv_size))
    {
        // std::cout << "parse error! response_str: " << recv_buf << std::endl;
        close(clientfd);
        char errTex[512] = {0};
        sprintf(errTex, "parse error! errno: %s", recv_buf);
        controller->SetFailed(errTex);
        return;
    }
    close(clientfd);
}