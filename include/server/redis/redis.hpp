#ifndef REDIS_H
#define REDIS_H

/* 
一台服务器上的redis需要处理多个隧道的消息推送和接收
*/
#include <hiredis/hiredis.h>
#include <thread>
#include <functional>
using namespace std;

class Redis
{
public:
    Redis();
    ~Redis();

    //连接redis服务器
    bool connect();
    //在redis中指定的channel发布消息
    bool publish(int channel,string message);
    //在redis中指定的channel订阅消息
    bool subscribe(int channel);
    //在redis中指定的channel取消订阅消息
    bool unsubscribe(int channel);
    //在独立线程中接收订阅通道中的消息(避免服务器work线程被阻塞)
    void observer_channel_message();
    //初始化_notify_message_handler
    void init_notify_handler(function<void(int,string)> fn);
private:
    //hiredis客户端同步上下文对象 负责publish消息
    redisContext *_publish_context;
    //hiredis客户端同步上下文对象 负责subscribe消息
    redisContext *_subscribe_context;
    //回调操作 收到订阅消息就给service层上报
    function<void(int,string)> _notify_message_handler;
};

#endif