#include "chatservice.hpp"
#include "public.hpp"

#include <muduo/base/Logging.h>
#include <vector>
using namespace std;
using namespace muduo;

//全局节点(获取单例)
ChatService* ChatService:: getInstance()
{
    static ChatService service;
    return &service;
}

//将特定消息与对应的处理器进行绑定(回调操作)
ChatService:: ChatService()
{   
    //用户业务处理器
    _msgHandlerMap.insert({LOGIN_MSG,std::bind(&ChatService::login,this,_1,_2,_3)});
    _msgHandlerMap.insert({REGIST_MSG,std::bind(&ChatService::regist,this,_1,_2,_3)});
    _msgHandlerMap.insert({CHAT_OTO_MSG,std::bind(&ChatService::chatOto,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG,std::bind(&ChatService::addFriend,this,_1,_2,_3)});
    _msgHandlerMap.insert({LOGIN_OUT_MSG,std::bind(&ChatService::loginOut,this,_1,_2,_3)});

    //群组业务处理器
    _msgHandlerMap.insert({CREATE_GROUP_MSG,std::bind(&ChatService::createGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({JOIN_GROUP_MSG,std::bind(&ChatService::joinGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG,std::bind(&ChatService::groupChat,this,_1,_2,_3)});

    //连接redis服务器
    if(_redis.connect())
    {
        //绑定回调函数
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubsrcibeMessage,this,_1,_2));
    }
}

//获取特定消息对应的处理器
MsgHandler ChatService:: getHandler(int msgid)
{   
    //错误日志 msgid没有对应的事件处理器(返回默认的空处理器)
    auto it=_msgHandlerMap.find(msgid);
    if(it==_msgHandlerMap.end())
    {
        return [=](const TcpConnectionPtr& conn,json& js,Timestamp time)
        {
            LOG_ERROR<<"msg_id:"<<msgid<<" can not find handler!";
        };
    }
         
    return _msgHandlerMap[msgid];
}

//处理登录业务
void ChatService:: login(const TcpConnectionPtr& conn,json& js,Timestamp time)
{
    int id=js["id"].get<int>();
    string pwd=js["password"];

    User user=_userModel.query(id);

    if(user.getId()==id&&user.getPwd()==pwd)
    {   
        if(user.getState()=="online") //用户已经在线 不允许重复登录
        {
            json response;
            response["msgid"]=LOGIN_MSG_ACK;
            response["errno"]=2;
            response["errmsg"]="This account is using,input another!";
            conn->send(response.dump());
        }
        else
        {   
            //登录成功 

            //记录客户端连接信息
            {
                //利用智能指针自动释放锁
                lock_guard<mutex> lock(_connMutex);
                _userConnectionMap.insert({id,conn});
            }

            //用户登录成功后 以id向redis订阅channel
            _redis.subscribe(id);

            //更新用户在线状态   
            user.setState("online");
            _userModel.updateState(user);

            //响应给客户端
            json response;
            response["msgid"]=LOGIN_MSG_ACK;
            response["errno"]=0;
            response["id"]=user.getId();
            response["name"]=user.getName();
            response["msg"]="Login Success!";

            //查询该用户是否有离线消息未接收
            vector<string> vec=_offlineMsgModel.query(user.getId());
            if(!vec.empty())
            {
                response["offlinemsg"]=vec;
                //转发完离线消息后直接进行删除
                _offlineMsgModel.remove(user.getId());
            }

            //查询该用户的好友列表并返回
            vector<User> uservec=_friendModel.query(user.getId());
            if(!uservec.empty())
            {   
                vector<string> vec2;
                for(User& u:uservec)
                {
                    json js;
                    js["id"]=u.getId();
                    js["name"]=u.getName();
                    js["state"]=u.getState();
                    vec2.push_back(js.dump());
                }
                response["friendlist"]=vec2; 
            }

            //查询该用户的群组列表并返回
            vector<Group> groupVec=_groupModel.queryGroups(user.getId());
            if(!groupVec.empty())
            {
                vector<string> groupV;
                for(Group &group:groupVec)
                {
                    json grpjs;
                    grpjs["id"]=group.getId();
                    grpjs["groupname"]=group.getName();
                    grpjs["groupdesc"]=group.getDesc(); 
                    vector<string> userV;

                    for(GroupUser &user:group.getUsers())
                    {
                        json js;
                        js["id"]=user.getId();
                        js["name"]=user.getName();
                        js["state"]=user.getState();
                        js["role"]=user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjs["users"]=userV;
                    groupV.push_back(grpjs.dump());
                }

                response["groups"]=groupV;
            }

            conn->send(response.dump());
        }
    }
    else  //登录失败 用户不存在或密码错误       
    {
        json response;
        response["msgid"]=LOGIN_MSG_ACK;
        response["errno"]=1;
        response["errmsg"]="Id or password is invalid!";
        conn->send(response.dump());
    }
}

//处理注册业务
void ChatService:: regist(const TcpConnectionPtr& conn,json& js,Timestamp time)
{
    string name=js["name"];
    string pwd=js["password"];

    User user; user.setName(name); user.setPwd(pwd);
    bool state=_userModel.insert(user);

    //注册成功
    if(state)
    {   
        //响应给客户端
        json response;
        response["msgid"]=REGIST_MSG_ACK;
        response["errno"]=0;
        response["id"]=user.getId();
        response["msg"]="Regist Success!";
        conn->send(response.dump());
    }
    //注册失败
    else
    {
        json response;
        response["msgid"]=REGIST_MSG_ACK;
        response["errno"]=1;
        response["errmsg"]="Regist Fail!";
        conn->send(response.dump());
    }
}

//处理注销业务
void ChatService:: loginOut(const TcpConnectionPtr& conn,json& js,Timestamp time)
{
    int id=js["id"].get<int>();
    User user; user.setId(id);
    {
        lock_guard<mutex> lock(_connMutex);
        auto it=_userConnectionMap.find(id);
        if(it!=_userConnectionMap.end())
        {
            _userConnectionMap.erase(it);
        }
    }

    //用户注销后 在redis中取消订阅channel
    _redis.unsubscribe(id);

    //更新用户的在线状态
    user.setState("offline");
    _userModel.updateState(user);
}

//处理客户端异常退出
void ChatService:: clientCloseException(const TcpConnectionPtr& conn)
{

    User user;
    {
        //利用智能指针自动释放锁
        lock_guard<mutex> lock(_connMutex);
        for(auto it=_userConnectionMap.begin();it!=_userConnectionMap.end();++it)
        {
            if(it->second==conn)
            {   
                user.setId(it->first);
                //将异常退出客户端的连接删除
                _userConnectionMap.erase(it);
                break;
            }
        }
    }

    //异常退出 在redis中取消订阅channel
    _redis.unsubscribe(user.getId());

    //更新用户的在线状态
    user.setState("offline");
    _userModel.updateState(user);
}

//处理服务端强制退出(重置部分业务)  
void ChatService:: reset()
{
    //重置用户的在线状态
    _userModel.resetState();
    
}

//处理聊天(1对1)业务
void ChatService:: chatOto(const TcpConnectionPtr& conn,json& js,Timestamp time)
{
    int toid=js["toid"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it=_userConnectionMap.find(toid);

        //对方用户在线 服务器直接转发消息
        if(it!=_userConnectionMap.end())
        {
            it->second->send(js.dump());
            return;
        }
    }
    //查询toid是否在线
    User user=_userModel.query(toid);
    if(user.getState()=="online")
    {
        _redis.publish(toid,js.dump());
        return;
    }

    //对方用户不在线 存储离线消息
    _offlineMsgModel.insert(toid,js.dump());    
}

//添加好友业务
void ChatService:: addFriend(const TcpConnectionPtr& conn,json& js,Timestamp time)
{
    int userid=js["id"].get<int>();
    int friendid=js["friendid"].get<int>();

    //插入好友关系
    _friendModel.insert(userid,friendid);   
}

//创建群组业务
void ChatService:: createGroup(const TcpConnectionPtr& conn,json& js,Timestamp time)
{
    int userid=js["id"].get<int>();
    string groupname=js["groupname"];
    string groupdesc=js["groupdesc"];

    //创建新群组
    Group group(-1,groupname,groupdesc);
    if(_groupModel.createGroup(group))
    {
        //将群主信息添加到GroupUser表中
        _groupModel.joinGroup(userid,group.getId(),"creator");
    }

}

//加入群组业务
void ChatService:: joinGroup(const TcpConnectionPtr& conn,json& js,Timestamp time)
{
    int userid=js["id"].get<int>();
    int groupid=js["groupid"].get<int>();

    _groupModel.joinGroup(userid,groupid,"normal");

}

//群组聊天业务
void ChatService:: groupChat(const TcpConnectionPtr& conn,json& js,Timestamp time)
{
    int userid=js["id"].get<int>();
    int groupid=js["groupid"].get<int>();

    //获取群员id
    vector<int> userVec=_groupModel.queryGroupUsers(userid,groupid);    
    //依次转发消息
    lock_guard<mutex> lock(_connMutex);
    for(int id:userVec)
    {
        auto it=_userConnectionMap.find(id);
        if(it!=_userConnectionMap.end())
        {
            it->second->send(js.dump());
        }
        else
        {   
            //查询id是否在线
            User user=_userModel.query(id);
            if(user.getState()=="online")
            {
                _redis.publish(id,js.dump());
            }
            else
            {
                //存储离线消息
                _offlineMsgModel.insert(id,js.dump());
            }
        }
    }
}

void ChatService::handleRedisSubsrcibeMessage(int userid,string msg)
{
    lock_guard<mutex> lock(_connMutex);
    auto it=_userConnectionMap.find(userid);
    if(it!=_userConnectionMap.end())
    {
        it->second->send(msg);
        return;
    }

    _offlineMsgModel.insert(userid,msg);
}