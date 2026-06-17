package rfcore.daemon;

import rfcore.daemon.CapabilityRequest;
import rfcore.daemon.CapabilityResult;
import rfcore.daemon.PolicyRecord;
import rfcore.daemon.AuditRecord;
import rfcore.daemon.IAuthCallback;

interface IRFCoreService {
    CapabilityResult requestCapability(in CapabilityRequest request);
    boolean isCapabilityGranted(String capability);
    int getStatus();
    boolean grantCapability(int uid, String packageName, String capability, int isGranted, long expiresAt);
    boolean revokeCapability(int uid, String capability);
    PolicyRecord[] getPolicies();
    AuditRecord[] getAuditLogs(int limit, int offset);
    
    // 🚨 必须加上这行，C++ 的 override 才不会报错！(记得参数前一定要有 in)
    void registerAuthCallback(in IAuthCallback callback);
}

