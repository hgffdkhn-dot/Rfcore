#include "RFCoreService.h"
// 🚨 修正：删除了废弃的 manager.h，只保留获取 UID 需要的 ibinder.h
#include <android/binder_ibinder.h> 
#include <fstream>
#include <sstream>
#include <iostream>

namespace aidl {
namespace rfcore {
namespace daemon {

RFCoreService::RFCoreService() {}
RFCoreService::~RFCoreService() {}

// 动态鉴权：扫描 packages.list
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

ndk::ScopedAStatus RFCoreService::registerAuthCallback(const std::shared_ptr<IAuthCallback>& in_callback) {
    uid_t caller_uid = AIBinder_getCallingUid();
    if (!isManagerApp(caller_uid)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_SECURITY);
    }
    std::lock_guard<std::mutex> lock(mCallbackMutex);
    mAuthCallback = in_callback;
    return ndk::ScopedAStatus::ok();
}

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

ndk::ScopedAStatus RFCoreService::getStatus(int32_t* _aidl_return) {
    *_aidl_return = 1; 
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus RFCoreService::getPolicies(std::vector<PolicyRecord>* _aidl_return) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus RFCoreService::grantCapability(int32_t in_uid, const std::string& in_packageName, const std::string& in_capability, int32_t in_isGranted, int64_t in_expiresAt, bool* _aidl_return) {
    *_aidl_return = true;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus RFCoreService::requestCapability(const CapabilityRequest& request, CapabilityResult* _aidl_return) { return ndk::ScopedAStatus::ok(); }
ndk::ScopedAStatus RFCoreService::isCapabilityGranted(const std::string& in_capability, bool* _aidl_return) { return ndk::ScopedAStatus::ok(); }
ndk::ScopedAStatus RFCoreService::revokeCapability(int32_t in_uid, const std::string& in_capability, bool* _aidl_return) { return ndk::ScopedAStatus::ok(); }
ndk::ScopedAStatus RFCoreService::getAuditLogs(int32_t in_limit, int32_t in_offset, std::vector<AuditRecord>* _aidl_return) { return ndk::ScopedAStatus::ok(); }

}  // namespace daemon
}  // namespace rfcore
}  // namespace aidl

