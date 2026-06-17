#pragma once
#include <aidl/rfcore/daemon/BnRFCoreBootstrap.h>
#include <aidl/rfcore/daemon/IRFCoreService.h>
#include <memory>

// 🚨 必须在 aidl 命名空间内
namespace aidl {
namespace rfcore {
namespace daemon {

class RFCoreBootstrap : public BnRFCoreBootstrap {
public:
    // 🚨 使用接口 IRFCoreService 进行解耦，避免找不到类的报错
    explicit RFCoreBootstrap(std::shared_ptr<IRFCoreService> worker);
    ndk::ScopedAStatus getWorker(std::shared_ptr<IRFCoreService>* _aidl_return) override;

private:
    std::shared_ptr<IRFCoreService> mWorker;
};

}  // namespace daemon
}  // namespace rfcore
}  // namespace aidl

