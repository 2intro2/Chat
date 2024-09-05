#include "groupmodel.hpp"
#include "db.hpp"

//创建群组
bool GroupModel:: createGroup(Group& group)
{
    //1.组合sql语句
    char sql[1024]="";
    sprintf(sql,"insert into AllGroup(groupname,groupdesc) values('%s','%s')",
    group.getName().c_str(),group.getDesc().c_str());

    //2.连接数据库进行操作
    MySQL mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {   
            //获取插入数据的主键id
            group.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }

    return false;
}

//加入群组
void GroupModel:: joinGroup(int userid,int groupid,string role)
{
    //1.组合sql语句
    char sql[1024]="";
    sprintf(sql,"insert into GroupUser values(%d, %d,'%s')",
    groupid,userid,role.c_str());

    //2.连接数据库进行操作
    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql);
    }
}

//查询用户所在群组每个群组的信息
vector<Group> GroupModel:: queryGroups(int userid)
{
    /*
      1.先根据userid查出该用户的所有群组的群组信息
      2.再更具每个群组id查出每个群组所有成员的信息
    */

    //1.组合sql语句
    char sql[1024]="";
    sprintf(sql,"select a.id,a.groupname,a.groupdesc from AllGroup a inner join GroupUser b \
    on a.id=b.groupid where b.userid=%d",userid);

    //2.连接数据库进行操作
    vector<Group> groupVec;
    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES* res=mysql.query(sql);
        //查询操作成功
        if(res!=nullptr)
        {
            //将该用户的所有群组消息存储到groupVec中
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr)
            {   
                Group group;
                group.setId(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                groupVec.push_back(group);
            }
            mysql_free_result(res);
        }
    }

    //查询每个群组的群员信息
    for(Group& group:groupVec)
    {
        sprintf(sql,"select a.id,a.name,a.state,b.grouprole from User a inner join GroupUser b \
        on a.id=b.userid where b.groupid=%d",group.getId());

        MYSQL_RES* res=mysql.query(sql); 
        //查询操作成功
        if(res!=nullptr)
        {
            //将该用户的所有群组消息存储到groupVec中
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr)
            {   
                GroupUser user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                user.setRole(row[3]);
                group.getUsers().push_back(user);
            }
            mysql_free_result(res);
        } 
    }

    return groupVec;
}


//查询指定群组的所有群员id(除userid外)
vector<int> GroupModel:: queryGroupUsers(int userid,int groupid)
{
    //1.组合sql语句
    char sql[1024]="";
    sprintf(sql,"select userid from GroupUser where userid !=%d and groupid= %d",userid,groupid);

    //2.连接数据库进行操作
    vector<int> idVec;
    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES* res=mysql.query(sql);
        //查询操作成功
        if(res!=nullptr)
        {
            //将该群组所有用户id存储到idVec中(除userid外)
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr)
            {   
                idVec.push_back(atoi(row[0]));
            }
            mysql_free_result(res);
        }
    }

    return idVec;
}
