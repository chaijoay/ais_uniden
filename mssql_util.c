#include <stdio.h>
#include <string.h>

#include "mssql_util.h"
#include "uniden_rep.h"
#include "strlogutl.h"

#include "procsig.h"


DBINFO gDbInfo[NUM_OF_DB];

#define HANDLE_ERROR(r, f, s, t) \
if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) \
{ \
extract_error(f, s, t, __LINE__, __func__); \
}

void extract_error (char *fn, SQLHANDLE handle, SQLSMALLINT type, int line, const char *func)
{

    SQLINTEGER i = 0;
    SQLINTEGER NativeError;
    SQLCHAR SQLState[ 7 ];
    SQLCHAR MessageText[256];
    SQLSMALLINT TextLength;
    SQLRETURN ret;

    printf("\nError!!! In '%s' (line no. %d) ", func, line-1);
    printf("ODBC reported following error for %s : ", fn);

    do
    {
        ret = SQLGetDiagRec(type, handle, ++i, SQLState, &NativeError, MessageText, sizeof(MessageText), &TextLength);
        if ( SQL_SUCCEEDED(ret) ) {
            printf("\n (%s %ld %ld) => %s\n\n", SQLState, (long)i, (long)NativeError, MessageText);
        }
    }
    while ( ret == SQL_SUCCESS );

}

void setDbInfo(char db_inf1[][SIZE_ITEM_L], int n_item1, char db_inf2[][SIZE_ITEM_L], int n_item2)
{

    memset(gDbInfo, 0x00, sizeof(gDbInfo));

    strcpy(gDbInfo[0].szDriver, db_inf1[E_IM_DRIVER]);
    strcpy(gDbInfo[0].szServer, db_inf1[E_IM_SERVER]);
    strcpy(gDbInfo[0].szDbName, db_inf1[E_IM_DBS_NAME]);
    strcpy(gDbInfo[0].szTabPrf, db_inf1[E_IM_TAB_PREF]);
    strcpy(gDbInfo[0].szUsr,    db_inf1[E_IM_DB_USR]);
    strcpy(gDbInfo[0].szPwd,    db_inf1[E_IM_DB_PWD]);

    strcpy(gDbInfo[1].szDriver, db_inf2[E_AW_DRIVER]);
    strcpy(gDbInfo[1].szServer, db_inf2[E_AW_SERVER]);
    strcpy(gDbInfo[1].szDbName, db_inf2[E_AW_DBS_NAME]);
    strcpy(gDbInfo[1].szTabPrf, db_inf2[E_AW_TAB_PREF]);
    strcpy(gDbInfo[1].szUsr,    db_inf2[E_AW_DB_USR]);
    strcpy(gDbInfo[1].szPwd,    db_inf2[E_AW_DB_PWD]);

}


void getTableList()
{
    char sqlTabList[SIZE_BUFF] = {0};
    char tabName[SIZE_ITEM_S] = {0}, tabNameTmp[SIZE_ITEM_S];
    char tabPrfOut[SIZE_DATE_TIME];
    int  i, j, nTabCnt, pref_len;
    char connStr[SIZE_ITEM_X] = {0}, *p = NULL;

    SQLHSTMT    stmt_tab;
    SQLRETURN   ret;
    SQLLEN      tableInd;
    SQLHENV     env;
    SQLHDBC     dbc_tab;

    writeLog(LOG_INF, "getTableList: preparing mtc table list ...");
    for ( i=0; i<NUM_OF_DB; i++ ) {

        memset(tabPrfOut, 0x00, sizeof(tabPrfOut));
        getTokenItem(gDbInfo[i].szTabPrf, 1, '_', tabPrfOut);
        pref_len = strlen(tabPrfOut);

        ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
            HANDLE_ERROR(ret, "SQLAllocHandle", env, SQL_HANDLE_ENV);
        ret = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void *)SQL_OV_ODBC3, 0);
            HANDLE_ERROR(ret, "SQLSetEnvAttr", env, SQL_HANDLE_ENV);

        sprintf(connStr, "Driver=%s; Server=%s; Database=%s; Uid=%s; Pwd=%s;", gDbInfo[i].szDriver, gDbInfo[i].szServer, gDbInfo[i].szDbName, gDbInfo[i].szUsr, gDbInfo[i].szPwd);
        writeLog(LOG_DB3, "connStr: %s", connStr);

        ret = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc_tab);
            HANDLE_ERROR(ret, "SQLAllocHandle", dbc_tab, SQL_HANDLE_DBC);
        ret = SQLDriverConnect(dbc_tab, NULL, connStr, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_COMPLETE);
            HANDLE_ERROR(ret, "SQLDriverConnect", dbc_tab, SQL_HANDLE_DBC);

        sprintf(sqlTabList,
            "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES " \
            "WHERE TABLE_TYPE = 'BASE TABLE' AND TABLE_CATALOG = '%s' AND TABLE_NAME LIKE '%s%%' " \
            "ORDER BY TABLE_NAME; ", gDbInfo[i].szDbName, gDbInfo[i].szTabPrf);
        writeLog(LOG_DB3, "getTableList: sqlTabList: %s", sqlTabList);

        ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc_tab, &stmt_tab);
            HANDLE_ERROR(ret, "SQLAllocHandle", dbc_tab, SQL_HANDLE_STMT);
        ret = SQLExecDirect(stmt_tab, (SQLCHAR*) sqlTabList, SQL_NTS);
            HANDLE_ERROR(ret, "SQLExecDirect", stmt_tab, SQL_HANDLE_STMT);
        ret = SQLBindCol(stmt_tab, 1, SQL_C_CHAR, tabName, sizeof(tabName), &tableInd);
            HANDLE_ERROR(ret, "SQLBindCol", stmt_tab, SQL_HANDLE_STMT);

        /* Fetch and print each row of data until SQL_NO_DATA returned. */
        nTabCnt = 0;
        memset(tabNameTmp, 0x00, sizeof(tabNameTmp));
        for ( j = 0; ; j++ ) {
            ret = SQLFetch(stmt_tab);
            if ( ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO ) {
                if ( *tabNameTmp == '\0' ) {
                    strcpy(tabNameTmp, tabName);
                }
                else {
                    if ( strncmp(tabNameTmp, tabName, strlen(tabNameTmp)-pref_len) == 0 ) {
//printf("%s %s\n", tabNameTmp, tabName);
                        p = NULL; p = strstr(tabNameTmp, "20");
                        sprintf(gDbInfo[i].aOutTabName[nTabCnt], "%s_%.8s", tabPrfOut, p);
                        sprintf(gDbInfo[i].aSqlSubQry[nTabCnt], "SELECT DIALED_DIGIT, EVENT_DURATION FROM %s UNION SELECT DIALED_DIGIT, EVENT_DURATION FROM %s", tabNameTmp, tabName);
                        writeLog(LOG_DB3, "\taSqlSubQry: %s", gDbInfo[i].aSqlSubQry[nTabCnt]);
                        memset(tabNameTmp, 0x00, sizeof(tabNameTmp));
                        ++nTabCnt;
                    }
                    else {
//printf("%s\n", tabNameTmp);
                        p = NULL; p = strstr(tabNameTmp, "20");
                        sprintf(gDbInfo[i].aOutTabName[nTabCnt], "%s_%.8s", tabPrfOut, p);
                        sprintf(gDbInfo[i].aSqlSubQry[nTabCnt], "SELECT DIALED_DIGIT, EVENT_DURATION FROM %s", tabNameTmp);
                        writeLog(LOG_DB3, "\taSqlSubQry: %s", gDbInfo[i].aSqlSubQry[nTabCnt]);
                        strcpy(tabNameTmp, tabName);
                        ++nTabCnt;
                    }
//printf("(%d-%d: %d) %s\t%s\n", i, j, nTabCnt-1, gDbInfo[i].aOutTabName[nTabCnt-1], gDbInfo[i].aSqlSubQry[nTabCnt-1]);
                }

            }
            else {
                if ( ret != SQL_NO_DATA ) {
                    HANDLE_ERROR(ret, "SQLFetch", stmt_tab, SQL_HANDLE_STMT);
                    writeLog(LOG_ERR, "getTableList: fetch error");
                    break;
                }
                else {
                    break;
                }
            }
        }

        if ( strncmp(tabNameTmp, tabName, strlen(tabNameTmp)-pref_len) == 0 ) {
            //printf("%s %s\n", tabNameTmp, tabName);
            p = NULL; p = strstr(tabNameTmp, "20");
            sprintf(gDbInfo[i].aOutTabName[nTabCnt], "%s_%.8s", tabPrfOut, p);
            sprintf(gDbInfo[i].aSqlSubQry[nTabCnt], "SELECT DIALED_DIGIT, EVENT_DURATION FROM %s UNION SELECT DIALED_DIGIT, EVENT_DURATION FROM %s", tabNameTmp, tabName);
            writeLog(LOG_DB3, "\taSqlSubQry: %s", gDbInfo[i].aSqlSubQry[nTabCnt]);
            ++nTabCnt;
        }

        // Cleanup
        if ( stmt_tab != SQL_NULL_HSTMT ) {
            SQLFreeHandle(SQL_HANDLE_STMT, stmt_tab);
        }

        if ( dbc_tab != SQL_NULL_HDBC ) {
            SQLDisconnect(dbc_tab);
            SQLFreeHandle(SQL_HANDLE_DBC, dbc_tab);
        }
        if ( env != SQL_NULL_HENV ) {
            SQLFreeHandle(SQL_HANDLE_ENV, env);
        }

        gDbInfo[i].nTabCnt = nTabCnt;
        writeLog(LOG_INF, "getTableList: prepare mtc table list done for '%s', total table %d", gDbInfo[i].szDbName, gDbInfo[i].nTabCnt);
    }
#if 0
    for ( i=0; i<NUM_OF_DB; i++ ) {
        for ( j=0; j<gDbInfo[i].nTabCnt; j++ ) {
            printf("[%d,%d] table: %s\n", i, j, gDbInfo[i].aOutTabName[j]);
            printf("\t subQ: %s\n", gDbInfo[i].aSqlSubQry[j]);
        }
        printf("=Total %d=\n", gDbInfo[i].nTabCnt);
    }
#endif
}

void buildMtcUsageTab(const char *date_to_build)
{
    char sqlSumUsage[SIZE_BUFF] = {0};
    char mobNo[SIZE_ITEM_T] = {0};
    int cdrUsage, i = 0, j = 0, k = 0;
    char connStr[SIZE_ITEM_X] = {0};
    int can_continue = 1;

    SQLHSTMT    stmt_cdr;
    SQLRETURN   ret;
    SQLLEN      cdrInd;
    SQLHENV     env;
    SQLHDBC     dbc_cdr;
#if 0
    for ( i=0; i<NUM_OF_DB; i++ ) {
        for ( j=0; j<gDbInfo[i].nTabCnt; j++ ) {
            //printf("(%d:%d) %s", i, j, gDbInfo[i].aSqlSubQry[j]);
            if ( gDbInfo[i].aSqlSubQry[j][0] == '\0' ) {
                continue;
            }
            writeLog(LOG_DB3, "buildMtcUsageTab: "\
                "SELECT U.DIALED_DIGIT, SUM(U.EVENT_DURATION) FROM " \
                "( %s ) U " \
                "WHERE LEN(U.DIALED_DIGIT) >= 10 AND LEN(U.DIALED_DIGIT) <= 12 AND SUBSTRING(U.DIALED_DIGIT, 1, 2) > '02' " \
                "GROUP BY U.DIALED_DIGIT; ", gDbInfo[i].aSqlSubQry[j]);
        }
        printf("\n");
    }
    return;

#endif
    writeLog(LOG_INF, "buildMtcUsageTab: building mtc usage database ...");
    for ( i=0; i<NUM_OF_DB; i++ ) {

        can_continue = 1;   // reset every

        ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
            HANDLE_ERROR(ret, "SQLAllocHandle", env, SQL_HANDLE_ENV);
        ret = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void *)SQL_OV_ODBC3, 0);
            HANDLE_ERROR(ret, "SQLSetEnvAttr", env, SQL_HANDLE_ENV);

        sprintf(connStr, "Driver=%s; Server=%s; Database=%s; Uid=%s; Pwd=%s;", gDbInfo[i].szDriver, gDbInfo[i].szServer, gDbInfo[i].szDbName, gDbInfo[i].szUsr, gDbInfo[i].szPwd);
        writeLog(LOG_DB3, "buildMtcUsageTab: connStr: %s", connStr);

        ret = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc_cdr);
            HANDLE_ERROR(ret, "SQLAllocHandle", dbc_cdr, SQL_HANDLE_DBC);
        ret = SQLDriverConnect(dbc_cdr, NULL, connStr, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_COMPLETE);
            HANDLE_ERROR(ret, "SQLDriverConnect", dbc_cdr, SQL_HANDLE_DBC);
//printf("total table count = %d\n", gDbInfo[i].nTabCnt);

        for ( j=0; j<gDbInfo[i].nTabCnt; j++ ) {

            if ( gDbInfo[i].aSqlSubQry[j][0] == '\0' ) {
                continue;
            }
#if 0
            if ( !can_continue ) {
                break;  // stop after processed the matched table name. (only specified date mode)
            }
#endif
            if ( date_to_build != NULL && *date_to_build != '\0' ) {    // build by specific date (specified date mode)
                if ( strstr(gDbInfo[i].aOutTabName[j], date_to_build) != NULL ) {
                    can_continue = 0;   // if date_to_build is matched, set can_continue to stop for next loop
                    writeLog(LOG_INF, "buildMtcUsageTab: building mtc usage table %s", gDbInfo[i].aOutTabName[j]);
                }
                else {
                    continue;   // if not match, find next match
                }
            }

//printf("[%d-%d] %s\n", i, j, gDbInfo[i].aSqlSubQry[j]);
            memset(sqlSumUsage, 0x00, sizeof(sqlSumUsage));
            sprintf(sqlSumUsage,
                "SELECT U.DIALED_DIGIT, SUM(U.EVENT_DURATION) FROM " \
                "( %s ) U " \
                "WHERE LEN(U.DIALED_DIGIT) >= 10 AND LEN(U.DIALED_DIGIT) <= 12 AND SUBSTRING(U.DIALED_DIGIT, 1, 2) > '02' " \
                "GROUP BY U.DIALED_DIGIT; ", gDbInfo[i].aSqlSubQry[j]);
            writeLog(LOG_DB3, "buildMtcUsageTab: %s", sqlSumUsage);
//printf("[%d-%d] sqlSumUsage => [%s]\n", i, j, sqlSumUsage);

            ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc_cdr, &stmt_cdr);
                HANDLE_ERROR(ret, "SQLAllocHandle", dbc_cdr, SQL_HANDLE_STMT);
            ret = SQLExecDirect(stmt_cdr, (SQLCHAR*) sqlSumUsage, SQL_NTS);
                HANDLE_ERROR(ret, "SQLExecDirect", stmt_cdr, SQL_HANDLE_STMT);

            ret = SQLBindCol (stmt_cdr, 1, SQL_C_CHAR, mobNo, sizeof(mobNo), &cdrInd);
                HANDLE_ERROR(ret, "SQLBindCol", stmt_cdr, SQL_HANDLE_STMT);
            ret = SQLBindCol (stmt_cdr, 2, SQL_C_ULONG, &cdrUsage, 0, &cdrInd);
                HANDLE_ERROR(ret, "SQLBindCol", stmt_cdr, SQL_HANDLE_STMT);

            time_t start_time = time(NULL);
            createTableMtc(gDbInfo[i].aOutTabName[j]);
            for ( k = 0; ; k++ ) {
                ret = SQLFetch(stmt_cdr);
                if ( ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO ) {
                    insertMtc(gDbInfo[i].aOutTabName[j], mobNo, cdrUsage, 0);
//printf("\t %s, %d\n", mobNo, cdrUsage);
                }
                else {
                    if ( ret != SQL_NO_DATA ) {
                        HANDLE_ERROR(ret, "SQLFetch", stmt_cdr, SQL_HANDLE_STMT);
                        break;
                    }
                    else {
                        insertMtc(gDbInfo[i].aOutTabName[j], mobNo, cdrUsage, 1);
                        break;
                    }
                }
                if ( isTerminated() == TRUE ) {
                    break;
                }
            }
            createIndexMtc(gDbInfo[i].aOutTabName[j]);
            time_t stop_time = time(NULL);
            writeLog(LOG_INF, "time taken %ld secs %s", (stop_time-start_time), gDbInfo[i].aOutTabName[j]);

            // Cleanup
            if ( stmt_cdr != SQL_NULL_HSTMT ) {
                SQLFreeHandle(SQL_HANDLE_STMT, stmt_cdr);
            }
            if ( isTerminated() == TRUE ) {
                break;
            }
        }

        if ( stmt_cdr != SQL_NULL_HSTMT ) {
            SQLFreeHandle(SQL_HANDLE_STMT, stmt_cdr);
        }
        if ( dbc_cdr != SQL_NULL_HDBC ) {
            SQLDisconnect(dbc_cdr);
            SQLFreeHandle(SQL_HANDLE_DBC, dbc_cdr);
        }
        if ( env != SQL_NULL_HENV ) {
            SQLFreeHandle(SQL_HANDLE_ENV, env);
        }
        if ( isTerminated() == TRUE ) {
            break;
        }

    }
    writeLog(LOG_INF, "buildMtcUsageTab: build mtc usage database done");
}
