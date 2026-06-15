#include "RFCoreBootstrap.h"
#include "RFCoreService.h"

#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/log.h> // 替换为 NDK 标准日志
#include <cstdlib>
#include <string>

// 定义 NDK 宏
#define LOG_TAG "RFCoreDaemon"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGF(...) __android_log_print(ANDROID_LOG_FATAL, LOG_TAG, __VA_ARGS__)

using rfcore::daemon::RFCoreBootstrap;
using rfcore::daemon::RFCoreService;

int main(int argc, char** argv) {
    LOGI("Starting RFCore Daemon process...");

    // 初始化 NDK Binder 线程池
    ABinderProcess_setThreadPoolMaxThreadCount(4);
    ABinderProcess_startThreadPool();

    std::shared_ptr<RFCoreService> worker = ndk::SharedRefBase::make<RFCoreService>();
    std::shared_ptr<RFCoreBootstrap> bootstrap = ndk::SharedRefBase::make<RFCoreBootstrap>(worker);

    const std::string bootstrap_name = "rfcore.bootstrap";
    binder_status_t status = AServiceManager_addService(bootstrap->asBinder().get(), bootstrap_name.c_str());

    if (status != STATUS_OK) {
        LOGF("Failed to register bootstrap service to ServiceManager! Status: %d", status);
        return -1;
    }

    LOGI("RFCore Bootstrap registered as '%s'. Worker initialized anonymously.", bootstrap_name.c_str());

    // 挂起主线程，阻塞监听
    ABinderProcess_joinThreadPool();

    LOGE("RFCore Daemon thread pool unexpectedly exited.");
    return 0;
}
