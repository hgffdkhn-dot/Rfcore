package rfcore.daemon;

interface IAuthCallback {
    /**
     * 当有底层权限请求被拦截时，Daemon 会调用此方法挂起请求，等待用户在屏幕上点击。
     * 返回值: 1 表示允许, 0 表示拒绝
     * * 🚨 核心防坑指南：
     * NDK C++ 编译环境下，像 String 这种非基本数据类型，必须显式声明数据流向标签 (in)！
     * 绝对不能漏掉参数前面的 'in' 关键字！
     */
    int onAuthRequested(int uid, in String processName, in String capability);
}
