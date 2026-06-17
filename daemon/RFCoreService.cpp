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
// 🛡️ 核心鉴权：动态解析 packages.list，绝对防止李鬼应用
// =========================================================
bool RFCoreService::isManagerApp(uid_t uid) const {
    if (uid == 0) return true; // Root 进程自身绝对放行
    
    std::ifstream file("/data/system/packages.list");
    if (!file.is_open()) return false;
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string pkgName;
        int pkgUid;
        if (iss >> pkgName >> pkgUid) {
            // 校验：UID 必须对得上，且包名必须是我们的 Manager！
            if (pkgUid == uid && pkgName == "com.rfcore.manager") return true;
        }
    }
    return false;
}

// =========================================================
// 📞 注册通信通道：接收前台 Manager 递过来的回调电话线
// =========================================================
ndk::ScopedAStatus RFCoreService::registerAuthCallback(const std::shared_ptr<IAuthCallback>& in_callback) {
    uid_t caller_uid = AIBinder_getCallingUid();
    
    // 安全第一：如果不是我们的 App 试图注册，当场打回！
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
        // 🚨 核心爆发点：发起请求的流氓软件线程，将在这里死死卡住！
        // 直到用户在前台 App 点击“允许”或“拒绝”后，这里才会拿到 userDecision 并苏醒。
        ndk::ScopedAStatus status = callback->onAuthRequested(target_uid, processName, capability, &userDecision);
        if (status.isOk()) return userDecision;
    }
    
    // 如果 App 在后台被杀死了，默认采取最严格的“全部拒绝”策略
    return 0; 
}

// =========================================================
// 🎯 测试接口：配合前台 App 的紫色测试按钮
// =========================================================
ndk::ScopedAStatus RFCoreService::getStatus(int32_t* _aidl_return) {
    // 当你在 App 里点击“模拟拦截测试”按钮时，会走到这里
    // 我们在这里假装抓到了一个名叫 com.malicious.spy 的木马在申请 Root 权限！
    int userDecision = triggerAuthIntercept(10443, "com.malicious.spy", "CAP_ROOT_SHELL");
    
    // 把你的点击结果 (1=允许, 0=拒绝) 返回给前台展示
    *_aidl_return = userDecision; 
    return ndk::ScopedAStatus::ok();
}

// =========================================================
// 📦 业务数据接口 (为下一步的 SQLite 数据库和真实 Hook 做准备)
// =========================================================

// 这是未来面向其他流氓软件 / Hook 模块的真实请求入口
ndk::ScopedAStatus RFCoreService::requestCapability(const CapabilityRequest& request, CapabilityResult* _aidl_return) { 
    uid_t target_uid = AIBinder_getCallingUid();
    
    // 如果是管理器自己调用，直接放行
    if (isManagerApp(target_uid)) {
        _aidl_return->isGranted = true;
        return ndk::ScopedAStatus::ok();
    }

    // 触发真实拦截挂起
    int userDecision = triggerAuthIntercept(target_uid, request.processName, request.capability);
    _aidl_return->isGranted = (userDecision == 1);
    
    return ndk::ScopedAStatus::ok(); 
}

ndk::ScopedAStatus RFCoreService::getPolicies(std::vector<PolicyRecord>* _aidl_return) {
    // TODO: 下一步接入 SQLite 读取授权列表
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus RFCoreService::grantCapability(int32_t in_uid, const std::string& in_packageName, const std::string& in_capability, int32_t in_isGranted, int64_t in_expiresAt, bool* _aidl_return) {
    // TODO: 下一步将授权结果写入 SQLite 保存
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
