# Chat
基于muduo网络库实现的集群式聊天服务器 采用了nginx的tcp负载均衡模块进行了反向代理 并且实现了跨服务器进行聊天

#编译脚本
执行项目目录下的autobuild.sh(自动编译脚本) 可以自动编译获取可执行文件

#项目文件
/bin: 存放最终的可执行文件
/build: cmake指令生成的文件
/include: 项目中使用的头文件
/src: 源文件
/thirdparty: 项目中使用的第三方库文件
