#include "RFCoreBootstrap.h"
#include "RFCoreService.h"

#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android-base/logging.h>
#include <cstdlib>
#include <string>

using rfcore::daemon::RFCoreBootstrap;
using rfcore::daemon::RFCoreService;

int main(int argc, char** argv) {
    // 1. 初始化 AOSP 标准日志系统，将日志输出至 logd
    // 采用 SYSTEM 日志缓冲区，符合底层系统服务规范
    android::base::InitLogging(argv, android::base::LogdLogger(android::base::SYSTEM));
    LOG(INFO) << "Starting RFCore Daemon process...";

    // 2. 初始化 NDK Binder 线程池
    // 鉴于 Daemon 主要处理轻量级的 IPC 请求和能力授权，4 个线程足以应对常规并发。
    // 避免设置过大导致被内核查杀或无谓的内存占用。
    ABinderProcess_setThreadPoolMaxThreadCount(4);
    ABinderProcess_startThreadPool();

    // 3. 实例化真正的特权 Worker (IRFCoreService 接口的实现)
    // [关键对抗策略]：绝不调用 AServiceManager_addService 将 Worker 注册到系统。
    // 它以匿名的 std::shared_ptr 形式驻留在当前进程内存中，不挂载任何名字，不可被枚举扫描。
    std::shared_ptr<RFCoreService> worker = ndk::SharedRefBase::make<RFCoreService>();

    // 4. 实例化 Bootstrap (IRFCoreBootstrap 接口的实现，充当看门人)
    // 将内存中的 Worker 句柄强引用直接注入给看门人。
    std::shared_ptr<RFCoreBootstrap> bootstrap = ndk::SharedRefBase::make<RFCoreBootstrap>(worker);

    // 5. 将看门人注册到 ServiceManager
    // 名字固定，对全系统公开。
    // 设计逻辑：暴露一个无特权的马甲给防线外部。反作弊检测到它也没有意义，因为它本身没有执行高权限命令的能力。
    const std::string bootstrap_name = "rfcore.bootstrap";
    binder_status_t status = AServiceManager_addService(bootstrap->asBinder().get(), bootstrap_name.c_str());

    if (status != STATUS_OK) {
        // 如果注册失败，通常是因为 SELinux 策略 (ServiceManager 拦截) 或名称冲突
        LOG(FATAL) << "Failed to register bootstrap service to ServiceManager! Status: " << status;
        return -1;
    }

    LOG(INFO) << "RFCore Bootstrap registered as '" << bootstrap_name << "'. Worker initialized anonymously.";

    // 6. 挂起主线程，阻塞监听并处理所有传入的 Binder 请求
    ABinderProcess_joinThreadPool();

    // 理论上，一旦加入线程池循环，进程就不会退出，除非被 init 强行 kill。
    // 如果走到这里，说明 Binder 驱动或线程池发生了致命异常。
    LOG(ERROR) << "RFCore Daemon thread pool unexpectedly exited.";
    return 0;
}
