#include "mprpcapplication.h"
#include <iostream>
#include <unistd.h>

//静态成员 需要在类外初始化
MprpcConfig MprpcApplication::m_config;

void ShowArgsHelp()
{
    std::cout << "format: command -i <configfile>" << std::endl;
}

void MprpcApplication::Init(int argc, char ** argv)
{
    if(argc < 2){       // 未传入任何参数   
        ShowArgsHelp();
        exit(EXIT_FAILURE);
    }
    int c = 0;
    std::string config_file;
    //getopt Linux系统上的命令行参数解析工具， 第三个参数表示 选项字符串，表示识别选项i, 冒号表示后面带参数
    while((c = getopt(argc, argv, "i:")) != -1)
    {
        switch (c)
        {
        case 'i':
            config_file = optarg;
            break;
        case '?':       // 出现了不应该出现的参数
            ShowArgsHelp();
            exit(EXIT_FAILURE);
        case ':':       // 出现了 -i ,带没带参数
            ShowArgsHelp();
            exit(EXIT_FAILURE);
        default:
            break;
        }
    }
    // 开始加载配置文件 rpcserver_ip= rpcserver_port zookeeper_ip= zookeeper_port=
    m_config.LoadConfigFile(config_file.c_str());

    // std::cout << "rpcserverip: " << m_config.Load("rpcserverip") << std::endl;
    // std::cout << "rpcserverport: " << m_config.Load("rpcserverport") << std::endl;
    // std::cout << "zookeeperip: " << m_config.Load("zookeeperip") << std::endl;
    // std::cout << "zookeeperport: " << m_config.Load("zookeeperport") << std::endl;

}
MprpcApplication& MprpcApplication::GetInstance()
{
    static MprpcApplication app;
    return app;
}

MprpcConfig& MprpcApplication::GetConfig()
{
    return m_config;
}