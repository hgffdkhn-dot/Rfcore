#include "RFCoreService.h"
#include <android/binder_ibinder.h> 
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

// 🚨 独立版新增：系统底层调用必需的头文件
#include <unistd.h>
#include <sys/types.h>

// 引入我们的 SQLite 大脑中枢
#include "AuthDatabase.h"

namespace aidl {
namespace rfcore {
namespace daemon {

RFCoreService::RFCoreService() {}
RFCoreService::~RFCoreService() {}

// =========================================================
// 🛡️ 核心鉴权：动态解析 packages.list
// =========================================================
bool RFCoreService::isManagerApp(uid_t uid) const {
    if (uid == 0) return true; // Root 自身绝对放行
    
    std::ifstream file("/data/system/packages.list");
    if (!file.is_open()) return false;
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string pkgName;
        int pkgUid;
        if (iss >> pkgName >> pkgUid) {
            if (pkgUid == uid && pkgName == "com.rfcore.manager") return true;
        }
    }
    return false;
}

// =========================================================
// 📞 注册通信通道
// =========================================================
ndk::ScopedAStatus RFCoreService::registerAuthCallback(const std::shared_ptr<IAuthCallback>& in_callback) {
    uid_t caller_uid = AIBinder_getCallingUid();
    if (!isManagerApp(caller_uid)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_SECURITY);
    }
    
    std::lock_guard<std::mutex> lock(mCallbackMutex);
    mAuthCallback = in_callback;
    return ndk::ScopedAStatus::ok();
}

// =========================================================
// ⏳ 时间停止黑魔法：挂起底层线程，呼叫前台弹窗
// =========================================================
int RFCoreService::triggerAuthIntercept(int target_uid, const std::string& processName, const std::string& capability) {
    std::shared_ptr<IAuthCallback> callback;
    {
        std::lock_guard<std::mutex> lock(mCallbackMutex);
        callback = mAuthCallback;
    }
    
    if (callback != nullptr) {
        int32_t userDecision = 0;
        ndk::ScopedAStatus status = callback->onAuthRequested(target_uid, processName, capability, &userDecision);
        if (status.isOk()) return userDecision;
    }
    return 0; 
}

// =========================================================
// 🚀 独立版终极核心：接收 su 提权、查数据库、派生 Root Shell
// =========================================================
ndk::ScopedAStatus RFCoreService::requestAuth(const AuthRequest& request, AuthResult* _aidl_return) {
    uid_t target_uid = AIBinder_getCallingUid();
    
    // 1. 管理器自身直接放行
    if (isManagerApp(target_uid)) {
        _aidl_return->isGranted = true;
        return ndk::ScopedAStatus::ok();
    }

    // 2. 去大脑(SQLite)里查记忆
    int decision = AuthDatabase::getInstance().getPolicy(target_uid, request.capability);

    // 3. 没见过的新请求，挂起并呼叫前台弹窗！
    if (decision == -1) {
        decision = triggerAuthIntercept(target_uid, request.processName, request.capability);
        AuthDatabase::getInstance().setPolicy(target_uid, request.processName, request.capability, decision);
    }

    // 将授权结果写入返回值
    _aidl_return->isGranted = (decision == 1);

    // ============================================================
    // ⚡ 上帝之手：如果授权通过，立刻进行细胞分裂，派生 Root 进程！
    // ============================================================
    if (decision == 1) {
        pid_t pid = fork();
        
        if (pid == 0) {
            // -------- 子进程区域 (未来的 Root Shell) --------
            
            // 夺取绝对的 Root 权限
            setresuid(0, 0, 0);
            setresgid(0, 0, 0);

            // 移花接木：把 App (比如 MT 管理器) 寄过来的包裹里的管子，插到自己身上
            // 这样 Shell 的输出就会直接显示在 App 的屏幕上！
            if (request.fdIn.get() >= 0) dup2(request.fdIn.get(), STDIN_FILENO);
            if (request.fdOut.get() >= 0) dup2(request.fdOut.get(), STDOUT_FILENO);
            if (request.fdErr.get() >= 0) dup2(request.fdErr.get(), STDERR_FILENO);

            // 组装要执行的参数
            std::vector<const char*> args;
            args.push_back("/system/bin/sh"); // 永远以 Shell 身份启动
            
            for (const auto& arg : request.command) {
                args.push_back(arg.c_str());
            }
            args.push_back(nullptr); // C 语言要求数组必须以 nullptr 结尾

            // 蜕变！将当前子进程的肉体，直接替换为 /system/bin/sh！
            execvp(args[0], const_cast<char* const*>(args.data()));
            
            // 如果 execvp 失败（比如找不到 sh），默默死亡，不留僵尸进程
            exit(1); 
            // ----------------------------------------------
        }
        // 父进程（rfcore_daemon 本体）继续运行，等待下一个提权请求
    }

    return ndk::ScopedAStatus::ok();
}

// =========================================================
// 🎯 测试接口：配合前台 App 的紫色测试按钮
// =========================================================
ndk::ScopedAStatus RFCoreService::getStatus(int32_t* _aidl_return) {
    int target_uid = 10443;
    std::string pkg = "com.malicious.spy";
    std::string cap = "CAP_ROOT_SHELL";

    int decision = AuthDatabase::getInstance().getPolicy(target_uid, cap);
    if (decision == -1) {
        decision = triggerAuthIntercept(target_uid, pkg, cap); 
        AuthDatabase::getInstance().setPolicy(target_uid, pkg, cap, decision);
    }

    *_aidl_return = decision; 
    return ndk::ScopedAStatus::ok();
}

// =========================================================
// 📊 拉取全量策略 (给前台 Compose UI 渲染列表用)
// =========================================================
ndk::ScopedAStatus RFCoreService::getPolicies(std::vector<PolicyRecord>* _aidl_return) {
    *_aidl_return = AuthDatabase::getInstance().getAllPolicies();
    return ndk::ScopedAStatus::ok();
}

// =========================================================
// 留空的老接口 (防止 AIDL 报错)
// =========================================================
ndk::ScopedAStatus RFCoreService::requestCapability(const CapabilityRequest& request, CapabilityResult* _aidl_return) { return ndk::ScopedAStatus::ok(); }
ndk::ScopedAStatus RFCoreService::grantCapability(int32_t in_uid, const std::string& in_packageName, const std::string& in_capability, int32_t in_isGranted, int64_t in_expiresAt, bool* _aidl_return) { *_aidl_return = true; return ndk::ScopedAStatus::ok(); }
ndk::ScopedAStatus RFCoreService::isCapabilityGranted(const std::string& in_capability, bool* _aidl_return) { *_aidl_return = false; return ndk::ScopedAStatus::ok(); }
ndk::ScopedAStatus RFCoreService::revokeCapability(int32_t in_uid, const std::string& in_capability, bool* _aidl_return) { *_aidl_return = true; return ndk::ScopedAStatus::ok(); }
ndk::ScopedAStatus RFCoreService::getAuditLogs(int32_t in_limit, int32_t in_offset, std::vector<AuditRecord>* _aidl_return) { return ndk::ScopedAStatus::ok(); }

}  // namespace daemon
}  // namespace rfcore
}  // namespace aidl
