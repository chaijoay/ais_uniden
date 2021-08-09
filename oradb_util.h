///
///
/// FACILITY    : create unidentified subscriber report
///
/// FILE NAME   : oradb_util.h
///
/// AUTHOR      : Thanakorn Nitipiromchai
///
/// CREATE DATE : 04-Feb-2021
///
/// CURRENT VERSION NO : 1.0
///
/// LAST RELEASE DATE  : Apr-2021
///
/// MODIFICATION HISTORY :
///     1.0         Apr-2021     First Version
///
///
#ifndef __ORA_DB_DBU_H__
#define __ORA_DB_DBU_H__

#ifdef  __cplusplus
    extern "C" {
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <stdlib.h>
#include <pcre.h>

// #include <sqlca.h>
// #include <sqlda.h>
// #include <sqlcpr.h>

#include "strlogutl.h"
#include "glb_str_def.h"

typedef enum {
    E_PRJ_FMC = 0,
    E_MAP_TAB,
    E_DEA_LER,
    E_BIZ_ORD,
    E_SVC_PRM,
    E_GET_DLR,
    E_GET_ORD,
    E_PRT_PRT,
    E_PRT_LCK,
    E_CQS_USG,
    E_MTC_USG,
    E_TOTAL_STAT
} QUERY_PROC_STAT;

typedef struct {
    int  n_call_count;
    long l_sum_use_ms;
} PROC_STAT;

typedef struct {
    char szName[20];
    char szStatus[20];
    char szPath[SIZE_ITEM_L];
    char szFname[SIZE_ITEM_L];
    char szRemark[SIZE_ITEM_L];
    int  nMapRec;
    int  nRejRec;
    long lSeqNo;
} LOG_DB_INF;

typedef enum {
    E_BILL_CYCL = 0,
    E_BILL_NAME,
    E_NETW_TYPE,
    E_CHNL_DEAL,
    E_CHNL_BIZT,
    E_ORDR_TYPE,
    E_ORDR_DATE,
    E_SERV_TYPE,
    E_PROMOTION,
    E_A2_CH_FMC,
    E_PRTTY_NUM,
    E_LUCKY_NUM,
    E_CQS_USAGE,
    E_MTC_USAGE,
    E_TOTAL_REJ
} E_REJECT_TYPE;

#define NOT_FOUND               1403
#define FETCH_NULL              1405
#define KEY_DUP                 -1
#define DEADLOCK                -60
#define FETCH_OUTOFSEQ          -1002

#define SIZE_GEN_STR            100
#define SIZE_SQL_STR            1024

#define PERCENT_SIGN            37      // '%'

#define ALRT2_RES               "_RES"
#define ALRT2_SME               "_SME"

#define OVECCOUNT               30
#define MAX_PAT_ITEM            1024
#define PAT_LEN                 250
#define COD_LEN                 20

typedef struct pat {
    char  pattern[PAT_LEN];
    char  patcode[COD_LEN];
    pcre *pcrepat;
} PATTERN;

// SHEET1_HEADER is from xml template file
#define SHEET1_TRAILER          "</Table> </Worksheet>"

#define SHEET2_HEADER           "<Worksheet ss:Name=\"DETAIL2\"> <Table ss:ExpandedColumnCount=\"5\" ss:ExpandedRowCount=\"50000\" x:FullColumns=\"1\" x:FullRows=\"1\" ss:DefaultRowHeight=\"15\"> "\
"<Row ss:AutoFitHeight=\"0\"> "\
"<Cell ss:StyleID=\"s62\"><Data ss:Type=\"String\">Mobile No</Data></Cell> "\
"<Cell ss:StyleID=\"s62\"><Data ss:Type=\"String\">Order Date Time</Data></Cell> "\
"<Cell ss:StyleID=\"s62\"><Data ss:Type=\"String\">Order Type</Data></Cell> "\
"<Cell ss:StyleID=\"s62\"><Data ss:Type=\"String\">CAT</Data></Cell> "\
"<Cell ss:StyleID=\"s62\"><Data ss:Type=\"String\">Sub CAT</Data></Cell> "\
"</Row> "
#define SHEET2_TRAILER          "</Table></Worksheet></Workbook>"

#define START_ROW_TAG           "<Row ss:AutoFitHeight=\"0\">"
#define START_COL_STR_TAG       "<Cell ss:StyleID=\"s63\"><Data ss:Type=\"String\">"
#define START_COL_DTM_TAG       "<Cell ss:StyleID=\"s64\"><Data ss:Type=\"DateTime\">"
#define START_COL_INT_TAG       "<Cell ss:StyleID=\"s63\"><Data ss:Type=\"Number\">"
#define END_COL_TAG             "</Data></Cell>"
#define END_ROW_TAG             "</Row>"

#define NO_DATA_FOUND           "<Row ss:AutoFitHeight=\"0\"><Cell ss:MergeAcross=\"6\" ss:StyleID=\"m1601721466160\"><Data ss:Type=\"String\">Data Not Found</Data></Cell></Row>"
#define NO_DATA_FOUND_CHANNEL   "<Row ss:AutoFitHeight=\"0\"><Cell ss:MergeAcross=\"9\" ss:StyleID=\"m1601721466160\"><Data ss:Type=\"String\">Data Not Found</Data></Cell></Row>"
#define NO_DATA_FOUND_SHEET2    "<Row ss:AutoFitHeight=\"0\"><Cell ss:MergeAcross=\"4\" ss:StyleID=\"m1601721466160\"><Data ss:Type=\"String\">Data Not Found</Data></Cell></Row>"
#define END_OF_REPORT           "<Row ss:AutoFitHeight=\"0\"><Cell ss:StyleID=\"s65\"><Data ss:Type=\"String\">*** End of Report ***</Data></Cell></Row>"

#define MAIL_SUBJ_ALRT2         "AWN Report No In/No Out Alert%d for bill cycle <%s> on %s"
#define MAIL_CONT_ALRT2         "<html><body>Dear All,<br/><br/>&nbsp;&nbsp;&nbsp;Report No In/No Out Alert%d for bill cycle %s on %s<br/><br/>&nbsp;&nbsp;&nbsp;1. %s = %d records<br/>&nbsp;&nbsp;&nbsp;2. %s = %d records<br/><br/>&nbsp;&nbsp;&nbsp;Please see attached file<br/><br/>FMS Administrator<br/>Tel. 8365<br/></body></html>"

#define MAIL_SUBJ               "AWN Report No In/No Out Alert2 - (%s) on %s"
#define MAIL_CONT               "<html><body>Dear All,<br/><br/>&nbsp;&nbsp;&nbsp;Report No In/No Out Alert2 - %s on %s<br/><br/>&nbsp;&nbsp;&nbsp;1. %s = %d records<br/><br/>&nbsp;&nbsp;&nbsp;Please see attached file<br/><br/>FMS Administrator<br/>Tel. 8365<br/></body></html>"

int   connAllDb(char *szSfnUsr, char *szSfnPwd, char *szSfnSvr, char *szSffUsr, char *szSffPwd, char *szSffSvr);
void  discAllDb(char *szSfnSvr, char *szSffSvr);

int   connectDbSfn(char *szDbUsr, char *szDbPwd, char *szDbSvr, int nRetryCnt, int nRetryWait);
void  disconnSfn(char *dbsvr);

int   connectDbSff(char *szDbUsr, char *szDbPwd, char *szDbSvr, int nRetryCnt, int nRetryWait);
void  disconnSff(char *dbsvr);

int   procRegSub(char run_inf[][SIZE_ITEM_L], int run_type, char map_file[][SIZE_ITEM_L]);

int   checkDealer(const char *mobNum, char *dealer_cd, char *dealer_name);
int   getDealer(const char *mobNum, char *dealer_cd, char *dealer_name);

int   checkRejBizOrd(int chk_type, const char *key, const char *map_file, char *channel_type);
int   getOrderInf(const char *mobNum, char *order_date, char *order_type);

int   checkRejSvcPromo(int chk_type, const char *mobNum);

int   checkProjFMC(const char *mobNum);

int   isLuckyNum(const char *mobNum, char *is_lucky);

int   regCompPretty(const char *pattern_file);
void  regFreePretty(int nsize);
int   isPrettyNum(const char *mobNum, char *is_pretty);

int   checkCqsUsage(const char *mobNum, char cond_inf[][SIZE_ITEM_L]);

int   calcBillDate(char *szStartBillDate, char *szEndBillDate, int iBillCycle, int iRelativeMonth);
int   _calcBillDate(char *szStartBillDate, char *szEndBillDate, int iBillCycle, int iRelativeMonth, const struct tm *pstTmCurrDate);

int   openExcelFile(const char *tmplt_file, const char *tmp_dir, const char *out_dir, const char *fname, const char *suffix, int run_type);
int   closeExcelFile(int run_type);

char  *changeDateFoExcel(const char *yyyymmddhhmmss);

void  logDbUpdateInfo(const char *repname);
void  sendMail(const char *subject, const char *content, const char *attach1, const char *attach2, const char *mail_list, const char *mail_app);


#ifdef  __cplusplus
    }
#endif

#endif
