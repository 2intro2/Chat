#ifndef USERMODEL_H
#define USERMODEL_H

#include"user.hpp"

//User表的数据操作类(数据层 与业务层无关)
class UserModel
{
public:

    //User表中进行添加
    bool insert(User& user);
    //User表中进行查询
    User query(int id);
    //User表中进行更新(用户在线状态)
    bool updateState(User user);
    //User表中重置用户的状态信息
    void resetState();
};
#endif 