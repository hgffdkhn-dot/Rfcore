#include "PolicyDatabase.h"
#include <android-base/logging.h>

namespace rfcore {
namespace daemon {

PolicyDatabase& PolicyDatabase::getInstance() {
    static PolicyDatabase instance;
    return instance;
}

PolicyDatabase::~PolicyDatabase() {
    if (mDb) {
        sqlite3_close(mDb);
        mDb = nullptr;
    }
}

bool PolicyDatabase::executeSql(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(mDb, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        LOG(ERROR) << errMsg;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool PolicyDatabase::initialize(const std::string& dbPath, const std::string& masterKey) {
    int rc = sqlite3_open(dbPath.c_str(), &mDb);
    if (rc != SQLITE_OK) {
        return false;
    }

    std::string keySql = "PRAGMA key = '" + masterKey + "';";
    executeSql(keySql);

    const std::string createPolicies = 
        "CREATE TABLE IF NOT EXISTS policies ("
        "uid INTEGER NOT NULL, "
        "package_name TEXT NOT NULL, "
        "capability TEXT NOT NULL, "
        "is_granted INTEGER NOT NULL DEFAULT 0, "
        "expires_at INTEGER DEFAULT 0, "
        "PRIMARY KEY (uid, capability));";

    const std::string createAudit = 
        "CREATE TABLE IF NOT EXISTS audit_logs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "uid INTEGER NOT NULL, "
        "capability TEXT NOT NULL, "
        "timestamp INTEGER NOT NULL, "
        "status INTEGER NOT NULL, "
        "details TEXT);";

    executeSql(createPolicies);
    executeSql(createAudit);

    executeSql("CREATE INDEX IF NOT EXISTS idx_policies_uid ON policies(uid);");
    executeSql("CREATE INDEX IF NOT EXISTS idx_audit_logs_timestamp ON audit_logs(timestamp);");

    return true;
}

int PolicyDatabase::checkCapability(int uid, const std::string& capability) {
    std::string sql = "SELECT is_granted FROM policies WHERE uid = ? AND capability = ?;";
    sqlite3_stmt* stmt;
    int isGranted = -1; 

    if (sqlite3_prepare_v2(mDb, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, uid);
        sqlite3_bind_text(stmt, 2, capability.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            isGranted = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return isGranted;
}

bool PolicyDatabase::grantCapability(int uid, const std::string& packageName, const std::string& capability, int isGranted, long expiresAt) {
    std::string sql = "INSERT OR REPLACE INTO policies (uid, package_name, capability, is_granted, expires_at) VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    bool success = false;

    if (sqlite3_prepare_v2(mDb, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, uid);
        sqlite3_bind_text(stmt, 2, packageName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, capability.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, isGranted);
        sqlite3_bind_int64(stmt, 5, expiresAt);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            success = true;
        }
        sqlite3_finalize(stmt);
    }
    return success;
}

bool PolicyDatabase::revokeCapability(int uid, const std::string& capability) {
    std::string sql = "DELETE FROM policies WHERE uid = ? AND capability = ?;";
    sqlite3_stmt* stmt;
    bool success = false;

    if (sqlite3_prepare_v2(mDb, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, uid);
        sqlite3_bind_text(stmt, 2, capability.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            success = true;
        }
        sqlite3_finalize(stmt);
    }
    return success;
}

bool PolicyDatabase::logAudit(int uid, const std::string& capability, long timestamp, int status, const std::string& details) {
    std::string sql = "INSERT INTO audit_logs (uid, capability, timestamp, status, details) VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    bool success = false;

    if (sqlite3_prepare_v2(mDb, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, uid);
        sqlite3_bind_text(stmt, 2, capability.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, timestamp);
        sqlite3_bind_int(stmt, 4, status);
        sqlite3_bind_text(stmt, 5, details.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            success = true;
        }
        sqlite3_finalize(stmt);
    }
    return success;
}

std::vector<aidl::rfcore::daemon::PolicyRecord> PolicyDatabase::getPolicies() {
    std::vector<aidl::rfcore::daemon::PolicyRecord> records;
    std::string sql = "SELECT uid, package_name, capability, is_granted, expires_at FROM policies;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(mDb, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            aidl::rfcore::daemon::PolicyRecord record;
            record.uid = sqlite3_column_int(stmt, 0);
            record.packageName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            record.capability = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            record.isGranted = sqlite3_column_int(stmt, 3);
            record.expiresAt = sqlite3_column_int64(stmt, 4);
            records.push_back(record);
        }
        sqlite3_finalize(stmt);
    }
    return records;
}

std::vector<aidl::rfcore::daemon::AuditRecord> PolicyDatabase::getAuditLogs(int limit, int offset) {
    std::vector<aidl::rfcore::daemon::AuditRecord> logs;
    std::string sql = "SELECT id, uid, capability, timestamp, status, details FROM audit_logs ORDER BY timestamp DESC LIMIT ? OFFSET ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(mDb, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, limit);
        sqlite3_bind_int(stmt, 2, offset);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            aidl::rfcore::daemon::AuditRecord log;
            log.id = sqlite3_column_int(stmt, 0);
            log.uid = sqlite3_column_int(stmt, 1);
            log.capability = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            log.timestamp = sqlite3_column_int64(stmt, 3);
            log.status = sqlite3_column_int(stmt, 4);
            
            const char* detailsText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            log.details = detailsText ? detailsText : "";
            
            logs.push_back(log);
        }
        sqlite3_finalize(stmt);
    }
    return logs;
}

}
}
