#pragma once

#include <aidl/rfcore/daemon/CapabilityRequest.aidl>
#include <aidl/rfcore/daemon/CapabilityResult.aidl>
#include <sys/types.h>
#include <string>

namespace rfcore {
namespace daemon {

class PrivilegedWorker {
public:
    /**
     * 派生沙箱子进程并执行特权操作。
     * @param request 客户端传来的请求对象（包含 Fd、参数等）
     * @param caller_pid 调用方的真实 PID（由 Binder 驱动提供）
     * @param out_result 执行结果
     */
    static void executeInSandbox(const aidl::rfcore::daemon::CapabilityRequest& request,
                                 pid_t caller_pid,
                                 aidl::rfcore::daemon::CapabilityResult* out_result);

private:
    // 内部方法：在子进程中执行降级和具体任务
    static void childProcessRoutine(const aidl::rfcore::daemon::CapabilityRequest& request,
                                    pid_t caller_pid);
                                    
    // 权限降级工具
    static bool dropPrivilegesAndEnterNamespace(pid_t target_pid, const std::string& capability_type);
};

} // namespace daemon
} // namespace rfcore
