#include <iostream>
#include <dlfcn.h>
#include <unistd.h>
#include <vector>
#include <string>
#include "RFCoreBootstrap.h"
#include "RFCoreService.h"

using aidl::rfcore::daemon::IRFCoreBootstrap;
using aidl::rfcore::daemon::IRFCoreService;
// 🚨 关键修复：这里必须使用我们刚刚新建的专用提权暗号！
using aidl::rfcore::daemon::AuthRequest;
using aidl::rfcore::daemon::AuthResult;

typedef void* (*get_service_t)(const char*);

int main(int argc, char** argv) {
    std::string command = "";
    for (int i = 1; i < argc; ++i) {
        command += argv[i];
        if (i < argc - 1) command += " ";
    }

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

    // 🚨 关键修复：使用 AuthRequest 组装请求
    AuthRequest req;
    req.processName = "UID_" + std::to_string(getuid()) + "_请求Root"; 
    req.capability = "CAP_ROOT_SHELL";
    
    // 🚨 关键修复：使用 AuthResult 接收结果
    AuthResult res;

    // 🚨 关键修复：调用咱们专为 su 开通的新通道 requestAuth
    worker->requestAuth(req, &res);

    if (res.isGranted) {
        std::cout << "✅ [RFCore] Root Access Granted!" << std::endl;
        if (!command.empty()) {
            std::cout << "准备执行命令 -> " << command << std::endl;
        }
        // TODO: 下一步将在这里把底层的真实 shell FD 传过来接管
        return 0;
    } else {
        std::cerr << "su: permission denied" << std::endl;
        return 1; 
    }
}
