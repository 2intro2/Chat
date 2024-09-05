/*
客户端 两个线程
main 负责向服务器发送消息
readTask 负责接收服务器的消息

服务器 四个线程
一个IO线程 负责客户端的连接
三个work线程 负责客户端的消息处理
网络层(muduo网络库负责 chatserver) 业务层(chatservice) 数据层(*model)

三层之间如何进行解耦
网络层 接收消息 通过消息类型 选择回调函数(回调操作) 进入业务层
业务层 不同的回调函数处理不同的业务 当需要修改数据时 通过表操作类进行修改表的操作 
数据层 复杂的表需要创建ORM类映射字段 每一张表都创建操作类进行操作 从而实现数据层与业务层的独立
*/

#include "json.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <ctime>
#include <mutex>
using namespace std;
using json=nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

//记录当前客户端登录的用户信息
User g_currentUser;
//记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
//记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;
//记录用户登录状态
bool isLogin=false;
//定义互斥锁 保证登录时main线程能够先于readTask线程获取服务器消息(通常用于第二次登录)
mutex _connMutex; 

//显示当前登录用户的基本信息
void showCurrentUserData();
//负责接收服务器消息的线程函数
void readTaskHandler(int clientfd);
//获取系统时间(聊天消息需要添加系统时间)
string getCurrentTime();
//聊天页面
void chatMenu(int clientfd);

//客户端程序实现 main线程主要向服务器发送消息 子线程主要处理接收服务器消息
int main(int argc,char **argv)
{   
    if(argc<3)
    {
        cerr<<"command invalid! ex: ./ChatClient 127.0.0.1 6666"<<endl;
        exit(-1);
    }

    //通过命令行参数获取服务器ip和端口
    char* ip=argv[1];
    uint16_t port= atoi(argv[2]);

    //创建与服务器进行通信的socket
    int clientfd=socket(AF_INET,SOCK_STREAM,0);
    if(clientfd==-1)
    {
        cerr<<"socket error!"<<endl;
        exit(-1);
    }

    //填写服务器信息
    sockaddr_in saddr;
    memset(&saddr,0,sizeof(saddr));

    saddr.sin_family=AF_INET;
    saddr.sin_addr.s_addr=inet_addr(ip);
    saddr.sin_port=htons(port);

    //连接服务器
    if(connect(clientfd,(sockaddr *)&saddr,sizeof(saddr))==-1)
    {
        cerr<<"connect error!"<<endl;
        close(clientfd);
        exit(-1);
    }

    while(1)
    {
        //显示首页菜单
        cout<<"=========================================="<<endl;
        cout<<"1.Login"<<endl;
        cout<<"2.Registr"<<endl;
        cout<<"3.Quit"<<endl;
        cout<<"=========================================="<<endl;
        cout<<"Please input:";
        int choice=0;
        cin>>choice; cin.get(); //读取掉缓冲区残留的回车

        switch (choice)
        {
        case 1: //Login业务
        {
            int id=0;
            char pwd[50]="";
            cout<<"id:"; cin>>id; cin.get();
            cout<<"password:"; cin.getline(pwd,50);

            cout<<endl;
            json js;
            js["msgid"]=LOGIN_MSG;
            js["id"]=id;
            js["password"]=pwd;
            string request=js.dump();

            unique_lock<mutex> lock(_connMutex);
            int len=send(clientfd,request.c_str(),strlen(request.c_str())+1,0);
            if(len==-1) 
            {
                cerr<<"send error!"<<endl;
                lock.unlock();
            }
            else
            {
                char buf[1024]="";
                len=recv(clientfd,buf,sizeof(buf),0);
                lock.unlock();
                if(len==-1) cerr<<"recv error!"<<endl;
                else
                {
                    json res=json::parse(buf);
                    if(res["errno"].get<int>()!=0) //登录失败
                    {
                        cerr<<res["errmsg"]<<endl;
                    }
                    else //登录成功
                    {
                        //记录当前用户的id和name
                        g_currentUser.setId(res["id"].get<int>());
                        g_currentUser.setName(res["name"]);

                        //记录当前用户的好友列表
                        if(res.contains("friendlist"))
                        {   
                            //初始化
                            g_currentUserFriendList.clear();

                            vector<string> vec=res["friendlist"];
                            for(string& str:vec)
                            {
                                json js=json::parse(str);
                                User user;
                                user.setId(js["id"].get<int>());
                                user.setName(js["name"]);
                                user.setState(js["state"]);

                                g_currentUserFriendList.push_back(user); 
                            }   
                        }
        
                        //记录当前用户的群组列表
                        if(res.contains("groups"))
                        {   
                            //初始化
                            g_currentUserGroupList.clear();

                            vector<string> vec1=res["groups"];
                            for(string &groupstr:vec1)
                            {
                                json grpjs=json::parse(groupstr);
                                Group group;
                                group.setId(grpjs["id"].get<int>());
                                group.setName(grpjs["groupname"]);
                                group.setDesc(grpjs["groupdesc"]);

                                vector<string> vec2=grpjs["users"];
                                for(string &userstr:vec2)
                                {
                                    json js=json::parse(userstr);
                                    GroupUser user;
                                    user.setId(js["id"].get<int>());
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    user.setRole(js["role"]);

                                    group.getUsers().push_back(user);
                                }
                                g_currentUserGroupList.push_back(group);
                            }
                        }

                        //显示当前用户的基本信息
                        showCurrentUserData();

                        //显示当前用户的离线信息
                        if(res.contains("offlinemsg"))
                        {
                            vector<string> vec=res["offlinemsg"];
                            for(string &str:vec)
                            {
                                json js=json::parse(str);
                                int msgType=js["msgid"].get<int>();
                                if(msgType==CHAT_OTO_MSG)
                                {
                                    cout<<js["time"].get<string>()<<" ["<<js["id"]<<"]"
                                    <<js["name"].get<string>()<<" said:"<<js["msg"].get<string>()<<endl;
                                }
                                else if(msgType==GROUP_CHAT_MSG)
                                {
                                    cout<<"群消息["<<js["groupid"]<<"]: "<<js["time"].get<string>()<<" ["<<js["id"]<<"]"
                                    <<js["name"].get<string>()<<" said:"<<js["msg"].get<string>()<<endl;
                                }

                            }
                        }
                        isLogin=true;

                        //登录成功 创建子线程负责接收服务器消息(只启动一次)
                        static int threadnum=0;
                        if(threadnum==0)
                        {
                            std::thread readTask(readTaskHandler,clientfd);
                            //回收资源
                            readTask.detach();
                            threadnum++;
                        }

                        //进入聊天界面
                        chatMenu(clientfd);
                    }
                }
            }

        }
        break;
        case 2: //Registr业务 
        {
            char name[50]="";
            char pwd[50]="";
            cout<<"username:"; cin.getline(name,50);
            cout<<"password:"; cin.getline(pwd,50);

            json js;
            js["msgid"]=REGIST_MSG;
            js["name"]=name;
            js["password"]=pwd;
            string request=js.dump();

            int len=send(clientfd,request.c_str(),strlen(request.c_str())+1,0);
            if(len==-1) cerr<<"send error!"<<endl;
            else
            {
                char buf[1024]="";
                len=recv(clientfd,buf,sizeof(buf),0);
                if(len==-1) cerr<<"recv error!"<<endl;
                else
                {
                    json res=json::parse(buf);
                    if(res["errno"].get<int>()!=0) //注册失败
                    {
                        cerr<<name<<" is already exist,regist error!"<<endl;
                    }
                    else //注册成功
                    {
                        cout<<name<<" registr success!"<<endl;
                        cout<<"ID"<<res["id"]<<endl;
                    }
                }
            }
        }
        break;
        case 3: //Quit业务
        {
            close(clientfd);
            exit(0);
        }
        default:
            cout<<"invalid input!"<<endl;
            break;
        }

    }

    return 0;
}

//显示当前登录用户的基本信息
void showCurrentUserData()
{
    cout<<"================Login User================"<<endl;
    cout<<"ID:"<<g_currentUser.getId()<<"    "<<"Name:"<<g_currentUser.getName()<<endl;
    cout<<"================Friend List==============="<<endl;
    if(!g_currentUserFriendList.empty())
    {
        for(User &user:g_currentUserFriendList)
        {
            cout<<"ID:"<<user.getId()<<"    "<<"Name:"<<user.getName()<<"    "<<"State:"<<user.getState()<<endl;
        }
    }
    cout<<"================Group List================"<<endl;
    if(!g_currentUserGroupList.empty())
    {
        for(Group &group:g_currentUserGroupList)
        {
            cout<<"ID:"<<group.getId()<<"    "<<"Name:"<<group.getName()<<"    "<<"Desc:"<<group.getDesc()<<endl;
            for(GroupUser &user:group.getUsers())
            {
                cout<<"ID:"<<user.getId()<<"    "<<"Name:"<<user.getName()<<"    "<<"State:"<<user.getState()
                <<"    "<<"Role:"<<user.getRole()<<endl;
            }
        }
    }
    cout<<"=========================================="<<endl;
}

//负责接收服务器消息的线程函数
void readTaskHandler(int clientfd)
{
    while(1)
    {
        char buf[1024]="";
        int len=recv(clientfd,buf,sizeof(buf),0);
        if(len==0||len==-1)
        {
            close(clientfd);
            exit(-1);
        }

        json js=json::parse(buf);
        int msgType=js["msgid"].get<int>();
        if(msgType==CHAT_OTO_MSG)
        {
            cout<<js["time"].get<string>()<<" ["<<js["id"]<<"]"
            <<js["name"].get<string>()<<" said:"<<js["msg"].get<string>()<<endl;
            continue;
        }
        else if(msgType==GROUP_CHAT_MSG)
        {
            cout<<"群消息["<<js["groupid"]<<"]: "<<js["time"].get<string>()<<" ["<<js["id"]<<"]"
            <<js["name"].get<string>()<<" said:"<<js["msg"].get<string>()<<endl;
            continue;
        }

    }
}

//处理函数
void help(int clientfd=-1,string str="");
void chat(int,string);
void addfriend(int,string);
void creategroup(int,string);
void addgroup(int,string);
void groupchat(int,string);
void loginout(int,string);

//客户端支持命令的帮助消息列表
unordered_map<string,string> commandMap =
{
    {"help","显示所有支持的命令 格式: help"},
    {"chat","一对一聊天 格式: chat:friendid:message"},
    {"addfriend","添加好友 格式: addfriend:friendid"},
    {"creategroup","创建群组 格式: creategroup:groupname:groupdesc"},
    {"addgroup","加入群组 格式: addgroup:groupid"},
    {"groupchat","群聊 格式: groupchat:groupid:message"},
    {"loginout","注销 格式: loginout"}

};

//绑定命令与其对应的处理函数
unordered_map<string,function<void(int,string)>> commandHandlerMap=
{
    {"help",help},
    {"chat",chat},
    {"addfriend",addfriend},
    {"creategroup",creategroup},
    {"addgroup",addgroup},
    {"groupchat",groupchat},
    {"loginout",loginout}

};

//聊天页面
void chatMenu(int clientfd)
{
    help();

    char buf[1024]="";
    while(isLogin)
    {
        cin.getline(buf,1024);
        string commandbuf(buf);
        string command; //存储命令

        int idx=commandbuf.find(":");
        if(idx==-1) command=commandbuf;
        else command=commandbuf.substr(0,idx);

        auto it=commandHandlerMap.find(command);
        if(it==commandHandlerMap.end())
        {
            cerr<<"command invalid"<<endl;
            continue;
        }

        //调用命令对应的处理函数
        it->second(clientfd,commandbuf.substr(idx+1,commandbuf.size()-idx));
    }
}

void help(int,string)
{
    cout<<"show commandlist >>>"<<endl;
    for(auto &p:commandMap)
    {
        cout<<p.first<<" "<<p.second<<endl;
    }cout<<endl;    
}

void chat(int clientfd,string str)
{
    int idx=str.find(":");
    if(idx==-1)
    {
        cerr<<"chat command invalid!"<<endl;
        return;
    }

    int friendid=atoi(str.substr(0,idx).c_str());
    string msg=str.substr(idx+1,str.size()-idx);

    json js;
    js["msgid"]=CHAT_OTO_MSG;
    js["id"]=g_currentUser.getId();
    js["name"]=g_currentUser.getName();
    js["toid"]=friendid;
    js["msg"]=msg;
    js["time"]=getCurrentTime();
    string buffer=js.dump();

    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);  
    if(len==-1) cerr<<"chat send error!"<<endl;
}

void addfriend(int clientfd,string str)
{
    int friendid=atoi(str.c_str());
    json js;
    js["msgid"]=ADD_FRIEND_MSG;
    js["id"]=g_currentUser.getId(); 
    js["friendid"]=friendid;
    string buffer=js.dump();

    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);  
    if(len==-1) cerr<<"addfriend send error!"<<endl;
}

void creategroup(int clientfd,string str)
{
    int idx=str.find(":");
    if(idx==-1)
    {
        cerr<<"creategroup command invalid!"<<endl;
        return;
    }

    string groupname=str.substr(0,idx);
    string groupdesc=str.substr(idx+1,str.size()-idx);
    
    json js;
    js["msgid"]=CREATE_GROUP_MSG;
    js["id"]=g_currentUser.getId();
    js["groupname"]=groupname;
    js["groupdesc"]=groupdesc;
    string buffer=js.dump();

    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);  
    if(len==-1) cerr<<"creategroup send error!"<<endl;

}

void addgroup(int clientfd,string str)
{
    int groupid=atoi(str.c_str());

    json js;
    js["msgid"]=JOIN_GROUP_MSG;
    js["id"]=g_currentUser.getId();
    js["groupid"]=groupid;
    string buffer=js.dump();

    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);  
    if(len==-1) cerr<<"addgroup send error!"<<endl;
}

void groupchat(int clientfd,string str)
{   
    int idx=str.find(":");
    if(idx==-1)
    {
        cerr<<"groupchat command invalid!"<<endl;
        return;
    }
    int groupid=atoi(str.substr(0,idx).c_str());
    string msg=str.substr(idx+1,str.size()-idx);    

    json js;
    js["msgid"]=GROUP_CHAT_MSG;
    js["id"]=g_currentUser.getId();
    js["name"]=g_currentUser.getName();
    js["groupid"]=groupid;
    js["msg"]=msg;
    js["time"]=getCurrentTime();
    string buffer=js.dump();

    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);  
    if(len==-1) cerr<<"groupchat send error!"<<endl;
}

void loginout(int clientfd,string str)
{
    json js;
    js["msgid"]=LOGIN_OUT_MSG,
    js["id"]=g_currentUser.getId();
    string buffer=js.dump();

    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);  
    if(len==-1) cerr<<"loginout send error!"<<endl;
    else isLogin=false;
}

//获取系统时间(聊天消息需要添加系统时间)
string getCurrentTime()
{
    auto tt=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm=localtime(&tt);
    char date[60]="";
    sprintf(date,"%d-%02d-%02d %02d:%02d:%02d",
    (int)ptm->tm_year+1900,(int)ptm->tm_mon+1,(int)ptm->tm_mday,
    (int)ptm->tm_hour,(int)ptm->tm_min,(int)ptm->tm_sec);
    return std::string(date);
}