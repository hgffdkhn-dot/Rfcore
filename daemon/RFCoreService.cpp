#include "RFCoreService.h"
#include <android/binder_ibinder.h> 
#include <fstream>
#include <sstream>
#include <iostream>

// 🚨 引入我们刚刚造好的大脑
#include "AuthDatabase.h"

namespace aidl {
namespace rfcore {
namespace daemon {

RFCoreService::RFCoreService() {}
RFCoreService::~RFCoreService() {}

// 动态鉴权
bool RFCoreService::isManagerApp(uid_t uid) const {
    if (uid == 0) return true; 
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

// 注册回调通道
ndk::ScopedAStatus RFCoreService::registerAuthCallback(const std::shared_ptr<IAuthCallback>& in_callback) {
    uid_t caller_uid = AIBinder_getCallingUid();
    if (!isManagerApp(caller_uid)) return ndk::ScopedAStatus::fromExceptionCode(EX_SECURITY);
    
    std::lock_guard<std::mutex> lock(mCallbackMutex);
    mAuthCallback = in_callback;
    return ndk::ScopedAStatus::ok();
}

// 时间停止挂起与呼叫
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
// 🎯 进化版：带数据库记忆的测试按钮
// =========================================================
ndk::ScopedAStatus RFCoreService::getStatus(int32_t* _aidl_return) {
    int target_uid = 10443;
    std::string pkg = "com.malicious.spy";
    std::string cap = "CAP_ROOT_SHELL";

    // 1. 去大脑里翻看记忆
    int decision = AuthDatabase::getInstance().getPolicy(target_uid, cap);

    // 2. 如果等于 -1，说明是第一次遇见，必须弹窗问用户
    if (decision == -1) {
        decision = triggerAuthIntercept(target_uid, pkg, cap); // 卡住弹窗
        
        // 3. 用户点完后，把决定写进数据库，下次就不弹了！
        AuthDatabase::getInstance().setPolicy(target_uid, pkg, cap, decision);
    }

    *_aidl_return = decision; 
    return ndk::ScopedAStatus::ok();
}

// =========================================================
// 🚀 进化版：真实拉取数据库列表供前台渲染
// =========================================================
ndk::ScopedAStatus RFCoreService::getPolicies(std::vector<PolicyRecord>* _aidl_return) {
    // 之前这里返回的是空列表，现在直接从数据库拉取全部策略！
    *_aidl_return = AuthDatabase::getInstance().getAllPolicies();
    return ndk::ScopedAStatus::ok();
}

// 其他留空方法保持不变 (防止报错)
ndk::ScopedAStatus RFCoreService::requestCapability(const CapabilityRequest& request, CapabilityResult* _aidl_return) { return ndk::ScopedAStatus::ok(); }
ndk::ScopedAStatus RFCoreService::grantCapability(int32_t in_uid, const std::string& in_packageName, const std::string& in_capability, int32_t in_isGranted, int64_t in_expiresAt, bool* _aidl_return) { *_aidl_return = true; return ndk::ScopedAStatus::ok(); }
ndk::ScopedAStatus RFCoreService::isCapabilityGranted(const std::string& in_capability, bool* _aidl_return) { *_aidl_return = false; return ndk::ScopedAStatus::ok(); }
ndk::ScopedAStatus RFCoreService::revokeCapability(int32_t in_uid, const std::string& in_capability, bool* _aidl_return) { *_aidl_return = true; return ndk::ScopedAStatus::ok(); }
ndk::ScopedAStatus RFCoreService::getAuditLogs(int32_t in_limit, int32_t in_offset, std::vector<AuditRecord>* _aidl_return) { return ndk::ScopedAStatus::ok(); }

}  // namespace daemon
}  // namespace rfcore
}  // namespace aidl

