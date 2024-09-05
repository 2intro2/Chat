#include "chatserver.hpp"
#include "chatservice.hpp"
#include "json.hpp"

#include <functional>
#include <string>

using namespace std;
using namespace placeholders;
using json=nlohmann::json;

//初始化聊天服务器对象
ChatServer:: ChatServer(EventLoop* loop,const InetAddress& listenAddr,const string& nameArg)
: _server(loop,listenAddr,nameArg),_loop(loop)
{
    //注册连接回调
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection,this,_1));
    //注册读写回调
    _server.setMessageCallback(std::bind(&ChatServer::onMessage,this,_1,_2,_3));
    //设置线程数量
    //一个主reactor 负责新用户的连接 三个子reactor 负责已连接用户的读写事件的处理
    _server.setThreadNum(4);
}

//启动服务
void ChatServer:: start()
{
    _server.start();
}

//与客户端连接相关的回调函数
void ChatServer:: onConnection(const TcpConnectionPtr&conn)
{
    //客户端断开连接
    if(!conn->connected())
    {   
        ChatService::getInstance()->clientCloseException(conn);
        conn->shutdown();
    }
}

//与读写事件相关的回调函数
void ChatServer:: onMessage(const TcpConnectionPtr&conn,Buffer*buffer,Timestamp time)
{
    string buf=buffer->retrieveAllAsString();
    //数据的反序列化
    json js=json::parse(buf);
    //通过js["msg_id"]获取一个业务处理器handler(与网络模块分离)
    //目的: 完全解耦网络模块的代码和业务模块的代码 

    //通过回调获取已绑定的消息处理器 处理相对应的业务
    auto msgHandler=ChatService::getInstance()->getHandler(js["msgid"].get<int>());
    msgHandler(conn,js,time); 
}