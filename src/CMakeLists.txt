# 这种写法有个问题，如果在当前目录下(src)添加了文件，因为之前cmake创建的makefile文件不包括最新的文件会报错。
# aux_source_directory(. SRC_LIST)
# 而如果改成下面这种方式，是需要添加 新建的文件的
set(SRC_LIST 
    mprpcapplication.cc 
    mprpcconfig.cc 
    rpcheader.pb.cc 
    rpcprovider.cc 
    mprpcchannel.cc 
    mprpccontroller.cc 
    logger.cc 
    zookeeperutil.cc)
add_library(mprpc ${SRC_LIST})
target_link_libraries(mprpc muduo_net muduo_base pthread zookeeper_mt)