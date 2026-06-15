#include "RFCoreBootstrap.h"
#include <android/binder_ibinder.h>
#include <android-base/logging.h>

namespace rfcore {
namespace daemon {

RFCoreBootstrap::RFCoreBootstrap(std::shared_ptr<RFCoreService> worker) 
    : mWorker(worker) {
}

ndk::ScopedAStatus RFCoreBootstrap::getWorker(std::shared_ptr<aidl::rfcore::daemon::IRFCoreService>* _aidl_return) {
    uid_t caller_uid = AIBinder_getCallingUid();
    LOG(INFO) << "Bootstrap handshake initiated from UID: " << caller_uid;

    // TODO: 查询 SQLite 策略，判断该 UID 是否有资格获取 Worker。
    // 在 MVP 阶段，我们假设所有请求都先放行，在 Worker 层再做具体 Capability 拦截；
    // 或者在这里做第一道粗粒度拦截（如禁止某些已知的反作弊 UID 连入）。
    
    bool is_eligible = true; 

    if (is_eligible) {
        // 利用 libbinder_ndk 的特性，直接将强引用赋值，底层会自动处理 writeStrongBinder
        *_aidl_return = mWorker;
        LOG(INFO) << "Handshake successful. Anonymous Worker dispatched to UID: " << caller_uid;
        return ndk::ScopedAStatus::ok();
    } else {
        // 拒绝分发：返回 null，并给出一个业务错误码
        *_aidl_return = nullptr;
        LOG(WARNING) << "Handshake rejected for UID: " << caller_uid;
        return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(-1, "Unauthorized UID");
    }
}

} // namespace daemon
} // namespace rfcore
