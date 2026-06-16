#include "RFCoreService.h"
#include "PrivilegedWorker.h"
#include "PolicyDatabase.h"

#include <android/binder_ibinder.h>
#include <time.h>

namespace rfcore {
namespace daemon {

using aidl::rfcore::daemon::CapabilityRequest;
using aidl::rfcore::daemon::CapabilityResult;

RFCoreService::RFCoreService() {
}

uid_t RFCoreService::getCallingUid() const {
    return AIBinder_getCallingUid();
}

pid_t RFCoreService::getCallingPid() const {
    return AIBinder_getCallingPid();
}

bool RFCoreService::isManagerApp(uid_t uid) const {
    return (uid == 10329 || uid == 0); 
}

ndk::ScopedAStatus RFCoreService::requestCapability(const CapabilityRequest& in_request, CapabilityResult* _aidl_return) {
    uid_t caller_uid = getCallingUid();
    pid_t caller_pid = getCallingPid();
    long current_time = time(NULL);

    int grantStatus = PolicyDatabase::getInstance().checkCapability(caller_uid, in_request.capability);

    if (grantStatus != 1 && caller_uid != 2000 && !isManagerApp(caller_uid)) {
        _aidl_return->status = -1;
        _aidl_return->exitCode = -1;
        _aidl_return->message = "Permission Denied by Policy Engine";
        
        PolicyDatabase::getInstance().logAudit(caller_uid, in_request.capability, current_time, -1, "Denied");
        return ndk::ScopedAStatus::ok();
    }

    PrivilegedWorker::executeInSandbox(in_request, caller_pid, _aidl_return);
    
    PolicyDatabase::getInstance().logAudit(caller_uid, in_request.capability, current_time, _aidl_return->status, "Executed");

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus RFCoreService::isCapabilityGranted(const std::string& in_capability, bool* _aidl_return) {
    uid_t caller_uid = getCallingUid();
    int grantStatus = PolicyDatabase::getInstance().checkCapability(caller_uid, in_capability);
    *_aidl_return = (grantStatus == 1);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus RFCoreService::getStatus(int32_t* _aidl_return) {
    *_aidl_return = 1;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus RFCoreService::grantCapability(int32_t in_uid, const std::string& in_packageName, const std::string& in_capability, int32_t in_isGranted, int64_t in_expiresAt, bool* _aidl_return) {
    uid_t caller_uid = getCallingUid();
    if (!isManagerApp(caller_uid)) {
        *_aidl_return = false;
        return ndk::ScopedAStatus::ok();
    }
    *_aidl_return = PolicyDatabase::getInstance().grantCapability(in_uid, in_packageName, in_capability, in_isGranted, in_expiresAt);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus RFCoreService::revokeCapability(int32_t in_uid, const std::string& in_capability, bool* _aidl_return) {
    uid_t caller_uid = getCallingUid();
    if (!isManagerApp(caller_uid)) {
        *_aidl_return = false;
        return ndk::ScopedAStatus::ok();
    }
    *_aidl_return = PolicyDatabase::getInstance().revokeCapability(in_uid, in_capability);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus RFCoreService::getPolicies(std::vector<aidl::rfcore::daemon::PolicyRecord>* _aidl_return) {
    uid_t caller_uid = getCallingUid();
    if (isManagerApp(caller_uid)) {
        *_aidl_return = PolicyDatabase::getInstance().getPolicies();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus RFCoreService::getAuditLogs(int32_t in_limit, int32_t in_offset, std::vector<aidl::rfcore::daemon::AuditRecord>* _aidl_return) {
    uid_t caller_uid = getCallingUid();
    if (isManagerApp(caller_uid)) {
        *_aidl_return = PolicyDatabase::getInstance().getAuditLogs(in_limit, in_offset);
    }
    return ndk::ScopedAStatus::ok();
}

}
}
