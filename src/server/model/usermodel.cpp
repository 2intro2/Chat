#include "usermodel.hpp"
#include "db.hpp"
#include<iostream>
using namespace std; 

//User表的增加方法
bool UserModel:: insert(User& user)
{
    //1.组合sql语句
    char sql[1024]="";
    sprintf(sql,"insert into User(name,password,state) values('%s','%s','%s')",
    user.getName().c_str(),user.getPwd().c_str(),user.getState().c_str());

    //2.连接数据库进行操作
    MySQL mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {   
            //获取插入数据的主键id
            user.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }

    return false;
}

//User表中进行查询
User UserModel::query(int id)
{
    //1.组合sql语句
    char sql[1024]="";
    sprintf(sql,"select * from User where id=%d",id);

    //2.连接数据库进行操作
    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES* res=mysql.query(sql);
        //查询操作成功
        if(res!=nullptr)
        {
            MYSQL_ROW row=mysql_fetch_row(res);
            //获取查询结果成功
            if(row!=nullptr)
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setPwd(row[2]);
                user.setState(row[3]);

                //释放资源
                mysql_free_result(res);
                return user;
            }
        }
    }

    //查询异常
    return User();
}

//User表中进行更新(用户在线状态)
bool UserModel:: updateState(User user)
{
    //1.组合sql语句
    char sql[1024]="";
    sprintf(sql,"update User set state='%s' where id=%d",user.getState().c_str(),user.getId());

    //2.连接数据库进行操作
    MySQL mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {   
            return true;
        }
    }

    return false;
}

//重置用户的状态信息
void UserModel:: resetState()
{
    //1.组合sql语句
    char sql[1024]="update User set state='offline' where state='online'";

    //2.连接数据库进行操作
    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql);
    }
}