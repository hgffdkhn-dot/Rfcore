package rfcore.daemon;

/**
 * 封装特权操作的执行结果。
 */
parcelable CapabilityResult {
    // RPC 框架或守护进程内部的验证状态码（0 代表成功，非 0 代表鉴权失败或内部错误）
    int status;
    
    // 实际目标命令/操作的退出码（如 Linux 进程退出码）
    int exitCode;
    
    // 简短的状态描述或错误信息（详细输出应通过 Request 中的 Fd 读取）
    String message;
}
