#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include "json.hpp"
#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include "redis.hpp"

#include <muduo/net/TcpServer.h>
#include <unordered_map>
#include <functional>
#include <mutex>

using namespace std;
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;
//表示处理消息的回调方法的类型
using MsgHandler = std::function<void(const TcpConnectionPtr&,json&,Timestamp)>; 

//聊天服务器的业务功能类
class ChatService
{
public:
    //全局节点(获取单例)
    static ChatService* getInstance();
    //获取特定消息对应的处理器
    MsgHandler getHandler(int msgid);

    //处理登录业务
    void login(const TcpConnectionPtr& conn,json& js,Timestamp time);
    //处理注销业务
    void loginOut(const TcpConnectionPtr& conn,json& js,Timestamp time);
    //处理注册业务
    void regist(const TcpConnectionPtr& conn,json& js,Timestamp time);
    //处理聊天(1对1)业务
    void chatOto(const TcpConnectionPtr& conn,json& js,Timestamp time); 

    //添加好友业务
    void addFriend(const TcpConnectionPtr& conn,json& js,Timestamp time);
    //创建群组业务
    void createGroup(const TcpConnectionPtr& conn,json& js,Timestamp time);
    //加入群组业务
    void joinGroup(const TcpConnectionPtr& conn,json& js,Timestamp time);
    //群组聊天业务
    void groupChat(const TcpConnectionPtr& conn,json& js,Timestamp time);

    //处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr& conn);
    //处理服务端强制退出(重置部分业务)   
    void reset();

    //从redis消息队列中获取订阅的消息
    void handleRedisSubsrcibeMessage(int userid,string msg);
    
private:
    //单例模式
    ChatService();

    //使用哈希表存储消息id与其对应的处理方法
    unordered_map<int,MsgHandler> _msgHandlerMap;
    
    //存储在线用户的通信连接
    unordered_map<int,TcpConnectionPtr> _userConnectionMap;

    //定义互斥锁 保证_userConnectionMap的线程安全(防止多个线程抢占资源)
    mutex _connMutex; 

    //数据层操作对象
    UserModel _userModel;
    OfflineMsgModel _offlineMsgModel;
    FriendModel _friendModel;
    GroupModel _groupModel;

    //redis操作对象
    Redis _redis;
};

#endif