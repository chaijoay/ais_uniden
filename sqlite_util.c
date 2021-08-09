
#include <time.h>
#include "sqlite_util.h"
#include "mssql_util.h"
#include "glb_str_def.h"
#include "strlogutl.h"
#include "oradb_util.h"


sqlite3 *g_SqliteDb = NULL;
sqlite3_stmt *g_stmt1 = NULL;
sqlite3_stmt *g_stmt2 = NULL;

char    *g_SqliteErr_msg = NULL;
char    gsdb_file_path[SIZE_ITEM_L+30];

char gszColName1[] = "MOBILE_NUM";
char gszColName2[] = "USAGE";

extern DBINFO gDbInfo[NUM_OF_DB];
extern PROC_STAT proc_stat[E_TOTAL_STAT];

void setSqlDbPath(const char *db_file_dir)
{
    memset(gsdb_file_path, 0x00, sizeof(gsdb_file_path));
    if ( db_file_dir == NULL ) {
        sprintf(gsdb_file_path, "./%s", UNIDEN_SQLITE_DB_FILE);
    }
    else {
        sprintf(gsdb_file_path, "%s/%s", db_file_dir, UNIDEN_SQLITE_DB_FILE);
    }
    writeLog(LOG_DB3, "setSqlDbPath: sqlite db file: %s", gsdb_file_path);
}


int createTableMtc(const char *tabName)
{
    int rc = SQLITE_OK;

    if ( g_SqliteDb == NULL ) {
//printf("open @ createTableMtc\n");
        rc = sqlite3_open(gsdb_file_path, &g_SqliteDb);
    }

    if ( rc != SQLITE_OK ) {
        writeLog(LOG_ERR, "createTableMtc: cannot open sqlite dbs: %s", sqlite3_errmsg(g_SqliteDb));
        return FAILED;
    }

    char sql[SIZE_ITEM_X]; memset(sql, 0x00, sizeof(sql));
    sprintf(sql,
         "CREATE TABLE IF NOT EXISTS %s ( "
         "%s TEXT NOT NULL UNIQUE, "
         "%s INTEGER NOT NULL "
         "); "
         "DELETE FROM %s; ",
         tabName, gszColName1, gszColName2, tabName);
    writeLog(LOG_DB3, "createTableMtc: create table: %s", sql);

    rc = sqlite3_exec(g_SqliteDb, sql, 0, 0, &g_SqliteErr_msg);
    if ( rc != SQLITE_OK ) {
        writeLog(LOG_ERR, "sql error: %s", g_SqliteErr_msg);
        sqlite3_free(g_SqliteErr_msg);
        g_SqliteErr_msg = NULL;
        sqlite3_close(g_SqliteDb);
        g_SqliteDb = NULL;
        return FAILED;
    }

    if ( g_SqliteErr_msg != NULL ) {
        sqlite3_free(g_SqliteErr_msg);
        g_SqliteErr_msg = NULL;
    }
    writeLog(LOG_INF, "createTableMtc: table %s created", tabName);

    return SUCCESS;
}

void createIndexMtc(const char *tabName)
{
    int rc = SQLITE_OK;
    char sql[SIZE_ITEM_X]; memset(sql, 0x00, sizeof(sql));

    if ( g_SqliteDb == NULL ) {
//printf("open @ createIndexMtc\n");
        rc = sqlite3_open(gsdb_file_path, &g_SqliteDb);
    }

    if ( rc != SQLITE_OK ) {
        writeLog(LOG_ERR, "createIndexMtc: cannot open sqlite dbs: %s", sqlite3_errmsg(g_SqliteDb));
        return;
    }

    sprintf(sql, "CREATE UNIQUE INDEX IF NOT EXISTS IDX_%s ON %s(%s); ", tabName, tabName, gszColName1);
    writeLog(LOG_DB3, "createIndexMtc: %s", sql);

    rc = sqlite3_exec(g_SqliteDb, sql, 0, 0, &g_SqliteErr_msg);

    if ( rc != SQLITE_OK ) {
        writeLog(LOG_ERR, "createIndexMtc: sql error: %s", g_SqliteErr_msg);
        sqlite3_free(g_SqliteErr_msg);
        g_SqliteErr_msg = NULL;
        sqlite3_close(g_SqliteDb);
        g_SqliteDb = NULL;
        //return FAILED;
    }

    if ( g_SqliteErr_msg != NULL ) {
        sqlite3_free(g_SqliteErr_msg);
        g_SqliteErr_msg = NULL;
    }
    writeLog(LOG_INF, "createIndexMtc: index(%s) created", tabName);

    if ( g_SqliteDb != NULL ) {
        sqlite3_close(g_SqliteDb);
        g_SqliteDb = NULL;
    }

    //return SUCCESS;
}

void insertMtc(const char *tabName, const char *val1, int val2, int force_commit)
{
    static int cnt = 0, first = 1, commit_cnt = 0;
    int rc = SQLITE_OK;
    char sql[SIZE_ITEM_M];
    //static sqlite3_stmt *stmt = NULL;

    if ( g_SqliteDb == NULL ) {
//printf("open @ insertMtc\n");
        rc = sqlite3_open(gsdb_file_path, &g_SqliteDb);
    }

    if ( rc != SQLITE_OK ) {
        writeLog(LOG_ERR, "insertMtc: cannot open sqlite dbs: %s", sqlite3_errmsg(g_SqliteDb));
        return;
    }

    if ( first ) {
        sprintf(sql, "INSERT INTO %s(%s, %s) VALUES(?1, ?2);", tabName, gszColName1, gszColName2);
        sqlite3_prepare_v2(g_SqliteDb, sql, -1, &g_stmt1, NULL);
//printf("prepare @ insertMtc\n");
        writeLog(LOG_DB2, "insertMtc: prepared_stmt: %s", sql);
        first = 0;
    }

    if ( cnt == 0 ) {
        memset(sql, 0x00, sizeof(sql));
        sqlite3_exec(g_SqliteDb, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    }

    sqlite3_bind_text(g_stmt1, 1, val1, -1, NULL);
    sqlite3_bind_int (g_stmt1, 2, val2);
    sqlite3_step(g_stmt1);   // do insert
    sqlite3_reset(g_stmt1);  // reset for next run
    cnt++;

    if ( cnt >= SQLITE_MAX_COMMIT || force_commit ) {
        commit_cnt += cnt;
        sqlite3_exec(g_SqliteDb, "COMMIT;", NULL, NULL, NULL);
        writeLog(LOG_DB3, "insertMtc: %s commit @ %d %d", tabName, cnt, commit_cnt);
        if ( force_commit ) {
            if ( g_stmt1 != NULL ) {
                sqlite3_finalize(g_stmt1);
                g_stmt1 = NULL;
            }
            writeLog(LOG_INF, "insertMtc: table %s have %d records", tabName, commit_cnt);
            first = 1;
            commit_cnt = 0;
        }
        // sqlite3_close(g_SqliteDb);
        cnt = 0;
    }
    //return SUCCESS;

}

int haveUsageMtc(const char *mobNum, const char *today_date, const char *cond_str, int search_bck)
{
    long duration = 0, i, rc = SQLITE_OK;
    int num_item = 0;
    sqlite3_stmt *stmt = NULL;
    sqlite3_stmt *stmt2 = NULL;
    char sql[SIZE_ITEM_L], sql2[SIZE_ITEM_L], temp_date[SIZE_DATE_ONLY+1], table[SIZE_ITEM_T];
    char cond_rec[SIZE_ITEM_M], *item[10];
    struct timespec start_t, stop_t;

proc_stat[E_MTC_USG].n_call_count++;
clock_gettime(CLOCK_MONOTONIC_RAW, &start_t);

    memset(sql, 0x00, sizeof(sql));

    if ( g_SqliteDb == NULL ) {
//printf("open @ haveUsageMtc\n");
        rc = sqlite3_open(gsdb_file_path, &g_SqliteDb);
    }

    if ( rc != SQLITE_OK ) {
        writeLog(LOG_ERR, "haveUsageMtc: cannot open sqlite dbs: %s", sqlite3_errmsg(g_SqliteDb));
clock_gettime(CLOCK_MONOTONIC_RAW, &stop_t);
proc_stat[E_MTC_USG].l_sum_use_ms += get_millsec_delta(&stop_t, &start_t);
        return FAILED;
    }

    if ( cond_str != '\0' ) {
        strcpy(cond_rec, cond_str);
        num_item = getTokenAll(item, 10, cond_rec, ',');
//printf("cond_str %s, %s, %s (%d)\n", cond_str, item[0], item[1], num_item);
    }

    for ( i=0; i<search_bck; i++ ) {
        memset(temp_date, 0x00, sizeof(temp_date));
        strcpy(temp_date, dateAdd(today_date, DTM_DATE_ONLY, 0, 0, (-1)*i));

        sprintf(sql, "SELECT NAME FROM SQLITE_MASTER WHERE TYPE ='table' AND  NAME NOT LIKE 'sqlite_%c' AND NAME LIKE '%c%s%c';", '%', '%', temp_date, '%');
        writeLog(LOG_DB3, "haveUsageMtc: %s", sql);
        if ( sqlite3_prepare_v2(g_SqliteDb, sql, -1, &g_stmt1, NULL) ) {
            if ( g_stmt1 != NULL ) {
                sqlite3_finalize(g_stmt1);
                g_stmt1 = NULL;
            }
           //sqlite3_Error executing sqlclose(g_SqliteDb);
clock_gettime(CLOCK_MONOTONIC_RAW, &stop_t);
proc_stat[E_MTC_USG].l_sum_use_ms += get_millsec_delta(&stop_t, &start_t);
           return FAILED;
        }
//printf("prepare @ haveUsageMtc 1\n");
        while ( (rc = sqlite3_step(g_stmt1)) == SQLITE_ROW ) {

            memset(table, 0x00, sizeof(table));
            sprintf(table, "%s", sqlite3_column_text(g_stmt1, 0));

            memset(sql2, 0x00, sizeof(sql2));
            sprintf(sql2, "SELECT %s FROM %s WHERE %s = ?", gszColName2, table, gszColName1);
            writeLog(LOG_DB2, "haveUsageMtc: %s", sql2);

            if ( sqlite3_prepare_v2(g_SqliteDb, sql2, -1, &g_stmt2, NULL) ) {
                if ( g_stmt2 != NULL ) {
                    writeLog(LOG_ERR, "haveUsageMtc: prepared stmt2: %s", sql2);
                    sqlite3_finalize(g_stmt2);
                    g_stmt2 = NULL;
                }
               //sqlite3_Error executing sqlclose(g_SqliteDb);
clock_gettime(CLOCK_MONOTONIC_RAW, &stop_t);
proc_stat[E_MTC_USG].l_sum_use_ms += get_millsec_delta(&stop_t, &start_t);
               return FAILED;
            }
//printf("prepare @ haveUsageMtc 2\n");
            sqlite3_bind_text(g_stmt2, 1, mobNum, -1, NULL);

            if ( sqlite3_step(g_stmt2) == SQLITE_ROW ) {
                duration = sqlite3_column_int(g_stmt2, 0);
                sqlite3_finalize(g_stmt2);
                g_stmt2 = NULL;
            }
            writeLog(LOG_DB3, "haveUsageMtc: mobNum:%s table:%s duration:%d (min:%ld max:%ld) (%s)", mobNum, table, duration, atol(item[0]), atol(item[1]), cond_str);

            if ( *item[1] == '\0' ) {
//printf("a(%ld, %ld),", duration, atol(item[0]));
                writeLog(LOG_DB2, "mtc duration %ld ini %ld", duration, atol(item[0]));
                if ( duration > atol(item[0]) ) {
//printf("b,");
                    if ( g_stmt2 != NULL ) {
                        sqlite3_finalize(g_stmt2);
                        g_stmt2 = NULL;
                    }
//printf("c: %d, %s\n", duration, item[0]);
clock_gettime(CLOCK_MONOTONIC_RAW, &stop_t);
proc_stat[E_MTC_USG].l_sum_use_ms += get_millsec_delta(&stop_t, &start_t);
                    return SUCCESS;
                }
            }
            else {
//printf("1,");
                writeLog(LOG_DB2, "mtc duration %ld ini %ld,%ld", duration, atol(item[0]), atol(item[1]));
                if ( duration >= atol(item[0]) && duration <= atol(item[1]) ) {
//printf("2,");
                    if ( g_stmt2 != NULL ) {
                        sqlite3_finalize(g_stmt2);
                        g_stmt2 = NULL;
                    }
//printf("3: %d, %s, %s\n", duration, item[0], item[1]);
clock_gettime(CLOCK_MONOTONIC_RAW, &stop_t);
proc_stat[E_MTC_USG].l_sum_use_ms += get_millsec_delta(&stop_t, &start_t);
                    return SUCCESS;
                }
            }
//printf("xxx\n");
        }
        sqlite3_reset(g_stmt1);
    }

    if ( g_stmt1 != NULL ) {
       sqlite3_finalize(g_stmt1);
       g_stmt1 = NULL;
    }
    if ( g_stmt2 != NULL ) {
        sqlite3_finalize(g_stmt2);
        g_stmt2 = NULL;
    }
    //sqlite3_close(g_SqliteDb);
clock_gettime(CLOCK_MONOTONIC_RAW, &stop_t);
proc_stat[E_MTC_USG].l_sum_use_ms += get_millsec_delta(&stop_t, &start_t);
    return FAILED;

}


int createMapTable(const char *tabName, const char *inp_file)
{
    FILE *fp = NULL;
    int rc = SQLITE_OK;
    char sql[SIZE_ITEM_X], rec[SIZE_ITEM_L], *item[10];
    int cnt = 0;
    //sqlite3_stmt *stmt;

    if ( g_stmt1 != NULL ) {
        sqlite3_finalize(g_stmt1);
        g_stmt1 = NULL;
    }

    if ( g_SqliteDb == NULL ) {
//printf("open @ createMapTable\n");
        rc = sqlite3_open(gsdb_file_path, &g_SqliteDb);
    }

    if ( rc != SQLITE_OK ) {
        writeLog(LOG_ERR, "createMapTable: cannot open sqlite dbs: %s", sqlite3_errmsg(g_SqliteDb));
        return FAILED;
    }

    memset(sql, 0x00, sizeof(sql));
    sprintf(sql,
         "CREATE TABLE IF NOT EXISTS %s ( "\
         "KEY TEXT, "\
         "INF TEXT "\
         "); "\
         "DELETE FROM %s; ", tabName, tabName);
    writeLog(LOG_DB3, "createMapTable: %s", sql);

    rc = sqlite3_exec(g_SqliteDb, sql, 0, 0, &g_SqliteErr_msg);
    if ( rc != SQLITE_OK ) {
        writeLog(LOG_ERR, "createMapTable: %s (%d)", g_SqliteErr_msg, rc);
        sqlite3_free(g_SqliteErr_msg);
        g_SqliteErr_msg = NULL;
        sqlite3_close(g_SqliteDb);
        g_SqliteDb = NULL;
        return FAILED;
    }
//printf("sqlite: g_SqliteErr_msg => [%s]\n", g_SqliteErr_msg);
    if ( g_SqliteErr_msg != NULL ) {
        sqlite3_free(g_SqliteErr_msg);
        g_SqliteErr_msg = NULL;
    }

    memset(rec, 0x00, sizeof(rec));
    memset(sql, 0x00, sizeof(sql));
    sprintf(sql, "INSERT INTO %s(KEY, INF) VALUES(?1, ?2);", tabName);
    writeLog(LOG_DB3, "createMapTable: %s", sql);

    if ( sqlite3_prepare_v2(g_SqliteDb, sql, -1, &g_stmt1, NULL) ) {
        writeLog(LOG_ERR, "createMapTable: prepared stmt: %s", sql);
        sqlite3_close(g_SqliteDb);
        //return FAILED;
    }
    writeLog(LOG_INF, "createMapTable: creating mapping table %s", tabName);
//printf("prepare @ createMapTable 2\n");
    if ( (fp = fopen(inp_file, "r")) == NULL ) {
        return FAILED;
    }
    while ( fgets(rec, SIZE_ITEM_L, fp) != NULL ) {

        trimStr(rec);
        if ( rec[0] == '#' ) {
            continue;   // skip comment
        }

        memset(item, 0x00, sizeof(item));
        getTokenAll(item, 2, rec, '|');

        sqlite3_bind_text(g_stmt1, 1, item[0], -1, NULL);
        if ( item[1][0] == '\0' ) {
            sqlite3_bind_text(g_stmt1, 2, "", -1, NULL);
        }
        else {
            sqlite3_bind_text(g_stmt1, 2, item[1], -1, NULL);
        }
        rc = sqlite3_step(g_stmt1);   // do insert

        sqlite3_reset(g_stmt1);      // reset for next run
        memset(rec, 0x00, sizeof(rec));
        cnt++;

    }
    fclose(fp); fp = NULL;

    memset(sql, 0x00, sizeof(sql));
    sprintf(sql, "CREATE INDEX IF NOT EXISTS IDX_%s ON %s(KEY); ", tabName, tabName);
    writeLog(LOG_DB3, "createMapTable: %s", sql);
    sqlite3_exec(g_SqliteDb, sql, 0, 0, &g_SqliteErr_msg);

    sqlite3_finalize(g_stmt1);
    g_stmt1 = NULL;
    sqlite3_close(g_SqliteDb);
    g_SqliteDb = NULL;
    writeLog(LOG_INF, "createMapTable: create %s table done (%d)", tabName, cnt);

    return cnt;
}

int lookupMapTable(const char *tabName, const char *key)
{
    //sqlite3_stmt *stmt;
    int count = 0, rc = SQLITE_OK;
    char sql[SIZE_ITEM_X], like_key[SIZE_ITEM_L];
    struct timespec start_t, stop_t;
proc_stat[E_MAP_TAB].n_call_count++;
clock_gettime(CLOCK_MONOTONIC_RAW, &start_t);

    memset(sql, 0x00, sizeof(sql));
    memset(like_key, 0x00, sizeof(like_key));

    sprintf(sql, "SELECT COUNT(KEY) FROM %s WHERE KEY LIKE ?1", tabName);
    sprintf(like_key, "%c%s%c", '%', key, '%');
    writeLog(LOG_DB3, "lookupMapTable: %s (%s)", sql, like_key);

    if ( g_stmt1 != NULL ) {
        sqlite3_finalize(g_stmt1);
        g_stmt1 = NULL;
    }

    if ( g_SqliteDb == NULL ) {
//printf("open @ lookupMapTable\n");
        rc = sqlite3_open(gsdb_file_path, &g_SqliteDb);
//printf("aa (%d)\n", rc);
    }
    if ( rc != SQLITE_OK ) {
        writeLog(LOG_ERR, "lookupMapTable: cannot open sqlite dbs: %s", sqlite3_errmsg(g_SqliteDb));
clock_gettime(CLOCK_MONOTONIC_RAW, &stop_t);
proc_stat[E_MAP_TAB].l_sum_use_ms += get_millsec_delta(&stop_t, &start_t);
        return FAILED;
    }

    //sqlite3_open(gsdb_file_path, &g_SqliteDb);
    if ( rc = sqlite3_prepare_v2(g_SqliteDb, sql, -1, &g_stmt1, NULL) ) {
        writeLog(LOG_ERR, "lookupMapTable: prepared stmt: %s", sql);
//printf("a (%d)\n", rc);
        if ( g_stmt1 != NULL ) {
//printf("b\n");
            sqlite3_finalize(g_stmt1);
            g_stmt1 = NULL;
        }
       //sqlite3_Error executing sqlclose(g_SqliteDb);
clock_gettime(CLOCK_MONOTONIC_RAW, &stop_t);
proc_stat[E_MAP_TAB].l_sum_use_ms += get_millsec_delta(&stop_t, &start_t);
       return FAILED;
    }
//printf("prepare @ lookupMapTable\n");
//printf("c\n");
    sqlite3_bind_text(g_stmt1, 1, like_key, -1, NULL);

    if ( sqlite3_step(g_stmt1) == SQLITE_ROW ) {
        count = sqlite3_column_int(g_stmt1, 0);
//printf("d\n");
    }

    if ( g_stmt1 != NULL ) {
       sqlite3_finalize(g_stmt1);
//printf("e\n");
       g_stmt1 = NULL;
    }
    //sqlite3_close(g_SqliteDb);
clock_gettime(CLOCK_MONOTONIC_RAW, &stop_t);
proc_stat[E_MAP_TAB].l_sum_use_ms += get_millsec_delta(&stop_t, &start_t);
    return count;
}

int getBillCycle(const char *tabName, const char *day, char *bc)
{
    //sqlite3_stmt *stmt;
    int rc = SQLITE_OK;
    char sql[SIZE_ITEM_L];

    memset(sql, 0x00, sizeof(sql));

    if ( g_SqliteDb == NULL ) {
//printf("open @ getBillCycle\n");
        rc = sqlite3_open(gsdb_file_path, &g_SqliteDb);
//printf("aa (%d)\n", rc);
    }
    if ( rc != SQLITE_OK ) {
        writeLog(LOG_ERR, "getBillCycle: cannot open sqlite dbs: %s", sqlite3_errmsg(g_SqliteDb));
        return FAILED;
    }

    // KEY => Billing Cycle
    // INF => Day
    sprintf(sql, "SELECT KEY FROM %s WHERE INF = ?1", tabName);
    writeLog(LOG_DB3, "getBillCycle: %s", sql);

    //sqlite3_open(gsdb_file_path, &g_SqliteDb);
    if ( rc = sqlite3_prepare_v2(g_SqliteDb, sql, -1, &g_stmt1, NULL) ) {
        writeLog(LOG_ERR, "getBillCycle: prepared stmt: %s", sql);
//printf("a (%d)\n", rc);
        if ( g_stmt1 != NULL ) {
//printf("b\n");
            sqlite3_finalize(g_stmt1);
            g_stmt1 = NULL;
        }
       //sqlite3_Error executing sqlclose(g_SqliteDb);
       return FAILED;
    }
//printf("prepare @ getBillCycle\n");
//printf("c\n");
    sqlite3_bind_text(g_stmt1, 1, day, -1, NULL);

    if ( sqlite3_step(g_stmt1) == SQLITE_ROW ) {
        strcpy(bc, sqlite3_column_text(g_stmt1, 0));
//printf("d\n");
    }

    if ( g_stmt1 != NULL ) {
       sqlite3_finalize(g_stmt1);
//printf("e\n");
       g_stmt1 = NULL;
    }
    //sqlite3_close(g_SqliteDb);
    return atoi(bc);
}

void purgeOldDataMtc(int day_to_keep)
{

    int i = -1, rc = SQLITE_OK;

    sqlite3_stmt *stmt = NULL;
    char sql[SIZE_ITEM_L], table[SIZE_ITEM_S];
    char table_date[SIZE_DATE_ONLY], last_date[SIZE_DATE_ONLY];
    char drop_cmd[50][50];

    if ( g_SqliteDb == NULL ) {
//printf("open @ purgeOldDataMtc\n");
        rc = sqlite3_open(gsdb_file_path, &g_SqliteDb);
    }
    if ( rc != SQLITE_OK ) {
        writeLog(LOG_ERR, "purgeOldDataMtc: cannot open sqlite dbs: %s", sqlite3_errmsg(g_SqliteDb));
        return;
    }

    memset(sql, 0x00, sizeof(sql));
    memset(last_date, 0x00, sizeof(last_date));
    memset(drop_cmd, 0x00, sizeof(drop_cmd));

    strcpy(last_date, dateAdd(NULL, DTM_DATE_ONLY, 0, 0, (-1)*day_to_keep));
    sprintf(sql, "SELECT NAME FROM SQLITE_MASTER WHERE TYPE ='table' AND  NAME NOT LIKE 'sqlite_%c' AND NAME LIKE '%c_20%c';", '%', '%', '%');
    writeLog(LOG_DB3, "purgeOldDataMtc: %s", sql);

    //sqlite3_open(gsdb_file_path, &g_SqliteDb);
    if ( sqlite3_prepare_v2(g_SqliteDb, sql, -1, &stmt, NULL) ) {
        if ( stmt != NULL ) {
            sqlite3_finalize(stmt);
            sqlite3_close(g_SqliteDb);
            stmt = NULL;
        }
       //sqlite3_Error executing sqlclose(g_SqliteDb);
       return;
    }
    writeLog(LOG_INF, "oldest day to be kept is %s", last_date);

    while ( (rc = sqlite3_step(stmt)) == SQLITE_ROW ) {

        memset(table, 0x00, sizeof(table));
        memset(table_date, 0x00, sizeof(table_date));

        sprintf(table, "%s", sqlite3_column_text(stmt, 0));
        getTokenItem(table, 2, '_', table_date);

        if ( strcmp(table_date, last_date) < 0 ) {
            ++i;
            sprintf(drop_cmd[i], "DROP TABLE %s; ", table);
        }
        if ( i+1 >= 50 ) {
            break;
        }
    }
    if ( stmt != NULL ) {
       sqlite3_finalize(stmt);
       stmt = NULL;
    }

    for ( ; i >= 0 ; i-- ) {
        writeLog(LOG_INF, "purgeOldDataMtc: %s", drop_cmd[i]);
        if ( sqlite3_exec(g_SqliteDb, drop_cmd[i], 0, 0, &g_SqliteErr_msg) != SQLITE_OK ) {
            writeLog(LOG_ERR, "purgeOldDataMtc: %s\n", g_SqliteErr_msg);
        }
        else {
            writeLog(LOG_INF, "table dropped");
        }
        if ( g_SqliteErr_msg != NULL ) {
            sqlite3_free(g_SqliteErr_msg);
            g_SqliteErr_msg = NULL;
        }
    }

    writeLog(LOG_INF, "purgeOldDataMtc: done");
    if ( g_SqliteErr_msg != NULL ) {
        sqlite3_free(g_SqliteErr_msg);
        g_SqliteErr_msg = NULL;
    }

    sqlite3_close(g_SqliteDb);
    g_SqliteDb = NULL;

}

void sqlite_cleanup()
{
    if ( g_SqliteDb != NULL ) {
        sqlite3_close(g_SqliteDb);
        g_SqliteDb = NULL;
    }
    if  ( g_stmt1 != NULL ) {
        sqlite3_finalize(g_stmt1);
        g_stmt1 = NULL;
    }
    if  ( g_stmt2 != NULL ) {
        sqlite3_finalize(g_stmt2);
        g_stmt2 = NULL;
    }
    if ( g_SqliteErr_msg != NULL ) {
        sqlite3_free(g_SqliteErr_msg);
        g_SqliteErr_msg = NULL;
    }
}