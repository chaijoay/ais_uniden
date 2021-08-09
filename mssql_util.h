
#ifndef __MS_SQL_UTIL_H__
#define __MS_SQL_UTIL_H__

#ifdef  __cplusplus
    extern "C" {
#endif

#include <sql.h>
#include <sqlext.h>
#include "glb_str_def.h"

#define NUM_OF_DB   2

typedef struct {
    char szDriver[SIZE_ITEM_S];
    char szServer[SIZE_ITEM_S];
    char szDbName[SIZE_ITEM_S];
    char szTabPrf[SIZE_ITEM_S];
    char szUsr[SIZE_ITEM_S];
    char szPwd[SIZE_ITEM_S];

    char aSqlSubQry[SIZE_ITEM_M][SIZE_ITEM_L];
    char aOutTabName[SIZE_ITEM_M][SIZE_ITEM_T];
    int  nTabCnt;

} DBINFO;


// #define DB_DRIVER_1         "{ODBC Driver 17 for SQL Server}"
// #define DB_SERVER_1         "10.251.56.45"  // Nami
// #define DB_NAME_1           "RawCDR2_IMS"
// #define DB_USER_1           "cdr"
// #define DB_PSWD_1           "cdrdl380g3"
// #define DB_TAB_PREF_1       "IMS_2"
//
// #define DB_SERVER_2         "10.251.56.27"  // Benten
// #define DB_NAME_2           "RawCDR2_VOICE"
// #define DB_USER_2           "cdr"
// #define DB_PSWD_2           "cdrdl380g3"
// #define DB_TAB_PREF_2       "VOICE_AWN_2"

void extract_error (char *fn, SQLHANDLE handle, SQLSMALLINT type, int line, const char *func);
void setDbInfo(char db_inf1[][SIZE_ITEM_L], int n_item1, char db_inf2[][SIZE_ITEM_L], int n_item2);
void getTableList();
void buildMtcUsageTab(const char *date_to_build);

#ifdef  __cplusplus
    }
#endif


#endif  // __MS_SQL_UTIL_H__