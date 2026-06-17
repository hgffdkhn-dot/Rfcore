#pragma once

#include <string>
#include <vector>
#include <mutex>
#include "sqlite3.h"

// 如果这里报错找不到 PolicyRecord，请改成你实际的相对路径，比如 #include <aidl/rfcore/daemon/PolicyRecord.h>
#include <aidl/rfcore/daemon/PolicyRecord.h>

namespace aidl {
namespace rfcore {
namespace daemon {

class AuthDatabase {
public:
    // 获取全局唯一的数据库实例 (单例模式)
    static AuthDatabase& getInstance();
    ~AuthDatabase();

    // 初始化数据库与表结构
    bool init();
    
    // 查询策略：返回 1 (允许), 0 (拒绝), -1 (未找到记录，说明是第一次遇到，需要弹窗)
    int getPolicy(int uid, const std::string& capability);
    
    // 保存策略：用户在屏幕上点击弹窗后，立刻把决定刻进数据库
    bool setPolicy(int uid, const std::string& packageName, const std::string& capability, int isGranted);
    
    // 获取所有策略 (给前台 App 里的 Compose 列表渲染用)
    std::vector<PolicyRecord> getAllPolicies();

private:
    AuthDatabase();
    sqlite3* db;
    std::mutex dbMutex;
};

}  // namespace daemon
}  // namespace rfcore
}  // namespace aidl
