#include <iostream>
#include <dlfcn.h>
#include <unistd.h>
#include <vector>
#include <string>
#include "RFCoreBootstrap.h"
#include "RFCoreService.h"

using aidl::rfcore::daemon::IRFCoreBootstrap;
using aidl::rfcore::daemon::IRFCoreService;
using aidl::rfcore::daemon::AuthRequest;
using aidl::rfcore::daemon::AuthResult;

typedef void* (*get_service_t)(const char*);

int main(int argc, char** argv) {
    // 1. 加载 Binder 库，准备呼叫咱们的守护进程
    void* handle = dlopen("libbinder_ndk.so", RTLD_NOW);
    if (!handle) {
        std::cerr << "rfsu: failed to load binder" << std::endl;
        return 1;
    }
    auto get_service = (get_service_t)dlsym(handle, "AServiceManager_getService");
    
    void* raw_binder = get_service("rfcore.bootstrap");
    if (!raw_binder) {
        std::cerr << "rfsu: daemon not running or access denied" << std::endl;
        return 1;
    }

    ndk::SpAIBinder binder((AIBinder*)raw_binder);
    auto bootstrap = IRFCoreBootstrap::fromBinder(binder);
    std::shared_ptr<IRFCoreService> worker;
    bootstrap->getWorker(&worker);

    // 2. 组装请求：向咱们的守护进程提交越权申请
    AuthRequest req;
    req.processName = "UID_" + std::to_string(getuid()) + "_请求Root"; 
    req.capability = "CAP_ROOT_SHELL";
    AuthResult res;

    // 发起提权呼叫，此时 Termux 会卡在这里等前台弹窗
    worker->requestAuth(req, &res);

    // 3. 宣判结果！
    if (res.isGranted) {
        // 🚨 核心劫持魔法：授权通过！移花接木！
        // 把当前参数列表的第 0 个参数（原本是 rfsu）强行改成 su
        argv[0] = (char*)"su"; 
        
        // execvp 会直接用系统的真 su 进程替换掉当前的 rfsu 进程！
        // 这一步之后，控制权完美移交给 AlphaSU，并瞬间进入真 Root Shell！
        execvp("su", argv);
        
        // 如果系统里连真的 su 都没有，execvp 失败才会走到这里
        std::cerr << "rfsu: failed to execute real su command" << std::endl;
        return 1;
    } else {
        // 授权拒绝，直接拦截，根本不给它碰到真 su 的机会！
        std::cerr << "rfsu: permission denied" << std::endl;
        return 1; 
    }
}
