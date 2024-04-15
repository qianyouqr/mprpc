#include <iostream>
#include "mprpcapplication.h"
#include "friend.pb.h"
int main(int argc, char **argv)
{
    MprpcApplication::Init(argc, argv);
    fixbug::FriendServiceRpc_Stub stub(new MprpcChannel());
    // rpc方法的请求参数
    fixbug::GetFriendsListRequest request;
    request.set_id(1000);
    // rpc方法的响应
    fixbug::GetFriendsListResponse response;

    MprpcController controller;
    stub.GetFriendsList(&controller, &request, &response, nullptr);
    // 一次调用结束，读调用的结果
    if(controller.Failed()){
        std::cout << controller.ErrorText() << std::endl;
    }else{
        if(0 == response.result().errcode()){
            std::cout << "rpc GetFriendsList response success!" << std::endl;
            int size = response.friends_size();
            for(int i = 0; i < size; i++){
                std::cout << "index: " << (i+1) << " name: " << response.friends(i) << std::endl;
            }
        }else{
            std::cout << "rpc GetFriendsList response error : " << response.result().errmsg() << std::endl;
        }
    }

    return 0;
}