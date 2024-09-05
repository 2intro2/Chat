#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include "group.hpp"
#include <string>
#include <vector>
using namespace std;

//ALLGroup表和GroupUser表的数据操作类(数据层 与业务层无关)
class GroupModel
{
public:
    //创建群组
    bool createGroup(Group& group);
    //加入群组
    void joinGroup(int userid,int groupid,string role);
    //查询用户所在群组每个群组的信息
    vector<Group> queryGroups(int userid);
    //查询指定群组的所有群员id(除userid外)
    vector<int> queryGroupUsers(int userid,int groupid);
};

#endif 