#ifndef GROUPUSER_H
#define GROUPUSER_H

#include "user.hpp"

//群组成员信息直接继承User类 添加成员变量role即可
class GroupUser:public User
{
public:
    void setRole(string role) { this->role=role; }
    string getRole() { return this->role; }
private:
    string role;
};

#endif