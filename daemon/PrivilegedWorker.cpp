#include "PrivilegedWorker.h"
#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <linux/capability.h>
#include <sys/syscall.h>

namespace rfcore {
namespace daemon {

using aidl::rfcore::daemon::CapabilityRequest;
using aidl::rfcore::daemon::CapabilityResult;

// Android 系统中的 nobody 用户
constexpr uid_t UID_NOBODY = 9999;
constexpr gid_t GID_NOBODY = 9999;

void PrivilegedWorker::executeInSandbox(const CapabilityRequest& request,
                                        pid_t caller_pid,
                                        CapabilityResult* out_result) {
    pid_t pid = fork();

    if (pid < 0) {
        LOG(ERROR) << "Fork failed!";
        out_result->status = -1;
        out_result->message = "Daemon internal fork error";
        return;
    }

    if (pid == 0) {
        // ========== 子进程 (Worker) 逻辑 ==========
        childProcessRoutine(request, caller_pid);
        // 绝对不能通过 return 返回，必须直接 _exit 终止子进程，
        // 否则会引发两个进程同时清理 Binder 线程池的致命灾难。
        _exit(0); 
    }

    // ========== 父进程 (Daemon) 逻辑 ==========
    int status;
    // TODO: 结合 request.timeoutMs 实现超时 kill，这里 MVP 阶段先使用阻塞 waitpid
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        out_result->exitCode = WEXITSTATUS(status);
        out_result->status = 0;
        out_result->message = "Execution completed";
    } else {
        out_result->status = -2;
        out_result->exitCode = -1;
        out_result->message = "Worker terminated abnormally";
    }
}

bool PrivilegedWorker::dropPrivilegesAndEnterNamespace(pid_t target_pid, const std::string& capability_type) {
    // 1. 切换 Mount Namespace，进入客户端 App 的文件视图
    std::string mnt_ns_path = "/proc/" + std::to_string(target_pid) + "/ns/mnt";
    android::base::unique_fd ns_fd(open(mnt_ns_path.c_str(), O_RDONLY | O_CLOEXEC));
    if (ns_fd < 0) {
        PLOG(ERROR) << "Failed to open mount namespace for PID " << target_pid;
        return false;
    }
    
    // CLONE_NEWNS 专指 Mount Namespace
    if (setns(ns_fd.get(), CLONE_NEWNS) != 0) {
        PLOG(ERROR) << "setns failed";
        return false;
    }

    // 2. 降级 UID/GID 至 nobody
    if (setresgid(GID_NOBODY, GID_NOBODY, GID_NOBODY) != 0 || 
        setresuid(UID_NOBODY, UID_NOBODY, UID_NOBODY) != 0) {
        PLOG(ERROR) << "Failed to drop UID/GID to nobody";
        return false;
    }

    // 3. 剥离 Linux Capabilities
    struct __user_cap_header_struct cap_header;
    struct __user_cap_data_struct cap_data[_LINUX_CAPABILITY_U32S_3];

    memset(&cap_header, 0, sizeof(cap_header));
    memset(&cap_data, 0, sizeof(cap_data));

    cap_header.version = _LINUX_CAPABILITY_VERSION_3;
    cap_header.pid = 0; // 0 代表当前线程

    // 默认剥离所有权限。如果是 CAP_READ_FILE，我们只保留 CAP_DAC_READ_SEARCH (用于绕过 DAC 读文件)
    if (capability_type == "CAP_READ_FILE") {
        cap_data[0].effective |= CAP_TO_MASK(CAP_DAC_READ_SEARCH);
        cap_data[0].permitted |= CAP_TO_MASK(CAP_DAC_READ_SEARCH);
    } 
    // 若后续有其他 capability 需求，可在此扩展
    
    if (syscall(SYS_capset, &cap_header, &cap_data) != 0) {
        PLOG(ERROR) << "capset failed";
        return false;
    }

    return true;
}

void PrivilegedWorker::childProcessRoutine(const CapabilityRequest& request, pid_t caller_pid) {
    // 第一步：进入沙箱
    if (!dropPrivilegesAndEnterNamespace(caller_pid, request.capability)) {
        _exit(1); // 初始化沙箱失败
    }

    // 第二步：处理 Fd 重定向
    // 将客户端传来的 stdoutFd 复制到标准输出 (fd 1)，实现 Zero-Copy 的回传
    if (request.stdoutFd.get() >= 0) {
        dup2(request.stdoutFd.get(), STDOUT_FILENO);
    }
    if (request.stderrFd.get() >= 0) {
        dup2(request.stderrFd.get(), STDERR_FILENO);
    }

    // 第三步：执行具体业务
    if (request.capability == "CAP_READ_FILE") {
        if (request.args.empty()) {
            _exit(2); // 参数缺失
        }
        
        std::string target_file = request.args[0];
        android::base::unique_fd target_fd(open(target_file.c_str(), O_RDONLY));
        if (target_fd < 0) {
            dprintf(STDERR_FILENO, "Failed to open file: %s\n", target_file.c_str());
            _exit(3);
        }

        // 高效的数据泵：从目标文件读出，直接写入标准输出（即客户端传来的 FD）
        char buffer[8192];
        ssize_t bytes_read;
        while ((bytes_read = TEMP_FAILURE_RETRY(read(target_fd.get(), buffer, sizeof(buffer)))) > 0) {
            ssize_t bytes_written = 0;
            while (bytes_written < bytes_read) {
                ssize_t w = TEMP_FAILURE_RETRY(write(STDOUT_FILENO, buffer + bytes_written, bytes_read - bytes_written));
                if (w < 0) _exit(4);
                bytes_written += w;
            }
        }
        _exit(0); // 成功完成
    }

    // 未知操作
    _exit(127);
}

} // namespace daemon
} // namespace rfcore
