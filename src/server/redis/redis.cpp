#include "redis.hpp"
#include <iostream>
using namespace std;

Redis:: Redis(): _publish_context(nullptr),_subscribe_context(nullptr)
{

}
Redis:: ~Redis()
{
    if(_publish_context!=nullptr) redisFree(_publish_context);
    if(_subscribe_context!=nullptr) redisFree(_subscribe_context);

}

//连接redis服务器
bool Redis:: connect()
{
    //客户端连接redis服务器
    _publish_context=redisConnect("127.0.0.1",6379);
    if(_publish_context==nullptr)
    {
        cerr<<"connect redis failed!"<<endl;
        return false;
    }
    _subscribe_context=redisConnect("127.0.0.1",6379);
    if(_subscribe_context==nullptr)
    {
        cerr<<"connect redis failed!"<<endl;
        return false;
    }

    //创建单独的线程用于监听通道发送的事件(避免服务器线程阻塞)
    thread t([&]()
    {
        observer_channel_message();
    });
    t.detach();

    cout<<"connect redis-server success!"<<endl;
    return true;    
}

//在redis中指定的channel发布消息
bool Redis:: publish(int channel,string message)
{
    redisReply *reply=(redisReply *)redisCommand(_publish_context,"PUBLISH %d %s",channel,message.c_str());
    if(reply==nullptr) 
    {
        cout<<"publish command failed"<<endl;
        return false;
    }

    freeReplyObject(reply);
    return true;
}

//在redis中指定的channel订阅消息
bool Redis:: subscribe(int channel)
{
    /*
        redisCommand=redisAppendCommand(将发送的命令缓存到本地)
        +redisBufferWrite(再将缓存的命令发送至redis-server进行处理)
        +redisGetReply(以阻塞的方式等待命令执行的结果)

        由于subsrcibe命令执行后会阻塞等待消息 这就会导致redisGetReply一直阻塞等待消息 
        从而导致线程阻塞 所以我们只执行前两步 即将命令发送给redis-server即可 后续的接收消息交给另一线程处理
    */

    if(redisAppendCommand(this->_subscribe_context,"SUBSCRIBE %d",channel)==REDIS_ERR)
    {
        cerr<<"subscribe command failed!"<<endl;
        return false;
    }
    int done=0;
    while(!done)
    {   
        //循环发送缓冲区数据 直到缓冲区数据发送完毕(done置为1)
        if(redisBufferWrite(this->_subscribe_context,&done)==REDIS_ERR)
        {
            cerr<<"subscribe command failed!"<<endl;
            return false;
        }
    }
    return true;

}

//在redis中指定的channel取消订阅消息
bool Redis:: unsubscribe(int channel)
{
    if(redisAppendCommand(this->_subscribe_context,"UNSUBSCRIBE %d",channel)==REDIS_ERR)
    {
        cerr<<"unsubscribe command failed!"<<endl;
        return false;
    }
    int done=0;
    while(!done)
    {   
        //循环发送缓冲区数据 直到缓冲区数据发送完毕(done置为1)
        if(redisBufferWrite(this->_subscribe_context,&done)==REDIS_ERR)
        {
            cerr<<"unsubscribe command failed!"<<endl;
            return false;
        }
    }
    return true;
}

//在独立线程中接收订阅通道中的消息(避免服务器work线程被阻塞)
void Redis:: observer_channel_message()
{
    redisReply* reply=nullptr;
    while(redisGetReply(this->_subscribe_context,(void **)&reply)==REDIS_OK)
    {
        //订阅收到的消息是含三个元素的数组
        if(reply!=nullptr && reply->element[2]!=nullptr && reply->element[2]->str !=nullptr)
        {
            //将消息上报给service层(回调操作)
            _notify_message_handler(atoi(reply->element[1]->str),reply->element[2]->str);
        }
        freeReplyObject(reply);
    }

    cerr<<"observer_channel_message quit!"<<endl;
}

//初始化_notify_message_handler
void Redis:: init_notify_handler(function<void(int,string)> fn)
{
    this->_notify_message_handler=fn;
}