#pragma once

#include <aidl/rfcore/daemon/BnRFCoreService.h>
#include <android/binder_ibinder.h>
#include <sys/types.h>
#include <string>
#include <vector>

namespace rfcore {
namespace daemon {

class RFCoreService : public aidl::rfcore::daemon::BnRFCoreService {
public:
    RFCoreService();
    ~RFCoreService() override = default;

    ndk::ScopedAStatus requestCapability(const aidl::rfcore::daemon::CapabilityRequest& in_request,
                                         aidl::rfcore::daemon::CapabilityResult* _aidl_return) override;

    ndk::ScopedAStatus isCapabilityGranted(const std::string& in_capability, bool* _aidl_return) override;
    
    ndk::ScopedAStatus getStatus(int32_t* _aidl_return) override;

    ndk::ScopedAStatus grantCapability(int32_t in_uid, const std::string& in_packageName, const std::string& in_capability, int32_t in_isGranted, int64_t in_expiresAt, bool* _aidl_return) override;

    ndk::ScopedAStatus revokeCapability(int32_t in_uid, const std::string& in_capability, bool* _aidl_return) override;

    ndk::ScopedAStatus getPolicies(std::vector<aidl::rfcore::daemon::PolicyRecord>* _aidl_return) override;

    ndk::ScopedAStatus getAuditLogs(int32_t in_limit, int32_t in_offset, std::vector<aidl::rfcore::daemon::AuditRecord>* _aidl_return) override;

private:
    uid_t getCallingUid() const;
    pid_t getCallingPid() const;
    bool isManagerApp(uid_t uid) const;
};

}
}
