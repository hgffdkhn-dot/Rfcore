#include "RFCoreBootstrap.h"

namespace aidl {
namespace rfcore {
namespace daemon {

RFCoreBootstrap::RFCoreBootstrap(std::shared_ptr<IRFCoreService> worker) : mWorker(worker) {}

ndk::ScopedAStatus RFCoreBootstrap::getWorker(std::shared_ptr<IRFCoreService>* _aidl_return) {
    *_aidl_return = mWorker;
    return ndk::ScopedAStatus::ok();
}

}  // namespace daemon
}  // namespace rfcore
}  // namespace aidl

