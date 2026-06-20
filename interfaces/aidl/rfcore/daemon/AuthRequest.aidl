package rfcore.daemon;

// 🚨 ParcelFileDescriptor 是 Android 的黑科技，能跨进程传递真实的底层文件句柄！
parcelable AuthRequest {
    String processName;
    String capability;
    
    // 携带参数 (比如 "-c", "ls", "-l")
    String[] command;
    
    // 携带 App 自己的嘴巴和耳朵
    ParcelFileDescriptor fdIn;
    ParcelFileDescriptor fdOut;
    ParcelFileDescriptor fdErr;
}
