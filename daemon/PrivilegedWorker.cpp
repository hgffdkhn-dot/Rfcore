#include "PrivilegedWorker.h"
#include <android/log.h> // 使用 NDK 日志
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <linux/capability.h>
#include <sys/syscall.h>
#include <cstring> // 添加 memset 所需的头文件

#define LOG_TAG "RFCoreWorker"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rfcore {
namespace daemon {

using aidl::rfcore::daemon::CapabilityRequest;
using aidl::rfcore::daemon::CapabilityResult;

constexpr uid_t UID_NOBODY = 9999;
constexpr gid_t GID_NOBODY = 9999;

// 极其轻量级的资源管理封装，替代 AOSP 的 unique_fd
class ScopedFd {
public:
    int fd;
    ScopedFd(int f) : fd(f) {}
    ~ScopedFd() { if (fd >= 0) close(fd); }
    int get() const { return fd; }
    bool isValid() const { return fd >= 0; }
};

void PrivilegedWorker::executeInSandbox(const CapabilityRequest& request,
                                        pid_t caller_pid,
                                        CapabilityResult* out_result) {
    pid_t pid = fork();

    if (pid < 0) {
        LOGE("Fork failed!");
        out_result->status = -1;
        out_result->message = "Daemon internal fork error";
        return;
    }

    if (pid == 0) {
        childProcessRoutine(request, caller_pid);
        _exit(0); 
    }

    int status;
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
    std::string mnt_ns_path = "/proc/" + std::to_string(target_pid) + "/ns/mnt";
    ScopedFd ns_fd(open(mnt_ns_path.c_str(), O_RDONLY | O_CLOEXEC));
    if (!ns_fd.isValid()) {
        LOGE("Failed to open mount namespace for PID %d", target_pid);
        return false;
    }
    
    if (setns(ns_fd.get(), CLONE_NEWNS) != 0) {
        LOGE("setns failed");
        return false;
    }

    if (setresgid(GID_NOBODY, GID_NOBODY, GID_NOBODY) != 0 || 
        setresuid(UID_NOBODY, UID_NOBODY, UID_NOBODY) != 0) {
        LOGE("Failed to drop UID/GID to nobody");
        return false;
    }

    struct __user_cap_header_struct cap_header;
    struct __user_cap_data_struct cap_data[_LINUX_CAPABILITY_U32S_3];

    memset(&cap_header, 0, sizeof(cap_header));
    memset(&cap_data, 0, sizeof(cap_data));

    cap_header.version = _LINUX_CAPABILITY_VERSION_3;
    cap_header.pid = 0; 

    if (capability_type == "CAP_READ_FILE") {
        cap_data[0].effective |= CAP_TO_MASK(CAP_DAC_READ_SEARCH);
        cap_data[0].permitted |= CAP_TO_MASK(CAP_DAC_READ_SEARCH);
    } 
    
    if (syscall(SYS_capset, &cap_header, &cap_data) != 0) {
        LOGE("capset failed");
        return false;
    }

    return true;
}

void PrivilegedWorker::childProcessRoutine(const CapabilityRequest& request, pid_t caller_pid) {
    if (!dropPrivilegesAndEnterNamespace(caller_pid, request.capability)) {
        _exit(1); 
    }

    if (request.stdoutFd.get() >= 0) {
        dup2(request.stdoutFd.get(), STDOUT_FILENO);
    }
    if (request.stderrFd.get() >= 0) {
        dup2(request.stderrFd.get(), STDERR_FILENO);
    }

    if (request.capability == "CAP_READ_FILE") {
        if (request.args.empty()) {
            _exit(2);
        }
        
        std::string target_file = request.args[0];
        ScopedFd target_fd(open(target_file.c_str(), O_RDONLY));
        if (!target_fd.isValid()) {
            dprintf(STDERR_FILENO, "Failed to open file: %s\n", target_file.c_str());
            _exit(3);
        }

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
        _exit(0); 
    }

    _exit(127);
}

} 
} 
