#include "RFCoreService.h"
#include <android/binder_ibinder.h> 
#include <fstream>
#include <sstream>
#include <iostream>

namespace aidl {
namespace rfcore {
namespace daemon {

RFCoreService::RFCoreService() {}
RFCoreService::~RFCoreService() {}

// =========================================================
// 🛡️ 核心鉴权：动态解析 packages.list
// =========================================================
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
// 🎯 测试接口：配合前台 App 的紫色测试按钮
// =========================================================
ndk::ScopedAStatus RFCoreService::getStatus(int32_t* _aidl_return) {
    // 终极测试魔法：主动触发跨进程拦截弹窗
    int userDecision = triggerAuthIntercept(10443, "com.malicious.spy", "CAP_ROOT_SHELL");
    *_aidl_return = userDecision; 
    return ndk::ScopedAStatus::ok();
}

// =========================================================
// 📦 业务数据接口 (🚨 修复区：直接留空，防止找不到字段报错)
// =========================================================
ndk::ScopedAStatus RFCoreService::requestCapability(const CapabilityRequest& request, CapabilityResult* _aidl_return) { 
    // 暂时留空，防止你的 AIDL 缺少字段导致编译崩溃
    return ndk::ScopedAStatus::ok(); 
}

ndk::ScopedAStatus RFCoreService::getPolicies(std::vector<PolicyRecord>* _aidl_return) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus RFCoreService::grantCapability(int32_t in_uid, const std::string& in_packageName, const std::string& in_capability, int32_t in_isGranted, int64_t in_expiresAt, bool* _aidl_return) {
    *_aidl_return = true;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus RFCoreService::isCapabilityGranted(const std::string& in_capability, bool* _aidl_return) { 
    *_aidl_return = false;
    return ndk::ScopedAStatus::ok(); 
}

ndk::ScopedAStatus RFCoreService::revokeCapability(int32_t in_uid, const std::string& in_capability, bool* _aidl_return) { 
    *_aidl_return = true;
    return ndk::ScopedAStatus::ok(); 
}

ndk::ScopedAStatus RFCoreService::getAuditLogs(int32_t in_limit, int32_t in_offset, std::vector<AuditRecord>* _aidl_return) { 
    return ndk::ScopedAStatus::ok(); 
}

}  // namespace daemon
}  // namespace rfcore
}  // namespace aidl

