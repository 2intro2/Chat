#include "friendmodel.hpp"
#include "db.hpp"

//添加好友关系
void FriendModel:: insert(int userid,int friendid)
{
    //1.组合sql语句
    char sql[1024]="";
    sprintf(sql,"insert into Friend values(%d,%d)",userid,friendid);

    //2.连接数据库进行操作
    MySQL mysql;
    if(mysql.connect())
    {
       mysql.update(sql);
    }
}

//返回用户好友列表
vector<User> FriendModel:: query(int userid)
{
     //1.组合sql语句
    char sql[1024]="";
    sprintf(sql,"select a.id,a.name,a.state from User a inner join Friend b on b.friendid=a.id where b.userid=%d",userid);

    //2.连接数据库进行操作
    MySQL mysql;
    vector<User> vec;
    if(mysql.connect())
    {
        MYSQL_RES* res=mysql.query(sql);
        //查询操作成功
        if(res!=nullptr)
        {
            //将该用户的好友信息存储到vec中并返回
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr)
            {   
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.push_back(user);
            }
            mysql_free_result(res);
            return vec;
        }
    }
    //查询异常
    return vec; 
}