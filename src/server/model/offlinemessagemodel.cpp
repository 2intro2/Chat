#include "offlinemessagemodel.hpp"
#include "db.hpp"

//存储用户的离线消息
void OfflineMsgModel:: insert(int userid,string msg )
{
    //1.组合sql语句
    char sql[1024]="";
    sprintf(sql,"insert into OfflineMessage(userid,message) values(%d,'%s')",userid,msg.c_str());

    //2.连接数据库进行操作
    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql);
    }
}
//删除用户的离线消息
void OfflineMsgModel:: remove(int userid)
{
    //1.组合sql语句
    char sql[1024]="";
    sprintf(sql,"delete from OfflineMessage where userid=%d",userid);

    //2.连接数据库进行操作
    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql);
    }
}
//查询用户的离线消息
vector<string> OfflineMsgModel:: query(int userid)
{
    //1.组合sql语句
    char sql[1024]="";
    sprintf(sql,"select message from OfflineMessage where userid=%d",userid);

    //2.连接数据库进行操作
    MySQL mysql;
    vector<string> vec;
    if(mysql.connect())
    {
        MYSQL_RES* res=mysql.query(sql);
        //查询操作成功
        if(res!=nullptr)
        {
            //将该用户的所有离线消息存储到vec中并返回
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr)
            {
                vec.push_back(row[0]);
            }
            mysql_free_result(res);
            return vec;
        }
    }

    //查询异常
    return vec;
}