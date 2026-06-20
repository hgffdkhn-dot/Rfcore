#include <iostream>
#include <dlfcn.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <android/binder_auto_utils.h> // 🚨 引入 Binder 文件描述符工具
#include "RFCoreBootstrap.h"
#include "RFCoreService.h"

using aidl::rfcore::daemon::IRFCoreBootstrap;
using aidl::rfcore::daemon::IRFCoreService;
using aidl::rfcore::daemon::AuthRequest;
using aidl::rfcore::daemon::AuthResult;

typedef void* (*get_service_t)(const char*);

int main(int argc, char** argv) {
    void* handle = dlopen("libbinder_ndk.so", RTLD_NOW);
    if (!handle) return 1;
    auto get_service = (get_service_t)dlsym(handle, "AServiceManager_getService");
    
    void* raw_binder = get_service("rfcore.bootstrap");
    if (!raw_binder) {
        std::cerr << "su: rfcore daemon is not running!" << std::endl;
        return 1;
    }

    ndk::SpAIBinder binder((AIBinder*)raw_binder);
    auto bootstrap = IRFCoreBootstrap::fromBinder(binder);
    std::shared_ptr<IRFCoreService> worker;
    bootstrap->getWorker(&worker);

    AuthRequest req;
    req.processName = "UID_" + std::to_string(getuid()) + "_请求Root"; 
    req.capability = "CAP_ROOT_SHELL";

    // 1. 打包参数 (例如 "su -c ls" 会打包成 ["-c", "ls"])
    for (int i = 1; i < argc; ++i) {
        req.command.push_back(argv[i]);
    }

    // 2. 🚨 黑魔法：把当前的 标准输入、标准输出、标准错误 复制一份，塞进请求包！
    req.fdIn = ndk::ScopedFileDescriptor(dup(STDIN_FILENO));
    req.fdOut = ndk::ScopedFileDescriptor(dup(STDOUT_FILENO));
    req.fdErr = ndk::ScopedFileDescriptor(dup(STDERR_FILENO));

    AuthResult res;
    worker->requestAuth(req, &res);

    // 3. 收尾处理
    if (!res.isGranted) {
        std::cerr << "su: Permission denied" << std::endl;
        return 1; 
    }

    // 如果授权成功，守护进程已经在后台把 Shell 和我们的管子对接好了！
    // 我们的 su 进程只需要安静地退出，把屏幕留给底层的 Shell 即可。
    return 0;
}
