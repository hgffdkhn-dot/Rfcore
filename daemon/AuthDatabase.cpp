#include "AuthDatabase.h"
#include <sys/stat.h>
#include <android/log.h>
#include <iostream>

#define LOG_TAG "RFCoreAuthDB"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace aidl {
namespace rfcore {
namespace daemon {

AuthDatabase& AuthDatabase::getInstance() {
    static AuthDatabase instance;
    return instance;
}

AuthDatabase::AuthDatabase() : db(nullptr) {
    init();
}

AuthDatabase::~AuthDatabase() {
    if (db) sqlite3_close(db);
}

bool AuthDatabase::init() {
    std::lock_guard<std::mutex> lock(dbMutex);
    
    // 强制使用极其安全的 adb 数据目录
    const char* dirPath = "/data/adb/rfcore";
    const char* dbPath = "/data/adb/rfcore/rfcore.db";

    struct stat st = {0};
    if (stat(dirPath, &st) == -1) {
        mkdir(dirPath, 0700);
    }

    if (sqlite3_open(dbPath, &db) != SQLITE_OK) {
        LOGE("无法打开数据库: %s", sqlite3_errmsg(db));
        return false;
    }

    // 建立记忆表
    const char* sql = 
        "CREATE TABLE IF NOT EXISTS policies ("
        "uid INTEGER, "
        "package_name TEXT, "
        "capability TEXT, "
        "is_granted INTEGER, "
        "expires_at INTEGER, "
        "PRIMARY KEY (uid, capability));";

    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        LOGE("创建表失败: %s", errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

int AuthDatabase::getPolicy(int uid, const std::string& capability) {
    std::lock_guard<std::mutex> lock(dbMutex);
    if (!db) return -1;

    const char* sql = "SELECT is_granted FROM policies WHERE uid = ? AND capability = ?;";
    sqlite3_stmt* stmt;
    int result = -1;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, uid);
        sqlite3_bind_text(stmt, 2, capability.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

bool AuthDatabase::setPolicy(int uid, const std::string& packageName, const std::string& capability, int isGranted) {
    std::lock_guard<std::mutex> lock(dbMutex);
    if (!db) return false;

    const char* sql = "REPLACE INTO policies (uid, package_name, capability, is_granted, expires_at) VALUES (?, ?, ?, ?, 0);";
    sqlite3_stmt* stmt;
    bool success = false;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, uid);
        sqlite3_bind_text(stmt, 2, packageName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, capability.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, isGranted);

        if (sqlite3_step(stmt) == SQLITE_DONE) success = true;
        sqlite3_finalize(stmt);
    }
    return success;
}

std::vector<PolicyRecord> AuthDatabase::getAllPolicies() {
    std::lock_guard<std::mutex> lock(dbMutex);
    std::vector<PolicyRecord> policies;
    if (!db) return policies;

    const char* sql = "SELECT uid, package_name, capability, is_granted, expires_at FROM policies;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            PolicyRecord record;
            record.uid = sqlite3_column_int(stmt, 0);
            const unsigned char* pkgText = sqlite3_column_text(stmt, 1);
            record.packageName = pkgText ? reinterpret_cast<const char*>(pkgText) : "";
            const unsigned char* capText = sqlite3_column_text(stmt, 2);
            record.capability = capText ? reinterpret_cast<const char*>(capText) : "";
            record.isGranted = sqlite3_column_int(stmt, 3);
            record.expiresAt = sqlite3_column_int64(stmt, 4);
            policies.push_back(record);
        }
        sqlite3_finalize(stmt);
    }
    return policies;
}

}  // namespace daemon
}  // namespace rfcore
}  // namespace aidl
