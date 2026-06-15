#pragma once

#include <aidl/rfcore/daemon/BnRFCoreBootstrap.h>
#include "RFCoreService.h"
#include <memory>

namespace rfcore {
namespace daemon {

class RFCoreBootstrap : public aidl::rfcore::daemon::BnRFCoreBootstrap {
public:
    // 构造时注入真实的 Worker 实例
    explicit RFCoreBootstrap(std::shared_ptr<RFCoreService> worker);
    ~RFCoreBootstrap() override = default;

    // 分发 Worker 的核心逻辑
    ndk::ScopedAStatus getWorker(std::shared_ptr<aidl::rfcore::daemon::IRFCoreService>* _aidl_return) override;

private:
    std::shared_ptr<RFCoreService> mWorker;
};

} // namespace daemon
} // namespace rfcore
