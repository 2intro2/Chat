#include "chatserver.hpp"
#include "chatservice.hpp"
#include <iostream>
#include <signal.h>
using namespace std;

//通过信号处理服务器(crtl+c)强制结束后 用户在线状态异常的问题
void resetHandler(int)
{
    ChatService::getInstance()->reset();
    exit(0);     
}

int main(int argc,char **argv)
{    
    if(argc<3)
    {
        cerr<<"command invalid!"<<endl;
        exit(-1);
    }
    signal(SIGINT,resetHandler);

    char* ip=argv[1];
    uint16_t port=atoi(argv[2]);

    EventLoop loop;
    InetAddress addr(ip,port);
    ChatServer server(&loop,addr,"ChatServer");

    server.start();
    loop.loop();
    
    return 0;
}