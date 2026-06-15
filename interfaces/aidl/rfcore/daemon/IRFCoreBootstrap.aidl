package rfcore.daemon;

import rfcore.daemon.IRFCoreService;

/**
 * RFCore 看门人服务。
 * 仅负责基础身份验证并分发匿名的 Worker Binder 引用。
 */
interface IRFCoreBootstrap {
    /**
     * 请求获取特权 Worker 句柄。
     * * @return 如果调用者 UID 在策略允许的名单内（或具备申请资格），则返回匿名的 IRFCoreService。
     * 如果无资格，则返回 null，或者直接抛出 Binder Exception。
     */
    @nullable IRFCoreService getWorker();
}
