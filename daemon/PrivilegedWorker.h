#pragma once

// 将后缀从 .aidl 改为 .h，因为编译后会被生成为头文件
#include <aidl/rfcore/daemon/CapabilityRequest.h>
#include <aidl/rfcore/daemon/CapabilityResult.h>
#include <sys/types.h>
#include <string>

namespace rfcore {
namespace daemon {

class PrivilegedWorker {
public:
    static void executeInSandbox(const aidl::rfcore::daemon::CapabilityRequest& request,
                                 pid_t caller_pid,
                                 aidl::rfcore::daemon::CapabilityResult* out_result);

private:
    static void childProcessRoutine(const aidl::rfcore::daemon::CapabilityRequest& request,
                                    pid_t caller_pid);
                                    
    static bool dropPrivilegesAndEnterNamespace(pid_t target_pid, const std::string& capability_type);
};

} // namespace daemon
} // namespace rfcore
