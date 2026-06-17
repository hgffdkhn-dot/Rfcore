#pragma once

#include <aidl/rfcore/daemon/BnRFCoreService.h>
// 🚨 必须引入刚刚编译生成的 Callback 头文件
#include <aidl/rfcore/daemon/IAuthCallback.h> 

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
    // 基础业务接口 (AIDL 自动生成的重写方法)
    // ==========================================
    ndk::ScopedAStatus requestCapability(const CapabilityRequest& request, CapabilityResult* _aidl_return) override;
    ndk::ScopedAStatus isCapabilityGranted(const std::string& in_capability, bool* _aidl_return) override;
    ndk::ScopedAStatus getStatus(int32_t* _aidl_return) override;
    ndk::ScopedAStatus grantCapability(int32_t in_uid, const std::string& in_packageName, const std::string& in_capability, int32_t in_isGranted, int64_t in_expiresAt, bool* _aidl_return) override;
    ndk::ScopedAStatus revokeCapability(int32_t in_uid, const std::string& in_capability, bool* _aidl_return) override;
    ndk::ScopedAStatus getPolicies(std::vector<PolicyRecord>* _aidl_return) override;
    ndk::ScopedAStatus getAuditLogs(int32_t in_limit, int32_t in_offset, std::vector<AuditRecord>* _aidl_return) override;

    // ==========================================
    // 🚨 新增：接听前台 Manager App 递过来的电话线
    // ==========================================
    ndk::ScopedAStatus registerAuthCallback(const std::shared_ptr<IAuthCallback>& in_callback) override;

    // ==========================================
    // 🚨 核心武器：供你的 Hook 代码调用的“时间停止”触发器
    // ==========================================
    int triggerAuthIntercept(int target_uid, const std::string& processName, const std::string& capability);

private:
    // 内部鉴权工具：扫描 packages.list
    bool isManagerApp(uid_t uid) const;

    // 保存前台的接线员实例
    std::shared_ptr<IAuthCallback> mAuthCallback;
    // 防止多线程抢占回调的锁
    std::mutex mCallbackMutex;
};

}  // namespace daemon
}  // namespace rfcore
}  // namespace aidl
