#ifndef PUBLIC_H
#define PUBLIC_H

//server和client的公共文件 
enum EnMsgType
{
    LOGIN_MSG=1,    //登录
    LOGIN_MSG_ACK,  //登录响应
    LOGIN_OUT_MSG,  //注销
    REGIST_MSG,     //注册
    REGIST_MSG_ACK, //注册响应
    CHAT_OTO_MSG,   //聊天(1对1)
    ADD_FRIEND_MSG, //添加好友
    CREATE_GROUP_MSG, //创建群组
    JOIN_GROUP_MSG, //加入群组
    GROUP_CHAT_MSG, //群组聊天
};

#endif