package rfcore.daemon;

import rfcore.daemon.CapabilityRequest;
import rfcore.daemon.CapabilityResult;
import rfcore.daemon.PolicyRecord;
import rfcore.daemon.AuditRecord;
import rfcore.daemon.IAuthCallback;
// 🚨 引入我们的新暗号
import rfcore.daemon.AuthRequest;
import rfcore.daemon.AuthResult;

interface IRFCoreService {
    CapabilityResult requestCapability(in CapabilityRequest request); 
    boolean isCapabilityGranted(String capability);
    int getStatus();
    boolean grantCapability(int uid, String packageName, String capability, int isGranted, long expiresAt);
    boolean revokeCapability(int uid, String capability);
    PolicyRecord[] getPolicies();
    AuditRecord[] getAuditLogs(int limit, int offset);
    void registerAuthCallback(in IAuthCallback callback);
    
    // 🚨 新增：专门给 su 提权用的独立通道！
    AuthResult requestAuth(in AuthRequest request);
}
