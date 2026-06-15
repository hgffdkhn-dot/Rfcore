package rfcore.daemon;

import android.os.ParcelFileDescriptor;

/**
 * 封装特权操作请求的参数。
 */
parcelable CapabilityRequest {
    // 请求的具体能力名称，例如 "CAP_READ_FILE" 或 "CAP_MOUNT"
    String capability;
    
    // 执行该能力所需的参数列表
    String[] args;
    
    // 超时时间（毫秒），防止底层挂起导致 Binder 线程池耗尽
    long timeoutMs;
    
    // 客户端传入的文件描述符，用于规避 Binder 1MB 传输限制，实现大文件读写或流式输出
    @nullable ParcelFileDescriptor stdoutFd;
    @nullable ParcelFileDescriptor stderrFd;
    @nullable ParcelFileDescriptor stdinFd;
}
