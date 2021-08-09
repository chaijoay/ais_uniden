
#include <sqlite3.h>

#define SQLITE_MAX_COMMIT       25000
#define UNIDEN_SQLITE_DB_FILE   ".mtc_usage_for_uniden.db"

void    setSqlDbPath(const char *db_file_dir);
int     createTableMtc(const char *tabName);
void    createIndexMtc(const char *tabName);
void    insertMtc(const char *tabName, const char *val1, int val2, int force_commit);

int     haveUsageMtc(const char *mobNum, const char *today_date, const char *cond_str, int search_bck);

int     createMapTable(const char *tabName, const char *inp_file);
int     lookupMapTable(const char *tabName, const char *key);
int     getBillCycle(const char *tabName, const char *day, char *bc);

void    purgeOldDataMtc(int day_to_keep);

void    sqlite_cleanup();
