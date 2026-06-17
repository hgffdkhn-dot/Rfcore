#include "RFCoreService.h"
#include <android/binder_ibinder.h> 
#include <fstream>
#include <sstream>
#include <iostream>

// 🚨 引入我们的 SQLite 大脑中枢
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
// 🚨 终极核心：专门接收 su 二进制文件的提权呼叫
// =========================================================
ndk::ScopedAStatus RFCoreService::requestAuth(const AuthRequest& request, AuthResult* _aidl_return) {
    uid_t target_uid = AIBinder_getCallingUid();
    
    // 1. 如果是我们自己的管理器去调用 su，直接放行
    if (isManagerApp(target_uid)) {
        _aidl_return->isGranted = true;
        return ndk::ScopedAStatus::ok();
    }

    // 2. 去大脑(SQLite)里查一下，之前这个 UID 有没有点过允许或拒绝？
    int decision = AuthDatabase::getInstance().getPolicy(target_uid, request.capability);

    // 3. 如果等于 -1，说明是从未见过的新请求，立即挂起并呼叫前台弹窗！
    if (decision == -1) {
        decision = triggerAuthIntercept(target_uid, request.processName, request.capability);
        
        // 4. 弹窗结束，把你在前台的选择，死死刻进 SQLite 数据库里！
        AuthDatabase::getInstance().setPolicy(target_uid, request.processName, request.capability, decision);
    }

    // 5. 把最终结果回传给 su 信使
    _aidl_return->isGranted = (decision == 1);
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
// 留空的老接口 (留着不删，防止 AIDL 报错)
// =========================================================
ndk::ScopedAStatus RFCoreService::requestCapability(const CapabilityRequest& request, CapabilityResult* _aidl_return) { return ndk::ScopedAStatus::ok(); }
ndk::ScopedAStatus RFCoreService::grantCapability(int32_t in_uid, const std::string& in_packageName, const std::string& in_capability, int32_t in_isGranted, int64_t in_expiresAt, bool* _aidl_return) { *_aidl_return = true; return ndk::ScopedAStatus::ok(); }
ndk::ScopedAStatus RFCoreService::isCapabilityGranted(const std::string& in_capability, bool* _aidl_return) { *_aidl_return = false; return ndk::ScopedAStatus::ok(); }
ndk::ScopedAStatus RFCoreService::revokeCapability(int32_t in_uid, const std::string& in_capability, bool* _aidl_return) { *_aidl_return = true; return ndk::ScopedAStatus::ok(); }
ndk::ScopedAStatus RFCoreService::getAuditLogs(int32_t in_limit, int32_t in_offset, std::vector<AuditRecord>* _aidl_return) { return ndk::ScopedAStatus::ok(); }

}  // namespace daemon
}  // namespace rfcore
}  // namespace aidl
