#include "RFCoreService.h"
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <fstream>
#include <sstream>
#include <iostream>

// 如果你有数据库单例，可以 include 进来
// #include "PolicyDatabase.h" 

namespace aidl {
namespace rfcore {
namespace daemon {

RFCoreService::RFCoreService() {}
RFCoreService::~RFCoreService() {}

// ==========================================
// 🛡️ 绝杀：花名册动态鉴权 (彻底抛弃写死 UID)
// ==========================================
bool RFCoreService::isManagerApp(uid_t uid) const {
    // 1. Root 自身 (uid=0) 绝对放行
    if (uid == 0) return true;

    // 2. 读取 Android 系统底层的应用花名册
    std::ifstream file("/data/system/packages.list");
    if (!file.is_open()) {
        // 如果系统魔改读不到文件，安全起见直接拒绝
        return false; 
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string pkgName;
        int pkgUid;
        
        // 解析格式，通常前面两个字段就是包名和 UID
        if (iss >> pkgName >> pkgUid) {
            // 如果呼叫我们的这个 UID，刚好对应的是 Manager App 的包名，立刻放行！
            if (pkgUid == uid && pkgName == "com.rfcore.manager") {
                return true;
            }
        }
    }
    return false; // 李鬼 App 统统拦截
}

// ==========================================
// 🚨 接收前台的“接线员”并保存
// ==========================================
ndk::ScopedAStatus RFCoreService::registerAuthCallback(const std::shared_ptr<IAuthCallback>& in_callback) {
    uid_t caller_uid = AIBinder_getCallingUid();
    
    // 安全第一：必须是我们自己的 App 才能插上这根电话线！
    if (!isManagerApp(caller_uid)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_SECURITY);
    }

    std::lock_guard<std::mutex> lock(mCallbackMutex);
    mAuthCallback = in_callback;
    
    return ndk::ScopedAStatus::ok();
}

// ==========================================
// ⏳ 时间停止黑魔法：主动呼叫前台并挂起等待
// ⚠️ 注意：不要在主 Binder 线程里自己调用自己，一般是在拦截到文件系统操作(如 openat) 的线程里调用这个函数！
// ==========================================
int RFCoreService::triggerAuthIntercept(int target_uid, const std::string& processName, const std::string& capability) {
    std::shared_ptr<IAuthCallback> callback;
    {
        std::lock_guard<std::mutex> lock(mCallbackMutex);
        callback = mAuthCallback;
    }

    if (callback != nullptr) {
        int32_t userDecision = 0;
        
        // 🚨 挂起爆发点：底层 C++ 会在这里同步等待，直到前台 App 的 Kotlin 闭包执行了 future.complete()！
        ndk::ScopedAStatus status = callback->onAuthRequested(target_uid, processName, capability, &userDecision);
        
        if (status.isOk()) {
            return userDecision; // 返回 1 (放行) 或 0 (拦截)
        }
    }

    // 如果 App 在后台被杀死了，或者断开了连接，默认采取最严格的“拒绝”策略
    return 0; 
}


// ==========================================
// 业务数据接口 (处理 App 的主动请求)
// ==========================================
ndk::ScopedAStatus RFCoreService::getStatus(int32_t* _aidl_return) {
    *_aidl_return = 1; 
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus RFCoreService::getPolicies(std::vector<PolicyRecord>* _aidl_return) {
    uid_t caller_uid = AIBinder_getCallingUid();
    if (!isManagerApp(caller_uid)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_SECURITY);
    }

    // TODO: 接入你的 SQLite 数据库
    // *_aidl_return = PolicyDatabase::getInstance().getPolicies();
    
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus RFCoreService::grantCapability(int32_t in_uid, const std::string& in_packageName, const std::string& in_capability, int32_t in_isGranted, int64_t in_expiresAt, bool* _aidl_return) {
    uid_t caller_uid = AIBinder_getCallingUid();
    if (!isManagerApp(caller_uid)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_SECURITY);
    }

    // TODO: 写入 SQLite 数据库
    *_aidl_return = true;
    return ndk::ScopedAStatus::ok();
}

// 其余必须实现的空接口 (按你的实际业务去填数据库逻辑)
ndk::ScopedAStatus RFCoreService::requestCapability(const CapabilityRequest& request, CapabilityResult* _aidl_return) { return ndk::ScopedAStatus::ok(); }
ndk::ScopedAStatus RFCoreService::isCapabilityGranted(const std::string& in_capability, bool* _aidl_return) { return ndk::ScopedAStatus::ok(); }
ndk::ScopedAStatus RFCoreService::revokeCapability(int32_t in_uid, const std::string& in_capability, bool* _aidl_return) { return ndk::ScopedAStatus::ok(); }
ndk::ScopedAStatus RFCoreService::getAuditLogs(int32_t in_limit, int32_t in_offset, std::vector<AuditRecord>* _aidl_return) { return ndk::ScopedAStatus::ok(); }

}  // namespace daemon
}  // namespace rfcore
}  // namespace aidl
