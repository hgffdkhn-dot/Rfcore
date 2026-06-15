#pragma once

#include <string>
#include <vector>
#include <sqlite3.h>
#include <aidl/rfcore/daemon/PolicyRecord.h>
#include <aidl/rfcore/daemon/AuditRecord.h>

namespace rfcore {
namespace daemon {

class PolicyDatabase {
public:
    static PolicyDatabase& getInstance();

    bool initialize(const std::string& dbPath, const std::string& masterKey);
    
    int checkCapability(int uid, const std::string& capability);

    bool grantCapability(int uid, const std::string& packageName, const std::string& capability, int isGranted, long expiresAt);

    bool revokeCapability(int uid, const std::string& capability);

    bool logAudit(int uid, const std::string& capability, long timestamp, int status, const std::string& details);

    std::vector<aidl::rfcore::daemon::PolicyRecord> getPolicies();

    std::vector<aidl::rfcore::daemon::AuditRecord> getAuditLogs(int limit, int offset);

private:
    PolicyDatabase() = default;
    ~PolicyDatabase();
    
    PolicyDatabase(const PolicyDatabase&) = delete;
    PolicyDatabase& operator=(const PolicyDatabase&) = delete;

    sqlite3* mDb = nullptr;
    
    bool executeSql(const std::string& sql);
};

}
}
