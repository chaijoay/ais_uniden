///
///
/// FACILITY    : create unidentified subscriber report
///
/// FILE NAME   : uniden_rep.c
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
///     1.2         Jul-2021     Add reject logic for alert2 (specific dealer_id/location_id)
///
///

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <libgen.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sqlite3.h>
#include <sys/types.h>

#include "minIni.h"
#include "procsig.h"
#include "strlogutl.h"

#include "sqlite_util.h"
#include "mssql_util.h"
#include "oradb_util.h"

#include "uniden_rep.h"

#define  TMPSUF     "._tmp"

int gnRunType;
int gnRebuildNumMtcTable;
int gnPurgeMtcDb;

const char gszIniKeySec[E_NO_INI_SEC][SIZE_ITEM_T] = {
    "LOOKUP",
    "LOGGER",
    "DBINFO_IMS",
    "DBINF_AWN",
    "DBINF_SUBFNT",
    "DBINF_OSISFF",
    "COMMON_CONFIG",
    "ALERT2",
    "ALERT2_CHANNEL",
    "ALERT2_FMC",
    "ALERT4"
};

const char gszIniKeyLkup[E_NO_INI_LKUP][SIZE_ITEM_T] = {
    "SQLITE_FILE_DIR",
    "SQLITE_KEEP_DAY"
};

const char gszIniKeyLogger[E_NO_INI_LOGGER][SIZE_ITEM_T] = {
    "LOG_DIR",
    "LOG_LEVEL"
};

const char gszIniKeyImsDb[E_NO_INI_IMS][SIZE_ITEM_T] = {
    "DRIVER_NAME",
    "SERVER_IP",
    "DBS_NAME",
    "DB_USER_NAME",
    "DB_PASSWORD",
    "TABLE_PREFIX"
};

const char gszIniKeyAwnDb[E_NO_INI_AWN][SIZE_ITEM_T] = {
    "DRIVER_NAME",
    "SERVER_IP",
    "DBS_NAME",
    "DB_USER_NAME",
    "DB_PASSWORD",
    "TABLE_PREFIX"
};

const char gszIniKeySubDb[E_NO_INI_SUBFNT][SIZE_ITEM_T] = {
    "DB_USER_NAME",
    "DB_PASSWORD",
    "DBS_SID"
};

const char gszIniKeySffDb[E_NO_INI_OSISFF][SIZE_ITEM_T] = {
    "DB_USER_NAME",
    "DB_PASSWORD",
    "DBS_SID"
};

const char gszIniKeyCmnConf[E_NO_INI_CMN_CONF][SIZE_ITEM_T] = {
    "BILLING_NAME_FILE",
    "NETWORK_TYPE_FILE",
    "ORDER_TYPE_FILE",
    "SERVICE_FILE",
    "PROMOTION_FILE",
    "BIZ_TYPE_FILE",
    "BC_ALERT_DAY_FILE",
    "SEND_MAIL_APP",
    "PRETTY_PAT_FILE",
    "PROMOTION_FMC_FILE",
    "LOCATION_ID_FILE"
};

const char gszIniKeyAlert[E_NO_INI_ALRT][SIZE_ITEM_T] = {
    "SQL_ACTIVE_DAY",
    "RUN_BY_BILL_CYCLE",
    "CQS_MAX_CHARGE",
    "CQS_MAX_DURATION",
    "CQS_MAX_VOLUME",
    "MTC_SEACH_BACK_DAY",
    "MTC_MAX_DURATION",
    "RPT_PATH",
    "RPT_PREFIX",
    "RPT_SUFFIX",
    "EXCEL_TEMPLATE_FILE",
    "MAIL_LIST",
    "TMP_DIR"
};

char gszIniValLkup[E_NO_INI_LKUP][SIZE_ITEM_L];
char gszIniValLogger[E_NO_INI_LOGGER][SIZE_ITEM_L];
char gszIniValImsDb[E_NO_INI_IMS][SIZE_ITEM_L];
char gszIniValAwnDb[E_NO_INI_AWN][SIZE_ITEM_L];
char gszIniValSubDb[E_NO_INI_SUBFNT][SIZE_ITEM_L];
char gszIniValSffDb[E_NO_INI_OSISFF][SIZE_ITEM_L];

char gszIniValCmnConf[E_NO_INI_CMN_CONF][SIZE_ITEM_L];
char gszIniValAlrt2[E_NO_INI_ALRT][SIZE_ITEM_L];
char gszIniValAlrt2Ch[E_NO_INI_ALRT][SIZE_ITEM_L];
char gszIniValAlrt2Fmc[E_NO_INI_ALRT][SIZE_ITEM_L];
char gszIniValAlrt4[E_NO_INI_ALRT][SIZE_ITEM_L];

char gszIniFile[SIZE_FULL_NAME];
char gszToday[SIZE_DATE_ONLY+1];
char gszOutFname[SIZE_ITEM_L];
char gszAppName[SIZE_ITEM_S];


int main(int argc, char *argv[])
{

    memset(gszIniFile, 0x00, sizeof(gszIniFile));
    memset(gszToday, 0x00, sizeof(gszToday));
    time_t start, stop;

    char (*alrt_info)[SIZE_ITEM_L];

    //createMapTable("PROMOTION", "/app/hpe/devsrc/uniden/rej_promotion.dat");
    //printf("lookupMapTable '8a7bb6023b61e6cd013ba8aa0e2819be', (%d)\n", lookupMapTable("PROMOTION", "8a7bb6023b61e6cd013ba8aa0e2819be"));
    //return 0;

    // 1. read ini file
    if ( readConfig(argc, argv) != SUCCESS ) {
        return EXIT_FAILURE;
    }

    if ( procLock(_APP_NAME_, E_CHK) != SUCCESS ) {
        fprintf(stderr, "another instance of %s is running\n", _APP_NAME_);
        return EXIT_FAILURE;
    }

    if ( handleSignal() != SUCCESS ) {
        fprintf(stderr, "init handle signal failed: %s\n", getSigInfoStr());
        return EXIT_FAILURE;
    }

    if ( startLogging(gszIniValLogger[E_C_LOG_DIR], gszAppName, atoi(gszIniValLogger[E_C_LOG_LVL])) != SUCCESS ) {
       return EXIT_FAILURE;
    }

    if ( validateIni() == FAILED ) {
        return EXIT_FAILURE;
    }

    logHeader();


    char inp_file[SIZE_ITEM_L]; memset(inp_file, 0x00, sizeof(inp_file));
    char inp_type[10];          memset(inp_type, 0x00, sizeof(inp_type));
    long cont_pos = 0L;

    strcpy(gszToday, getSysDTM(DTM_DATE_ONLY));

    // Main processing loop
    // ...................
    procLock(_APP_NAME_, E_SET);
    setSqlDbPath(gszIniValLkup[E_L_SQ_FILE_DIR]);

    // purge mtc usage table mode
    if ( gnPurgeMtcDb == 1 ) {
        char rmcmd[SIZE_BUFF];
        memset(rmcmd, 0x00, sizeof(rmcmd));

        start = time(NULL);
        purgeOldDataMtc(atoi(gszIniValLkup[E_L_SQ_KEEP_DAY]));
        stop = time(NULL);
        writeLog(LOG_INF, "purge old data: time taken %s", sec2HMS((long)(stop-start)));

        // purge old report file
        sprintf(rmcmd, "find %s -name '%s*' -mtime +%s -delete", gszIniValAlrt2[E_RPT_PATH], gszIniValAlrt2[E_RPT_PREFIX], gszIniValLkup[E_L_SQ_KEEP_DAY]);
        writeLog(LOG_DBG, "purge old report, %s", rmcmd);
        system(rmcmd);
        sprintf(rmcmd, "find %s -name '%s*' -mtime +%s -delete", gszIniValAlrt2Ch[E_RPT_PATH], gszIniValAlrt2Ch[E_RPT_PREFIX], gszIniValLkup[E_L_SQ_KEEP_DAY]);
        writeLog(LOG_DBG, "purge old report, %s", rmcmd);
        system(rmcmd);
        sprintf(rmcmd, "find %s -name '%s*' -mtime +%s -delete", gszIniValAlrt2Fmc[E_RPT_PATH], gszIniValAlrt2Fmc[E_RPT_PREFIX], gszIniValLkup[E_L_SQ_KEEP_DAY]);
        writeLog(LOG_DBG, "purge old report, %s", rmcmd);
        system(rmcmd);
        sprintf(rmcmd, "find %s -name '%s*' -mtime +%s -delete", gszIniValAlrt4[E_RPT_PATH], gszIniValAlrt4[E_RPT_PREFIX], gszIniValLkup[E_L_SQ_KEEP_DAY]);
        writeLog(LOG_DBG, "purge old report, %s", rmcmd);
        system(rmcmd);

        goto _COMPLETE_;
        //return EXIT_SUCCESS;
    }

    // rebuild db mode
    // -2 rebuild none, -1 rebuild all, 0 ... n rebuild according given number
    if ( gnRebuildNumMtcTable > -2 ) {

        start = time(NULL);
        setDbInfo(gszIniValImsDb, E_NO_INI_IMS, gszIniValAwnDb, E_NO_INI_AWN);
        getTableList();

        // rebuild all mtc usage table mode
        if ( gnRebuildNumMtcTable == -1 ) {
            buildMtcUsageTab(NULL);
        }
        else {
            char date_to_build[SIZE_DATE_ONLY+1];
            int i;
            // gnRebuildNumMtcTable+1 => plus one because its today inclusive
            for ( i=0; i<gnRebuildNumMtcTable+1; i++ ) {
                memset(date_to_build, 0x00, sizeof(date_to_build));
                strcpy(date_to_build, dateAdd(gszToday, DTM_DATE_ONLY, 0, 0, (-1)*i));
                buildMtcUsageTab(date_to_build);
            }
        }
        stop = time(NULL);
        writeLog(LOG_INF, "mtc usage build: time taken %s", sec2HMS((long)(stop-start)));
        goto _COMPLETE_;

    }

    if ( gnRunType == E_ALERT2 ) {
        writeLog(LOG_INF, "running in Alert2 mode");
        alrt_info = gszIniValAlrt2;
    }
    else if ( gnRunType == E_ALERT2_CHL ) {
        writeLog(LOG_INF, "running in Alert2 Channel mode");
        alrt_info = gszIniValAlrt2Ch;
    }
    else if ( gnRunType == E_ALERT2_FMC ) {
        writeLog(LOG_INF, "running in Alert2 FMC mode");
        alrt_info = gszIniValAlrt2Fmc;
    }
    else if ( gnRunType == E_ALERT4 ) {
        writeLog(LOG_INF, "running in Alert4 mode");
        alrt_info = gszIniValAlrt4;
    }
    else {
        goto _COMPLETE_;
    }

    int i=0;
    for ( i=0; i<E_NO_INI_CMN_CONF; i++ ) {
        if ( i == E_F_MAIL_APP || i == E_F_PRETTY_FILE ) {
            // email file and pretty pattern have another processing
            continue;
        }
        createMapTable(gszIniKeyCmnConf[i], gszIniValCmnConf[i]);
    }

    int num_item = regCompPretty(gszIniValCmnConf[E_F_PRETTY_FILE]);
    if ( num_item == FAILED ) {
        goto _COMPLETE_;
    }

    // connect to two oracle database this is required for getting registered subscribers' information
    if ( connAllDb(gszIniValSubDb[E_SB_USR], gszIniValSubDb[E_SB_PWD], gszIniValSubDb[E_SB_SID],
                   gszIniValSffDb[E_SF_USR], gszIniValSffDb[E_SF_PWD], gszIniValSffDb[E_SF_SID]) != SUCCESS ) {
        goto _COMPLETE_;
    }

    start = time(NULL);

    // main processing function ...
    procRegSub(alrt_info, gnRunType, gszIniValCmnConf);
    regFreePretty(num_item);
    sqlite_cleanup();
    stop = time(NULL);
    writeLog(LOG_INF, "main process: time taken %s", sec2HMS((long)(stop-start)));


    // disconnect db
    discAllDb(gszIniValSubDb[E_SB_SID], gszIniValSffDb[E_SF_SID]);

_COMPLETE_:

    // clean up
    procLock(_APP_NAME_, E_CLR);
    writeLog(LOG_INF, "%s", getSigInfoStr());
    writeLog(LOG_INF, "------- %s process completely stop -------", _APP_NAME_);
    stopLogging();

    return EXIT_SUCCESS;

}


int readConfig(int argc, char *argv[])
{
    int key, i, tmp_day_to_keep = -1;
    char gAppPath[100];

    gnRunType = -1;
    gnPurgeMtcDb = -1;
    gnRebuildNumMtcTable = -2;  // -2 rebuild none, -1 rebuild all, 0 ... n rebuild according given number

    memset(gszAppName, 0x00, sizeof(gszAppName));
    memset(gszIniFile, 0x00, sizeof(gszIniFile));

    memset(gszIniValLkup, 0x00, sizeof(gszIniValLkup));
    memset(gszIniValLogger, 0x00, sizeof(gszIniValLogger));
    memset(gszIniValImsDb, 0x00, sizeof(gszIniValImsDb));
    memset(gszIniValAwnDb, 0x00, sizeof(gszIniValAwnDb));
    memset(gszIniValSubDb, 0x00, sizeof(gszIniValSubDb));
    memset(gszIniValSffDb, 0x00, sizeof(gszIniValSffDb));
    memset(gszIniValCmnConf, 0x00, sizeof(gszIniValCmnConf));
    memset(gszIniValAlrt2, 0x00, sizeof(gszIniValAlrt2));
    memset(gszIniValAlrt2Ch, 0x00, sizeof(gszIniValAlrt2Ch));
    memset(gszIniValAlrt2Fmc, 0x00, sizeof(gszIniValAlrt2Fmc));
    memset(gszIniValAlrt4, 0x00, sizeof(gszIniValAlrt4));

    strcpy(gAppPath, argv[0]);
    char *p = strrchr(gAppPath, '/');
    *p = '\0';

    if ( argc <= 1 ) {
        printUsage();
        return FAILED;
    }

    for ( i = 1; i < argc; i++ ) {
        if ( strcmp(argv[i], "-n") == 0 ) {     // specified ini file
            strcpy(gszIniFile, argv[++i]);
        }
        else if ( strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0 ) {
            printUsage();
            return FAILED;
        }
        else if ( strcmp(argv[i], "-mkini") == 0 ) {
            makeIni();
            return FAILED;
        }
        else if ( strcmp(argv[i], "-a") == 0 ) {
            if ( i+1 >= argc ) {
                printUsage();
                return FAILED;
            }
            if ( strcmp(argv[i+1], "alert2") == 0 ) {
                gnRunType = E_ALERT2;
                sprintf(gszAppName, "%s_alert2", _APP_NAME_);
            }
            else if ( strcmp(argv[i+1], "channel") == 0 ) {
                gnRunType = E_ALERT2_CHL;
                sprintf(gszAppName, "%s_channel", _APP_NAME_);
            }
            else if ( strcmp(argv[i+1], "fmc") == 0 ) {
                gnRunType = E_ALERT2_FMC;
                sprintf(gszAppName, "%s_fmc", _APP_NAME_);
            }
            else if ( strcmp(argv[i+1], "alert4") == 0 ) {
                gnRunType = E_ALERT4;
                sprintf(gszAppName, "%s_alert4", _APP_NAME_);
            }
            else {
                printUsage();
                return FAILED;
            }
            i++;
        }
        else if ( strcmp(argv[i], "-purge") == 0 ) {
            gnPurgeMtcDb = 1;
            sprintf(gszAppName, "%s_purge", _APP_NAME_);
            if ( i+1 >= argc ) {
                tmp_day_to_keep = -1;
            }
            else {
                if ( atoi(argv[i+1]) > 0 ) {
                    tmp_day_to_keep = atoi(argv[i+1]);
                }
                i++;
            }
        }
        else if ( strcmp(argv[i], "-db") == 0 ) {
            if ( i+1 >= argc ) {
                printUsage();
                return FAILED;
            }
            sprintf(gszAppName, "%s_db", _APP_NAME_);
            if ( strcmp(argv[i+1], "all") == 0 ) {
                gnRebuildNumMtcTable = -1;
            }
            else if ( atoi(argv[i+1]) >= 0 ) {
                gnRebuildNumMtcTable = atoi(argv[i+1]);
            }
            i++;
        }
        else {
            fprintf(stderr, "unknown option: %s", argv[i]);
            printUsage();
            return FAILED;
        }
    }

    if ( gszIniFile[0] == '\0' ) {
        sprintf(gszIniFile, "%s/%s.ini", gAppPath, _APP_NAME_);
    }

    if ( access(gszIniFile, F_OK|R_OK) != SUCCESS ) {
        sprintf(gszIniFile, "%s/%s.ini", gAppPath, _APP_NAME_);
        if ( access(gszIniFile, F_OK|R_OK) != SUCCESS ) {
            fprintf(stderr, "unable to access ini file %s (%s)\n", gszIniFile, strerror(errno));
            return FAILED;
        }
    }

    // Read config of MAPPING Section
    for ( key = 0; key < E_NO_INI_LKUP; key++ ) {
        ini_gets(gszIniKeySec[E_LOOKUP], gszIniKeyLkup[key], "NA", gszIniValLkup[key], sizeof(gszIniValLkup[key]), gszIniFile);
    }
    if ( tmp_day_to_keep > 0 ) {
        sprintf(gszIniValLkup[E_L_SQ_KEEP_DAY], "%d", tmp_day_to_keep);
    }

    // Read config of COMMON Section
    for ( key = 0; key < E_NO_INI_LOGGER; key++ ) {
        ini_gets(gszIniKeySec[E_LOGGER], gszIniKeyLogger[key], "NA", gszIniValLogger[key], sizeof(gszIniValLogger[key]), gszIniFile);
    }

    // Read config of DB Connection Section
    for ( key = 0; key < E_NO_INI_IMS; key++ ) {
        ini_gets(gszIniKeySec[E_DBINF_IMS], gszIniKeyImsDb[key], "NA", gszIniValImsDb[key], sizeof(gszIniValImsDb[key]), gszIniFile);
    }

    for ( key = 0; key < E_NO_INI_AWN; key++ ) {
        ini_gets(gszIniKeySec[E_DBINF_AWN], gszIniKeyAwnDb[key], "NA", gszIniValAwnDb[key], sizeof(gszIniValAwnDb[key]), gszIniFile);
    }

    for ( key = 0; key < E_NO_INI_SUBFNT; key++ ) {
        ini_gets(gszIniKeySec[E_DBINF_SUBFNT], gszIniKeySubDb[key], "NA", gszIniValSubDb[key], sizeof(gszIniValSubDb[key]), gszIniFile);
    }

    for ( key = 0; key < E_NO_INI_OSISFF; key++ ) {
        ini_gets(gszIniKeySec[E_DBINF_OSISFF], gszIniKeySffDb[key], "NA", gszIniValSffDb[key], sizeof(gszIniValSffDb[key]), gszIniFile);
    }

    for ( key = 0; key < E_NO_INI_CMN_CONF; key++ ) {
        ini_gets(gszIniKeySec[E_COMMON_CONFIG], gszIniKeyCmnConf[key], "NA", gszIniValCmnConf[key], sizeof(gszIniValCmnConf[key]), gszIniFile);
    }

    for ( key = 0; key < E_NO_INI_ALRT; key++ ) {
        ini_gets(gszIniKeySec[E_ALERT2], gszIniKeyAlert[key], "NA", gszIniValAlrt2[key], sizeof(gszIniValAlrt2[key]), gszIniFile);
    }

    for ( key = 0; key < E_NO_INI_ALRT; key++ ) {
        ini_gets(gszIniKeySec[E_ALERT2_CHL], gszIniKeyAlert[key], "NA", gszIniValAlrt2Ch[key], sizeof(gszIniValAlrt2Ch[key]), gszIniFile);
    }

    for ( key = 0; key < E_NO_INI_ALRT; key++ ) {
        ini_gets(gszIniKeySec[E_ALERT2_FMC], gszIniKeyAlert[key], "NA", gszIniValAlrt2Fmc[key], sizeof(gszIniValAlrt2Fmc[key]), gszIniFile);
    }

    for ( key = 0; key < E_NO_INI_ALRT; key++ ) {
        ini_gets(gszIniKeySec[E_ALERT4], gszIniKeyAlert[key], "NA", gszIniValAlrt4[key], sizeof(gszIniValAlrt4[key]), gszIniFile);
    }

    return SUCCESS;

}

void logHeader()
{
    writeLog(LOG_INF, "---- Start %s (v%s) with following parameters ----", _APP_NAME_, _APP_VERS_);
    // print out all ini file
    ini_browse(_ini_callback, NULL, gszIniFile);
}

int _ini_callback(const char *section, const char *key, const char *value, void *userdata)
{
    if ( strstr(key, "PASSWORD") ) {
        writeLog(LOG_INF, "[%s]\t%s = ********", section, key);
    }
    else {
        writeLog(LOG_INF, "[%s]\t%s = %s", section, key, value);
//printf("[%s]\t%s = %s", section, key, value);
    }
    return 1;
}

void printUsage()
{

    fprintf(stderr, "\nusage: %s version %s\n", _APP_NAME_, _APP_VERS_);
    fprintf(stderr, "\tgenerate unidentified subscriber report\n\n");
    fprintf(stderr, "%s.exe [-a <alert_option>] [-n <ini_file>] [-db <all | back_day>] [-purge [day_to_keep]] [-mkini]\n", _APP_NAME_);
    fprintf(stderr, "   alert_option\tthere are 3 possible options\n");
    fprintf(stderr, "      alert2\n");
    fprintf(stderr, "      channel\n");
    fprintf(stderr, "      fmc\n");
    fprintf(stderr, "      alert4\n\n");
    fprintf(stderr, "   ini_file    to specify ini file other than default ini\n\n");
    fprintf(stderr, "   -db         to rebuild mtc db then exit\n");
    fprintf(stderr, "      back_day - supply number of day to rebuild mtc data\n");
    fprintf(stderr, "      all      - to rebuild all available mtc data\n");
    fprintf(stderr, "                 the 'all' option must be used with CAUTION since all\n");
    fprintf(stderr, "                 db's tables will be droped and recreation time may take up to days\n\n");
    fprintf(stderr, "   -purge      to purge old mtc db (by supplying day_to_keep or left blank for\n");
    fprintf(stderr, "               using ini) then exit\n");
    fprintf(stderr, "               then exit\n\n");
    fprintf(stderr, "   -mkini      to create ini template\n");
    fprintf(stderr, "\n");

}
#if 0
int validateIni()
{
    int i, result = SUCCESS;

    // Logger parameter validation and set default if any ...
    if ( access(gszIniValLogger[E_C_LOG_DIR], F_OK|R_OK) != SUCCESS ) {
        fprintf(stderr, "[%s] %s: unable to access %s (%s), using running dir\n", gszIniKeySec[E_LOGGER], gszIniKeyLogger[E_C_LOG_DIR], gszIniValLogger[E_C_LOG_DIR], strerror(errno));
        writeLog(LOG_WRN, "[%s] %s: unable to access %s (%s), using running dir", gszIniKeySec[E_LOGGER], gszIniKeyLogger[E_C_LOG_DIR], gszIniValLogger[E_C_LOG_DIR], strerror(errno));
    }

    if ( atoi(gszIniValLogger[E_C_LOG_LVL]) < 0 ) {
        fprintf(stderr, "[%s] %s: invalid log level of %s, set default to %d (0-7)\n", gszIniKeySec[E_LOGGER], gszIniKeyLogger[E_C_LOG_LVL], gszIniValLogger[E_C_LOG_LVL], LOG_INF);
        writeLog(LOG_WRN, "[%s] %s: invalid log level of %s, set default to %d (0-7)", gszIniKeySec[E_LOGGER], gszIniKeyLogger[E_C_LOG_LVL], gszIniValLogger[E_C_LOG_LVL], LOG_INF);
        sprintf(gszIniValLogger[E_C_LOG_LVL], "%d", LOG_INF);
    }

    // Common parameter validation ...
    for ( i=0; i<E_NO_INI_CMN_CONF; i++ ) {
        if ( access(gszIniValCmnConf[i], F_OK|R_OK) != SUCCESS ) {
            fprintf(stderr, "[%s] %s: unable to access %s (%s)\n", gszIniKeySec[E_COMMON_CONFIG], gszIniKeyCmnConf[i], gszIniValCmnConf[i], strerror(errno));
            writeLog(LOG_ERR, "[%s] %s: unable to access %s (%s)", gszIniKeySec[E_COMMON_CONFIG], gszIniKeyCmnConf[i], gszIniValCmnConf[i], strerror(errno));
            result = FAILED;
        }
    }

    if ( access(gszIniValLkup[E_L_SQ_FILE_DIR], F_OK|R_OK) != SUCCESS ) {
        fprintf(stderr, "[%s] %s: unable to access %s (%s)\n", gszIniKeySec[E_LOOKUP], gszIniKeyLkup[E_L_SQ_FILE_DIR], gszIniValLkup[E_L_SQ_FILE_DIR], strerror(errno));
        writeLog(LOG_ERR, "[%s] %s: unable to access %s (%s)", gszIniKeySec[E_LOOKUP], gszIniKeyLkup[E_L_SQ_FILE_DIR], gszIniValLkup[E_L_SQ_FILE_DIR], strerror(errno));
        result = FAILED;
    }

    // MTC usage searching back day parameter validation ...
    int max_day;
    int a_sch_day = atoi(gszIniValAlrt2[E_MTC_SCH_DAY]);
    int c_sch_day = atoi(gszIniValAlrt2Ch[E_MTC_SCH_DAY]);
    int f_sch_day = atoi(gszIniValAlrt2Fmc[E_MTC_SCH_DAY]);
    int a4_sch_day = atoi(gszIniValAlrt4[E_MTC_SCH_DAY]);

    if ( a_sch_day <= 0 ) {
        fprintf(stderr, "[%s] %s: (%s) must be greater than 0\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[E_MTC_SCH_DAY], gszIniValAlrt2[E_MTC_SCH_DAY]);
        writeLog(LOG_ERR, "[%s] %s: (%s) must be greater than 0", gszIniKeySec[E_ALERT2], gszIniKeyAlert[E_MTC_SCH_DAY], gszIniValAlrt2[E_MTC_SCH_DAY]);
        result = FAILED;
    }
    if ( c_sch_day <= 0 ) {
        fprintf(stderr, "[%s] %s: (%s)  must be greater than 0\n", gszIniKeySec[E_ALERT2_CHL], gszIniKeyAlert[E_MTC_SCH_DAY], gszIniValAlrt2Ch[E_MTC_SCH_DAY]);
        writeLog(LOG_ERR, "[%s] %s: (%s)  must be greater than 0", gszIniKeySec[E_ALERT2_CHL], gszIniKeyAlert[E_MTC_SCH_DAY], gszIniValAlrt2Ch[E_MTC_SCH_DAY]);
        result = FAILED;
    }
    if ( f_sch_day <= 0 ) {
        fprintf(stderr, "[%s] %s: (%s)  must be greater than 0\n", gszIniKeySec[E_ALERT2_FMC], gszIniKeyAlert[E_MTC_SCH_DAY], gszIniValAlrt2Fmc[E_MTC_SCH_DAY]);
        writeLog(LOG_ERR, "[%s] %s: (%s)  must be greater than 0", gszIniKeySec[E_ALERT2_FMC], gszIniKeyAlert[E_MTC_SCH_DAY], gszIniValAlrt2Fmc[E_MTC_SCH_DAY]);
        result = FAILED;
    }
    if ( a4_sch_day <= 0 ) {
        fprintf(stderr, "[%s] %s: (%s) must be greater than 0\n", gszIniKeySec[E_ALERT4], gszIniKeyAlert[E_MTC_SCH_DAY], gszIniValAlrt4[E_MTC_SCH_DAY]);
        writeLog(LOG_ERR, "[%s] %s: (%s) must be greater than 0", gszIniKeySec[E_ALERT4], gszIniKeyAlert[E_MTC_SCH_DAY], gszIniValAlrt4[E_MTC_SCH_DAY]);
        result = FAILED;
    }

    // MTC usage searching back day parameter validation against available data after purge day ...
    max_day = a_sch_day;
    if ( max_day < c_sch_day ) {
        max_day = c_sch_day;
    }
    if ( max_day < f_sch_day ) {
        max_day = f_sch_day;
    }
    if ( max_day < a4_sch_day ) {
        max_day = a4_sch_day;
    }
    int sq_keep_day = atoi(gszIniValLkup[E_L_SQ_KEEP_DAY]);
    if ( sq_keep_day <= 0 ) {
        fprintf(stderr, "[%s] %s: (%s) must be greater than 0\n", gszIniKeySec[E_LOOKUP], gszIniKeyLkup[E_L_SQ_KEEP_DAY], gszIniValLkup[E_L_SQ_KEEP_DAY]);
        writeLog(LOG_ERR, "[%s] %s: (%s) must be greater than 0", gszIniKeySec[E_LOOKUP], gszIniKeyLkup[E_L_SQ_KEEP_DAY], gszIniValLkup[E_L_SQ_KEEP_DAY]);
        result = FAILED;
    }
    if ( sq_keep_day < max_day ) {
        fprintf(stderr, "[%s] %s: (%s) must be greater than %s: (%d)\n", gszIniKeySec[E_LOOKUP], gszIniKeyLkup[E_L_SQ_KEEP_DAY], gszIniValLkup[E_L_SQ_KEEP_DAY], gszIniKeyAlert[E_MTC_SCH_DAY], max_day);
        writeLog(LOG_ERR, "[%s] %s: (%s) must be greater than %s: (%d)", gszIniKeySec[E_LOOKUP], gszIniKeyLkup[E_L_SQ_KEEP_DAY], gszIniValLkup[E_L_SQ_KEEP_DAY], gszIniKeyAlert[E_MTC_SCH_DAY], max_day);
        result = FAILED;
    }

    switch ( *gszIniValAlrt2[E_RUN_BC] ) {
        case 'Y':
        case 'y':
            *gszIniValAlrt2[E_RUN_BC] = 'Y';
            break;
        case 'N':
        case 'n':
            *gszIniValAlrt2[E_RUN_BC] = 'N';
            break;
        default:
            fprintf(stderr, "[%s] %s: (%s) set default to N (Y,N)\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[E_RUN_BC], gszIniValAlrt2[E_RUN_BC]);
            writeLog(LOG_WRN, "[%s] %s: (%s) set default to N (Y,N)", gszIniKeySec[E_ALERT2], gszIniKeyAlert[E_RUN_BC], gszIniValAlrt2[E_RUN_BC]);
            break;
    }

    switch ( *gszIniValAlrt2Ch[E_RUN_BC] ) {
        case 'Y':
        case 'y':
            *gszIniValAlrt2Ch[E_RUN_BC] = 'Y';
            break;
        case 'N':
        case 'n':
            *gszIniValAlrt2Ch[E_RUN_BC] = 'N';
            break;
        default:
            fprintf(stderr, "[%s] %s: (%s) set default to N (Y,N)\n", gszIniKeySec[E_ALERT2_CHL], gszIniKeyAlert[E_RUN_BC], gszIniValAlrt2Ch[E_RUN_BC]);
            writeLog(LOG_WRN, "[%s] %s: (%s) set default to N (Y,N)", gszIniKeySec[E_ALERT2_CHL], gszIniKeyAlert[E_RUN_BC], gszIniValAlrt2Ch[E_RUN_BC]);
            break;
    }

    switch ( *gszIniValAlrt2Fmc[E_RUN_BC] ) {
        case 'Y':
        case 'y':
            *gszIniValAlrt2Fmc[E_RUN_BC] = 'Y';
            break;
        case 'N':
        case 'n':
            *gszIniValAlrt2Fmc[E_RUN_BC] = 'N';
            break;
        default:
            fprintf(stderr, "[%s] %s: (%s) set default to N (Y,N)\n", gszIniKeySec[E_ALERT2_FMC], gszIniKeyAlert[E_RUN_BC], gszIniValAlrt2Fmc[E_RUN_BC]);
            writeLog(LOG_WRN, "[%s] %s: (%s) set default to N (Y,N)", gszIniKeySec[E_ALERT2_FMC], gszIniKeyAlert[E_RUN_BC], gszIniValAlrt2Fmc[E_RUN_BC]);
            break;
    }

    switch ( *gszIniValAlrt4[E_RUN_BC] ) {
        case 'Y':
        case 'y':
            *gszIniValAlrt4[E_RUN_BC] = 'Y';
            break;
        case 'N':
        case 'n':
            *gszIniValAlrt4[E_RUN_BC] = 'N';
            break;
        default:
            fprintf(stderr, "[%s] %s: (%s) set default to N (Y,N)\n", gszIniKeySec[E_ALERT4], gszIniKeyAlert[E_RUN_BC], gszIniValAlrt4[E_RUN_BC]);
            writeLog(LOG_WRN, "[%s] %s: (%s) set default to N (Y,N)", gszIniKeySec[E_ALERT4], gszIniKeyAlert[E_RUN_BC], gszIniValAlrt4[E_RUN_BC]);
            break;
    }

    for ( i=0; i<E_NO_INI_CMN_CONF; i++ ) {
        switch ( i ) {
            case E_RPT_PATH:
            case E_XCL_TMPL_FILE:
            case E_MAIL_LIST:
            case E_TMP_DIR:
                if ( access(gszIniValAlrt2[i], F_OK|R_OK) != SUCCESS ) {
                    fprintf(stderr, "[%s] %s: unable to access %s (%s)\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2[i], strerror(errno));
                    writeLog(LOG_ERR, "[%s] %s: unable to access %s (%s)", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2[i], strerror(errno));
                    result = FAILED;
                }
                if ( access(gszIniValAlrt2Ch[i], F_OK|R_OK) != SUCCESS ) {
                    fprintf(stderr, "[%s] %s: unable to access %s (%s)\n", gszIniKeySec[E_ALERT2_CHL], gszIniKeyAlert[i], gszIniValAlrt2Ch[i], strerror(errno));
                    writeLog(LOG_ERR, "[%s] %s: unable to access %s (%s)", gszIniKeySec[E_ALERT2_CHL], gszIniKeyAlert[i], gszIniValAlrt2Ch[i], strerror(errno));
                    result = FAILED;
                }
                if ( access(gszIniValAlrt2Fmc[i], F_OK|R_OK) != SUCCESS ) {
                    fprintf(stderr, "[%s] %s: unable to access %s (%s)\n", gszIniKeySec[E_ALERT2_FMC], gszIniKeyAlert[i], gszIniValAlrt2Fmc[i], strerror(errno));
                    writeLog(LOG_ERR, "[%s] %s: unable to access %s (%s)", gszIniKeySec[E_ALERT2_FMC], gszIniKeyAlert[i], gszIniValAlrt2Fmc[i], strerror(errno));
                    result = FAILED;
                }
                if ( access(gszIniValAlrt4[i], F_OK|R_OK) != SUCCESS ) {
                    fprintf(stderr, "[%s] %s: unable to access %s (%s)\n", gszIniKeySec[E_ALERT4], gszIniKeyAlert[i], gszIniValAlrt4[i], strerror(errno));
                    writeLog(LOG_ERR, "[%s] %s: unable to access %s (%s)", gszIniKeySec[E_ALERT4], gszIniKeyAlert[i], gszIniValAlrt4[i], strerror(errno));
                    result = FAILED;
                }
                break;
            default:
                break;
        }
    }

    char param_str[50], *item[3];
    for ( i=0; i<E_NO_INI_CMN_CONF; i++ ) {
        switch ( i ) {
            case E_CQS_MAX_CHG:
            case E_CQS_MAX_DUR:
            case E_CQS_MAX_VOL:
            case E_MTC_MAX_DUR:
                memset(param_str, 0x00, sizeof(param_str));
                strcpy(param_str, gszIniValAlrt2[i]);
                getTokenAll(item, 5, param_str, ',');
                if ( *item[1] == '\0' ) {
                    if ( atol(item[0]) < 0 ) {
                        fprintf(stderr, "[%s] %s: (%s) must be >= 0\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2[i]);
                        writeLog(LOG_ERR, "[%s] %s: (%s) must be >= 0", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2[i]);
                        result = FAILED;
                    }
                }
                else {
                    if ( atol(item[0]) < 0 ) {
                        fprintf(stderr, "[%s] %s: %s (%s) must be >= 0\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2[i], item[0]);
                        writeLog(LOG_ERR, "[%s] %s: %s (%s) must be >= 0", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2[i], item[0]);
                        result = FAILED;
                    }
                    if ( atol(item[1]) < 0 ) {
                        fprintf(stderr, "[%s] %s: %s (%s) must be >= 0\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2[i], item[1]);
                        writeLog(LOG_ERR, "[%s] %s: %s (%s) must be >= 0", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2[i], item[1]);
                        result = FAILED;
                    }
                    if ( atol(item[0]) >= atol(item[1]) ) {
                        fprintf(stderr, "[%s] %s: (%s) 1st term must be less than 2nd term\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2[i]);
                        writeLog(LOG_ERR, "[%s] %s: (%s) 1st term must be less than 2nd term", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2[i]);
                        result = FAILED;
                    }
                }

                memset(param_str, 0x00, sizeof(param_str));
                strcpy(param_str, gszIniValAlrt2Ch[i]);
                getTokenAll(item, 5, param_str, ',');
                if ( *item[1] == '\0' ) {
                    if ( atol(item[0]) < 0 ) {
                        fprintf(stderr, "[%s] %s: (%s) must be >= 0\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Ch[i]);
                        writeLog(LOG_ERR, "[%s] %s: (%s) must be >= 0", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Ch[i]);
                        result = FAILED;
                    }
                }
                else {
                    if ( atol(item[0]) < 0 ) {
                        fprintf(stderr, "[%s] %s: %s (%s) must be >= 0\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Ch[i], item[0]);
                        writeLog(LOG_ERR, "[%s] %s: %s (%s) must be >= 0", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Ch[i], item[0]);
                        result = FAILED;
                    }
                    if ( atol(item[1]) < 0 ) {
                        fprintf(stderr, "[%s] %s: %s (%s) must be >= 0\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Ch[i], item[1]);
                        writeLog(LOG_ERR, "[%s] %s: %s (%s) must be >= 0", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Ch[i], item[1]);
                        result = FAILED;
                    }
                    if ( atol(item[0]) >= atol(item[1]) ) {
                        fprintf(stderr, "[%s] %s: (%s) 1st term must be less than 2nd term\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Ch[i]);
                        writeLog(LOG_ERR, "[%s] %s: (%s) 1st term must be less than 2nd term", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Ch[i]);
                        result = FAILED;
                    }
                }

                memset(param_str, 0x00, sizeof(param_str));
                strcpy(param_str, gszIniValAlrt2Fmc[i]);
                getTokenAll(item, 5, param_str, ',');
                if ( *item[1] == '\0' ) {
                    if ( atol(item[0]) < 0 ) {
                        fprintf(stderr, "[%s] %s: (%s) must be >= 0\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Fmc[i]);
                        writeLog(LOG_ERR, "[%s] %s: (%s) must be >= 0", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Fmc[i]);
                        result = FAILED;
                    }
                }
                else {
                    if ( atol(item[0]) < 0 ) {
                        fprintf(stderr, "[%s] %s: %s (%s) must be >= 0\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Fmc[i], item[0]);
                        writeLog(LOG_ERR, "[%s] %s: %s (%s) must be >= 0", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Fmc[i], item[0]);
                        result = FAILED;
                    }
                    if ( atol(item[1]) < 0 ) {
                        fprintf(stderr, "[%s] %s: %s (%s) must be >= 0\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Fmc[i], item[1]);
                        writeLog(LOG_ERR, "[%s] %s: %s (%s) must be >= 0", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Fmc[i], item[1]);
                        result = FAILED;
                    }
                    if ( atol(item[0]) >= atol(item[1]) ) {
                        fprintf(stderr, "[%s] %s: (%s) 1st term must be less than 2nd term\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Fmc[i]);
                        writeLog(LOG_ERR, "[%s] %s: (%s) 1st term must be less than 2nd term", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Fmc[i]);
                        result = FAILED;
                    }
                }

                memset(param_str, 0x00, sizeof(param_str));
                strcpy(param_str, gszIniValAlrt4[i]);
                getTokenAll(item, 5, param_str, ',');
                if ( *item[1] == '\0' ) {
                    if ( atol(item[0]) < 0 ) {
                        fprintf(stderr, "[%s] %s: (%s) must be >= 0\n", gszIniKeySec[E_ALERT4], gszIniKeyAlert[i], gszIniValAlrt4[i]);
                        writeLog(LOG_ERR, "[%s] %s: (%s) must be >= 0", gszIniKeySec[E_ALERT4], gszIniKeyAlert[i], gszIniValAlrt4[i]);
                        result = FAILED;
                    }
                }
                else {
                    if ( atol(item[0]) < 0 ) {
                        fprintf(stderr, "[%s] %s: %s (%s) must be >= 0\n", gszIniKeySec[E_ALERT4], gszIniKeyAlert[i], gszIniValAlrt4[i], item[0]);
                        writeLog(LOG_ERR, "[%s] %s: %s (%s) must be >= 0", gszIniKeySec[E_ALERT4], gszIniKeyAlert[i], gszIniValAlrt4[i], item[0]);
                        result = FAILED;
                    }
                    if ( atol(item[1]) < 0 ) {
                        fprintf(stderr, "[%s] %s: %s (%s) must be >= 0\n", gszIniKeySec[E_ALERT4], gszIniKeyAlert[i], gszIniValAlrt4[i], item[1]);
                        writeLog(LOG_ERR, "[%s] %s: %s (%s) must be >= 0", gszIniKeySec[E_ALERT4], gszIniKeyAlert[i], gszIniValAlrt4[i], item[1]);
                        result = FAILED;
                    }
                    if ( atol(item[0]) >= atol(item[1]) ) {
                        fprintf(stderr, "[%s] %s: (%s) 1st term must be less than 2nd term\n", gszIniKeySec[E_ALERT4], gszIniKeyAlert[i], gszIniValAlrt4[i]);
                        writeLog(LOG_ERR, "[%s] %s: (%s) 1st term must be less than 2nd term", gszIniKeySec[E_ALERT4], gszIniKeyAlert[i], gszIniValAlrt4[i]);
                        result = FAILED;
                    }
                }
                break;
            default:
                break;
        }

    }

    return result;

}
#endif

int validateIni()
{
    int i, result = SUCCESS;

    // Logger parameter validation and set default if any ...
    if ( access(gszIniValLogger[E_C_LOG_DIR], F_OK|R_OK) != SUCCESS ) {
        fprintf(stderr, "[%s] %s: unable to access %s (%s), using running dir\n", gszIniKeySec[E_LOGGER], gszIniKeyLogger[E_C_LOG_DIR], gszIniValLogger[E_C_LOG_DIR], strerror(errno));
        writeLog(LOG_WRN, "[%s] %s: unable to access %s (%s), using running dir", gszIniKeySec[E_LOGGER], gszIniKeyLogger[E_C_LOG_DIR], gszIniValLogger[E_C_LOG_DIR], strerror(errno));
    }

    if ( atoi(gszIniValLogger[E_C_LOG_LVL]) < 0 ) {
        fprintf(stderr, "[%s] %s: invalid log level of %s, set default to %d (0-7)\n", gszIniKeySec[E_LOGGER], gszIniKeyLogger[E_C_LOG_LVL], gszIniValLogger[E_C_LOG_LVL], LOG_INF);
        writeLog(LOG_WRN, "[%s] %s: invalid log level of %s, set default to %d (0-7)", gszIniKeySec[E_LOGGER], gszIniKeyLogger[E_C_LOG_LVL], gszIniValLogger[E_C_LOG_LVL], LOG_INF);
        sprintf(gszIniValLogger[E_C_LOG_LVL], "%d", LOG_INF);
    }

    // Common parameter validation ...
    for ( i=0; i<E_NO_INI_CMN_CONF; i++ ) {
        if ( access(gszIniValCmnConf[i], F_OK|R_OK) != SUCCESS ) {
            fprintf(stderr, "[%s] %s: unable to access %s (%s)\n", gszIniKeySec[E_COMMON_CONFIG], gszIniKeyCmnConf[i], gszIniValCmnConf[i], strerror(errno));
            writeLog(LOG_ERR, "[%s] %s: unable to access %s (%s)", gszIniKeySec[E_COMMON_CONFIG], gszIniKeyCmnConf[i], gszIniValCmnConf[i], strerror(errno));
            result = FAILED;
        }
    }

    if ( access(gszIniValLkup[E_L_SQ_FILE_DIR], F_OK|R_OK) != SUCCESS ) {
        fprintf(stderr, "[%s] %s: unable to access %s (%s)\n", gszIniKeySec[E_LOOKUP], gszIniKeyLkup[E_L_SQ_FILE_DIR], gszIniValLkup[E_L_SQ_FILE_DIR], strerror(errno));
        writeLog(LOG_ERR, "[%s] %s: unable to access %s (%s)", gszIniKeySec[E_LOOKUP], gszIniKeyLkup[E_L_SQ_FILE_DIR], gszIniValLkup[E_L_SQ_FILE_DIR], strerror(errno));
        result = FAILED;
    }

    // MTC usage searching back day parameter validation ...
    int max_day;
    int a_sch_day = atoi(gszIniValAlrt2[E_MTC_SCH_DAY]);
    int c_sch_day = atoi(gszIniValAlrt2Ch[E_MTC_SCH_DAY]);
    int f_sch_day = atoi(gszIniValAlrt2Fmc[E_MTC_SCH_DAY]);
    int a4_sch_day = atoi(gszIniValAlrt4[E_MTC_SCH_DAY]);

    char *ini_sec_bc, *ini_key_bc, *ini_val_bc;
    if ( gnRunType == E_ALERT2 ) {
        if ( a_sch_day <= 0 ) {
            fprintf(stderr, "[%s] %s: (%s) must be greater than 0\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[E_MTC_SCH_DAY], gszIniValAlrt2[E_MTC_SCH_DAY]);
            writeLog(LOG_ERR, "[%s] %s: (%s) must be greater than 0", gszIniKeySec[E_ALERT2], gszIniKeyAlert[E_MTC_SCH_DAY], gszIniValAlrt2[E_MTC_SCH_DAY]);
            result = FAILED;
        }
        ini_val_bc = (char *)gszIniValAlrt2[E_RUN_BC];
        ini_sec_bc = (char *)gszIniKeySec[E_ALERT2];
        ini_key_bc = (char *)gszIniKeyAlert[E_RUN_BC];
    }
    else if ( gnRunType == E_ALERT2_CHL ) {
        if ( c_sch_day <= 0 ) {
            fprintf(stderr, "[%s] %s: (%s)  must be greater than 0\n", gszIniKeySec[E_ALERT2_CHL], gszIniKeyAlert[E_MTC_SCH_DAY], gszIniValAlrt2Ch[E_MTC_SCH_DAY]);
            writeLog(LOG_ERR, "[%s] %s: (%s)  must be greater than 0", gszIniKeySec[E_ALERT2_CHL], gszIniKeyAlert[E_MTC_SCH_DAY], gszIniValAlrt2Ch[E_MTC_SCH_DAY]);
            result = FAILED;
        }
        ini_val_bc = (char *)gszIniValAlrt2Ch[E_RUN_BC];
        ini_sec_bc = (char *)gszIniKeySec[E_ALERT2_CHL];
        ini_key_bc = (char *)gszIniKeyAlert[E_RUN_BC];
    }
    else if ( gnRunType == E_ALERT2_FMC ) {
        if ( f_sch_day <= 0 ) {
            fprintf(stderr, "[%s] %s: (%s)  must be greater than 0\n", gszIniKeySec[E_ALERT2_FMC], gszIniKeyAlert[E_MTC_SCH_DAY], gszIniValAlrt2Fmc[E_MTC_SCH_DAY]);
            writeLog(LOG_ERR, "[%s] %s: (%s)  must be greater than 0", gszIniKeySec[E_ALERT2_FMC], gszIniKeyAlert[E_MTC_SCH_DAY], gszIniValAlrt2Fmc[E_MTC_SCH_DAY]);
            result = FAILED;
        }
        ini_val_bc = (char *)gszIniValAlrt2Fmc[E_RUN_BC];
        ini_sec_bc = (char *)gszIniKeySec[E_ALERT2_FMC];
        ini_key_bc = (char *)gszIniKeyAlert[E_RUN_BC];
    }
    else if ( gnRunType == E_ALERT4 ) {
        if ( a4_sch_day <= 0 ) {
            fprintf(stderr, "[%s] %s: (%s) must be greater than 0\n", gszIniKeySec[E_ALERT4], gszIniKeyAlert[E_MTC_SCH_DAY], gszIniValAlrt4[E_MTC_SCH_DAY]);
            writeLog(LOG_ERR, "[%s] %s: (%s) must be greater than 0", gszIniKeySec[E_ALERT4], gszIniKeyAlert[E_MTC_SCH_DAY], gszIniValAlrt4[E_MTC_SCH_DAY]);
            result = FAILED;
        }
        ini_val_bc = (char *)gszIniValAlrt4[E_RUN_BC];
        ini_sec_bc = (char *)gszIniKeySec[E_ALERT4];
        ini_key_bc = (char *)gszIniKeyAlert[E_RUN_BC];
    }

    switch ( *ini_val_bc ) {
        case 'Y':
        case 'y':
            *ini_val_bc = 'Y';
            break;
        case 'N':
        case 'n':
            *ini_val_bc = 'N';
            break;
        default:
            fprintf(stderr, "[%s] %s: (%s) set default to N (Y,N)\n", ini_sec_bc, ini_key_bc, ini_val_bc);
            writeLog(LOG_WRN, "[%s] %s: (%s) set default to N (Y,N)", ini_sec_bc, ini_key_bc, ini_val_bc);
            break;
    }

    // MTC usage searching back day parameter validation against available data after purge day ...
    max_day = a_sch_day;
    if ( max_day < c_sch_day ) {
        max_day = c_sch_day;
    }
    if ( max_day < f_sch_day ) {
        max_day = f_sch_day;
    }
    if ( max_day < a4_sch_day ) {
        max_day = a4_sch_day;
    }
    int sq_keep_day = atoi(gszIniValLkup[E_L_SQ_KEEP_DAY]);
    if ( sq_keep_day <= 0 ) {
        fprintf(stderr, "[%s] %s: (%s) must be greater than 0\n", gszIniKeySec[E_LOOKUP], gszIniKeyLkup[E_L_SQ_KEEP_DAY], gszIniValLkup[E_L_SQ_KEEP_DAY]);
        writeLog(LOG_ERR, "[%s] %s: (%s) must be greater than 0", gszIniKeySec[E_LOOKUP], gszIniKeyLkup[E_L_SQ_KEEP_DAY], gszIniValLkup[E_L_SQ_KEEP_DAY]);
        result = FAILED;
    }
    if ( sq_keep_day < max_day ) {
        fprintf(stderr, "[%s] %s: (%s) must be greater than %s: (%d)\n", gszIniKeySec[E_LOOKUP], gszIniKeyLkup[E_L_SQ_KEEP_DAY], gszIniValLkup[E_L_SQ_KEEP_DAY], gszIniKeyAlert[E_MTC_SCH_DAY], max_day);
        writeLog(LOG_ERR, "[%s] %s: (%s) must be greater than %s: (%d)", gszIniKeySec[E_LOOKUP], gszIniKeyLkup[E_L_SQ_KEEP_DAY], gszIniValLkup[E_L_SQ_KEEP_DAY], gszIniKeyAlert[E_MTC_SCH_DAY], max_day);
        result = FAILED;
    }

    for ( i=0; i<E_NO_INI_CMN_CONF; i++ ) {
        switch ( i ) {
            case E_RPT_PATH:
            case E_XCL_TMPL_FILE:
            case E_MAIL_LIST:
            case E_TMP_DIR:
                if ( gnRunType == E_ALERT2 && access(gszIniValAlrt2[i], F_OK|R_OK) != SUCCESS ) {
                    fprintf(stderr, "[%s] %s: unable to access %s (%s)\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2[i], strerror(errno));
                    writeLog(LOG_ERR, "[%s] %s: unable to access %s (%s)", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2[i], strerror(errno));
                    result = FAILED;
                }
                if ( gnRunType == E_ALERT2_CHL && access(gszIniValAlrt2Ch[i], F_OK|R_OK) != SUCCESS ) {
                    fprintf(stderr, "[%s] %s: unable to access %s (%s)\n", gszIniKeySec[E_ALERT2_CHL], gszIniKeyAlert[i], gszIniValAlrt2Ch[i], strerror(errno));
                    writeLog(LOG_ERR, "[%s] %s: unable to access %s (%s)", gszIniKeySec[E_ALERT2_CHL], gszIniKeyAlert[i], gszIniValAlrt2Ch[i], strerror(errno));
                    result = FAILED;
                }
                if ( gnRunType == E_ALERT2_FMC && access(gszIniValAlrt2Fmc[i], F_OK|R_OK) != SUCCESS ) {
                    fprintf(stderr, "[%s] %s: unable to access %s (%s)\n", gszIniKeySec[E_ALERT2_FMC], gszIniKeyAlert[i], gszIniValAlrt2Fmc[i], strerror(errno));
                    writeLog(LOG_ERR, "[%s] %s: unable to access %s (%s)", gszIniKeySec[E_ALERT2_FMC], gszIniKeyAlert[i], gszIniValAlrt2Fmc[i], strerror(errno));
                    result = FAILED;
                }
                if ( gnRunType == E_ALERT4 && access(gszIniValAlrt4[i], F_OK|R_OK) != SUCCESS ) {
                    fprintf(stderr, "[%s] %s: unable to access %s (%s)\n", gszIniKeySec[E_ALERT4], gszIniKeyAlert[i], gszIniValAlrt4[i], strerror(errno));
                    writeLog(LOG_ERR, "[%s] %s: unable to access %s (%s)", gszIniKeySec[E_ALERT4], gszIniKeyAlert[i], gszIniValAlrt4[i], strerror(errno));
                    result = FAILED;
                }
                break;
            default:
                break;
        }
    }

    char param_str[50], *item[3];
    for ( i=0; i<E_NO_INI_CMN_CONF; i++ ) {
        switch ( i ) {
            case E_CQS_MAX_CHG:
            case E_CQS_MAX_DUR:
            case E_CQS_MAX_VOL:
            case E_MTC_MAX_DUR:
                memset(param_str, 0x00, sizeof(param_str));
                if ( gnRunType == E_ALERT2 ) {
                    strcpy(param_str, gszIniValAlrt2[i]);
                    getTokenAll(item, 5, param_str, ',');
                    if ( *item[1] == '\0' ) {
                        if ( atol(item[0]) < 0 ) {
                            fprintf(stderr, "[%s] %s: (%s) must be >= 0\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2[i]);
                            writeLog(LOG_ERR, "[%s] %s: (%s) must be >= 0", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2[i]);
                            result = FAILED;
                        }
                    }
                    else {
                        if ( atol(item[0]) < 0 ) {
                            fprintf(stderr, "[%s] %s: %s (%s) must be >= 0\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2[i], item[0]);
                            writeLog(LOG_ERR, "[%s] %s: %s (%s) must be >= 0", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2[i], item[0]);
                            result = FAILED;
                        }
                        if ( atol(item[1]) < 0 ) {
                            fprintf(stderr, "[%s] %s: %s (%s) must be >= 0\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2[i], item[1]);
                            writeLog(LOG_ERR, "[%s] %s: %s (%s) must be >= 0", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2[i], item[1]);
                            result = FAILED;
                        }
                        if ( atol(item[0]) >= atol(item[1]) ) {
                            fprintf(stderr, "[%s] %s: (%s) 1st term must be less than 2nd term\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2[i]);
                            writeLog(LOG_ERR, "[%s] %s: (%s) 1st term must be less than 2nd term", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2[i]);
                            result = FAILED;
                        }
                    }
                }
                else if ( gnRunType == E_ALERT2_CHL ) {
                    strcpy(param_str, gszIniValAlrt2Ch[i]);
                    getTokenAll(item, 5, param_str, ',');
                    if ( *item[1] == '\0' ) {
                        if ( atol(item[0]) < 0 ) {
                            fprintf(stderr, "[%s] %s: (%s) must be >= 0\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Ch[i]);
                            writeLog(LOG_ERR, "[%s] %s: (%s) must be >= 0", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Ch[i]);
                            result = FAILED;
                        }
                    }
                    else {
                        if ( atol(item[0]) < 0 ) {
                            fprintf(stderr, "[%s] %s: %s (%s) must be >= 0\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Ch[i], item[0]);
                            writeLog(LOG_ERR, "[%s] %s: %s (%s) must be >= 0", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Ch[i], item[0]);
                            result = FAILED;
                        }
                        if ( atol(item[1]) < 0 ) {
                            fprintf(stderr, "[%s] %s: %s (%s) must be >= 0\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Ch[i], item[1]);
                            writeLog(LOG_ERR, "[%s] %s: %s (%s) must be >= 0", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Ch[i], item[1]);
                            result = FAILED;
                        }
                        if ( atol(item[0]) >= atol(item[1]) ) {
                            fprintf(stderr, "[%s] %s: (%s) 1st term must be less than 2nd term\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Ch[i]);
                            writeLog(LOG_ERR, "[%s] %s: (%s) 1st term must be less than 2nd term", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Ch[i]);
                            result = FAILED;
                        }
                    }
                }
                else if ( gnRunType == E_ALERT2_FMC ) {
                    strcpy(param_str, gszIniValAlrt2Fmc[i]);
                    getTokenAll(item, 5, param_str, ',');
                    if ( *item[1] == '\0' ) {
                        if ( atol(item[0]) < 0 ) {
                            fprintf(stderr, "[%s] %s: (%s) must be >= 0\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Fmc[i]);
                            writeLog(LOG_ERR, "[%s] %s: (%s) must be >= 0", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Fmc[i]);
                            result = FAILED;
                        }
                    }
                    else {
                        if ( atol(item[0]) < 0 ) {
                            fprintf(stderr, "[%s] %s: %s (%s) must be >= 0\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Fmc[i], item[0]);
                            writeLog(LOG_ERR, "[%s] %s: %s (%s) must be >= 0", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Fmc[i], item[0]);
                            result = FAILED;
                        }
                        if ( atol(item[1]) < 0 ) {
                            fprintf(stderr, "[%s] %s: %s (%s) must be >= 0\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Fmc[i], item[1]);
                            writeLog(LOG_ERR, "[%s] %s: %s (%s) must be >= 0", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Fmc[i], item[1]);
                            result = FAILED;
                        }
                        if ( atol(item[0]) >= atol(item[1]) ) {
                            fprintf(stderr, "[%s] %s: (%s) 1st term must be less than 2nd term\n", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Fmc[i]);
                            writeLog(LOG_ERR, "[%s] %s: (%s) 1st term must be less than 2nd term", gszIniKeySec[E_ALERT2], gszIniKeyAlert[i], gszIniValAlrt2Fmc[i]);
                            result = FAILED;
                        }
                    }
                }
                else if ( gnRunType == E_ALERT4 ) {
                    strcpy(param_str, gszIniValAlrt4[i]);
                    getTokenAll(item, 5, param_str, ',');
                    if ( *item[1] == '\0' ) {
                        if ( atol(item[0]) < 0 ) {
                            fprintf(stderr, "[%s] %s: (%s) must be >= 0\n", gszIniKeySec[E_ALERT4], gszIniKeyAlert[i], gszIniValAlrt4[i]);
                            writeLog(LOG_ERR, "[%s] %s: (%s) must be >= 0", gszIniKeySec[E_ALERT4], gszIniKeyAlert[i], gszIniValAlrt4[i]);
                            result = FAILED;
                        }
                    }
                    else {
                        if ( atol(item[0]) < 0 ) {
                            fprintf(stderr, "[%s] %s: %s (%s) must be >= 0\n", gszIniKeySec[E_ALERT4], gszIniKeyAlert[i], gszIniValAlrt4[i], item[0]);
                            writeLog(LOG_ERR, "[%s] %s: %s (%s) must be >= 0", gszIniKeySec[E_ALERT4], gszIniKeyAlert[i], gszIniValAlrt4[i], item[0]);
                            result = FAILED;
                        }
                        if ( atol(item[1]) < 0 ) {
                            fprintf(stderr, "[%s] %s: %s (%s) must be >= 0\n", gszIniKeySec[E_ALERT4], gszIniKeyAlert[i], gszIniValAlrt4[i], item[1]);
                            writeLog(LOG_ERR, "[%s] %s: %s (%s) must be >= 0", gszIniKeySec[E_ALERT4], gszIniKeyAlert[i], gszIniValAlrt4[i], item[1]);
                            result = FAILED;
                        }
                        if ( atol(item[0]) >= atol(item[1]) ) {
                            fprintf(stderr, "[%s] %s: (%s) 1st term must be less than 2nd term\n", gszIniKeySec[E_ALERT4], gszIniKeyAlert[i], gszIniValAlrt4[i]);
                            writeLog(LOG_ERR, "[%s] %s: (%s) 1st term must be less than 2nd term", gszIniKeySec[E_ALERT4], gszIniKeyAlert[i], gszIniValAlrt4[i]);
                            result = FAILED;
                        }
                    }
                }
                break;
            default:
                break;
        }

    }

    return result;

}


void makeIni()
{

    int key;
    char cmd[SIZE_ITEM_S];
    char tmp_ini[SIZE_ITEM_S];
    char tmp_itm[SIZE_ITEM_S];

    sprintf(tmp_ini, "./%s_XXXXXX", _APP_NAME_);
    mkstemp(tmp_ini);
    strcpy(tmp_itm, "<place_holder>");

    // Write config of LOGGER Section
    for ( key = 0; key < E_NO_INI_LOGGER; key++ ) {
        ini_puts(gszIniKeySec[E_LOGGER], gszIniKeyLogger[key], tmp_itm, tmp_ini);
    }

    // Write config of BACKUP Section
    for ( key = 0; key < E_NO_INI_IMS; key++ ) {
        ini_puts(gszIniKeySec[E_DBINF_IMS], gszIniKeyImsDb[key], tmp_itm, tmp_ini);
    }

    for ( key = 0; key < E_NO_INI_AWN; key++ ) {
        ini_puts(gszIniKeySec[E_DBINF_AWN], gszIniKeyAwnDb[key], tmp_itm, tmp_ini);
    }

    for ( key = 0; key < E_NO_INI_SUBFNT; key++ ) {
        ini_puts(gszIniKeySec[E_DBINF_SUBFNT], gszIniKeySubDb[key], tmp_itm, tmp_ini);
    }

    for ( key = 0; key < E_NO_INI_OSISFF; key++ ) {
        ini_puts(gszIniKeySec[E_DBINF_OSISFF], gszIniKeySffDb[key], tmp_itm, tmp_ini);
    }

    for ( key = 0; key < E_NO_INI_CMN_CONF; key++ ) {
        ini_puts(gszIniKeySec[E_COMMON_CONFIG], gszIniKeyCmnConf[key], tmp_itm, tmp_ini);
    }

    for ( key = 0; key < E_NO_INI_ALRT; key++ ) {
        ini_puts(gszIniKeySec[E_ALERT2], gszIniKeyAlert[key], tmp_itm, tmp_ini);
    }

    for ( key = 0; key < E_NO_INI_ALRT; key++ ) {
        ini_puts(gszIniKeySec[E_ALERT2_CHL], gszIniKeyAlert[key], tmp_itm, tmp_ini);
    }

    for ( key = 0; key < E_NO_INI_ALRT; key++ ) {
        ini_puts(gszIniKeySec[E_ALERT2_FMC], gszIniKeyAlert[key], tmp_itm, tmp_ini);
    }

    sprintf(cmd, "mv %s %s.ini", tmp_ini, tmp_ini);
    system(cmd);
    fprintf(stderr, "ini template file is %s.ini\n", tmp_ini);

}


