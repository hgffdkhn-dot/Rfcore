#include <iostream>
#include <dlfcn.h>
#include <unistd.h>
#include <vector>
#include <string>
#include "RFCoreBootstrap.h"
#include "RFCoreService.h"

using aidl::rfcore::daemon::IRFCoreBootstrap;
using aidl::rfcore::daemon::IRFCoreService;
using aidl::rfcore::daemon::CapabilityRequest;
using aidl::rfcore::daemon::CapabilityResult;

typedef void* (*get_service_t)(const char*);

int main(int argc, char** argv) {
    // 1. 提取应用调用 su 时传入的参数 (比如 su -c "ls /data")
    std::string command = "";
    for (int i = 1; i < argc; ++i) {
        command += argv[i];
        if (i < argc - 1) command += " ";
    }

    // 2. 加载 Binder 库，准备呼叫咱们的守护进程
    void* handle = dlopen("libbinder_ndk.so", RTLD_NOW);
    if (!handle) {
        std::cerr << "su: failed to load binder" << std::endl;
        return 1;
    }
    auto get_service = (get_service_t)dlsym(handle, "AServiceManager_getService");
    
    void* raw_binder = get_service("rfcore.bootstrap");
    if (!raw_binder) {
        std::cerr << "su: daemon not running or access denied" << std::endl;
        return 1;
    }

    ndk::SpAIBinder binder((AIBinder*)raw_binder);
    auto bootstrap = IRFCoreBootstrap::fromBinder(binder);
    std::shared_ptr<IRFCoreService> worker;
    bootstrap->getWorker(&worker);

    // 3. 组装请求：告诉守护进程，我是谁，我要干嘛
    CapabilityRequest req;
    // 真实的 su 会在这里通过 getppid() 获取父进程包名，这里我们暂用 UID 占位
    req.processName = "UID_" + std::to_string(getuid()) + "_请求Root"; 
    req.capability = "CAP_ROOT_SHELL";
    CapabilityResult res;

    // 4. 发起提权请求！(此时当前 App 的 su 进程会被挂起，等待屏幕弹窗)
    worker->requestCapability(req, &res);

    // 5. 宣判结果！
    if (res.isGranted) {
        // 🚨 核心伏笔：授权通过！
        // 真正的 su 在这里会接收守护进程传回来的“文件描述符 (FD)”，接管真正的 Root Shell。
        // 我们先在这里打印成功标识，下一步我们将把真实的 /system/bin/sh 环境接进来！
        std::cout << "RFCore: Root Access Granted!" << std::endl;
        if (!command.empty()) {
            std::cout << "RFCore: Ready to execute -> " << command << std::endl;
        }
        return 0; // 成功退出
    } else {
        std::cerr << "su: permission denied" << std::endl;
        return 1; // 拒绝退出，流氓软件会收到权限拒绝错误
    }
}
