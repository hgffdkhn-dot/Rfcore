#pragma once

#include <aidl/rfcore/daemon/BnRFCoreService.h>
#include <aidl/rfcore/daemon/IAuthCallback.h>
// 🚨 引入我们为 su 专门定制的最新暗号
#include <aidl/rfcore/daemon/AuthRequest.h>
#include <aidl/rfcore/daemon/AuthResult.h>

#include <memory>
#include <mutex>
#include <vector>
#include <string>

namespace aidl {
namespace rfcore {
namespace daemon {

class RFCoreService : public BnRFCoreService {
public:
    RFCoreService();
    ~RFCoreService();

    // ==========================================
    // 基础业务接口 (保留，防止 AIDL 报错)
    // ==========================================
    ndk::ScopedAStatus requestCapability(const CapabilityRequest& request, CapabilityResult* _aidl_return) override;
    ndk::ScopedAStatus isCapabilityGranted(const std::string& in_capability, bool* _aidl_return) override;
    ndk::ScopedAStatus getStatus(int32_t* _aidl_return) override;
    ndk::ScopedAStatus grantCapability(int32_t in_uid, const std::string& in_packageName, const std::string& in_capability, int32_t in_isGranted, int64_t in_expiresAt, bool* _aidl_return) override;
    ndk::ScopedAStatus revokeCapability(int32_t in_uid, const std::string& in_capability, bool* _aidl_return) override;
    ndk::ScopedAStatus getPolicies(std::vector<PolicyRecord>* _aidl_return) override;
    ndk::ScopedAStatus getAuditLogs(int32_t in_limit, int32_t in_offset, std::vector<AuditRecord>* _aidl_return) override;

    // ==========================================
    // 📞 核心回调：接听前台 Manager 递过来的电话线
    // ==========================================
    ndk::ScopedAStatus registerAuthCallback(const std::shared_ptr<IAuthCallback>& in_callback) override;

    // ==========================================
    // 🚨 核心通道：专门接收 su 二进制文件的提权请求
    // ==========================================
    ndk::ScopedAStatus requestAuth(const AuthRequest& request, AuthResult* _aidl_return) override;

    // ==========================================
    // ⏳ 挂起触发器：供内部调用，卡死当前线程弹窗
    // ==========================================
    int triggerAuthIntercept(int target_uid, const std::string& processName, const std::string& capability);

private:
    // 内部鉴权工具
    bool isManagerApp(uid_t uid) const;

    // 保存前台的接线员实例和锁
    std::shared_ptr<IAuthCallback> mAuthCallback;
    std::mutex mCallbackMutex;
};

}  // namespace daemon
}  // namespace rfcore
}  // namespace aidl
