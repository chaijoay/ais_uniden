///
///
/// FACILITY    : create unidentified subscriber report
///
/// FILE NAME   : uniden_rep.h
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
#ifndef __UNIDEN_REP_H__
#define __UNIDEN_REP_H__

#ifdef  __cplusplus
    extern "C" {
#endif

#define _APP_NAME_              "uniden_rep"
#define _APP_VERS_              "1.1"


// ----- INI Parameters -----
// All Section
typedef enum {
    E_LOOKUP = 0,
    E_LOGGER,
    E_DBINF_IMS,
    E_DBINF_AWN,
    E_DBINF_SUBFNT,
    E_DBINF_OSISFF,
    E_COMMON_CONFIG,
    E_ALERT2,
    E_ALERT2_CHL,
    E_ALERT2_FMC,
    E_ALERT4,
    E_NO_INI_SEC
} E_INI_SECTION;

typedef enum {
    // Lookup Table Section
    E_L_SQ_FILE_DIR = 0,
    E_L_SQ_KEEP_DAY,
    E_NO_INI_LKUP
} E_INI_MAP_SEC;

typedef enum {
    // COMMON Section
    E_C_LOG_DIR = 0,
    E_C_LOG_LVL,
    E_NO_INI_LOGGER
} E_INI_LOG_SEC;

typedef enum {
    E_IM_DRIVER = 0,
    E_IM_SERVER,
    E_IM_DBS_NAME,
    E_IM_DB_USR,
    E_IM_DB_PWD,
    E_IM_TAB_PREF,
    E_NO_INI_IMS
} E_INI_DB_IMS_SEC;

typedef enum {
    E_AW_DRIVER = 0,
    E_AW_SERVER,
    E_AW_DBS_NAME,
    E_AW_DB_USR,
    E_AW_DB_PWD,
    E_AW_TAB_PREF,
    E_NO_INI_AWN
} E_INI_DB_AWN_SEC;

typedef enum {
    E_SB_USR = 0,
    E_SB_PWD,
    E_SB_SID,
    E_NO_INI_SUBFNT
} E_INI_DB_SUB_SEC;

typedef enum {
    E_SF_USR = 0,
    E_SF_PWD,
    E_SF_SID,
    E_NO_INI_OSISFF
} E_INI_DB_SFF_SEC;

typedef enum {
    E_F_BIL_NAME=0,
    E_F_NET_TYPE,
    E_F_ORD_TYPE,
    E_F_SERVICE,
    E_F_PROMO,
    E_F_BIZ_TYPE,
    E_F_BC_DAY,
    E_F_MAIL_APP,
    E_F_PRETTY_FILE,
    E_F_PROMO_FMC,
    E_NO_INI_CMN_CONF
} E_INI_CMN_CONF;

typedef enum {
    E_ACT_DAY = 0,
    E_RUN_BC,
    E_CQS_MAX_CHG,
    E_CQS_MAX_DUR,
    E_CQS_MAX_VOL,
    E_MTC_SCH_DAY,
    E_MTC_MAX_DUR,
    E_RPT_PATH,
    E_RPT_PREFIX,
    E_RPT_SUFFIX,
    E_XCL_TMPL_FILE,
    E_MAIL_LIST,
    E_TMP_DIR,
    E_NO_INI_ALRT
} E_INI_ALRT_CONF;

int  readConfig(int argc, char *argv[]);
void logHeader();
int  _ini_callback(const char *section, const char *key, const char *value, void *userdata);
void printUsage();
int  validateIni();
void makeIni();


#ifdef  __cplusplus
    }
#endif


#endif  // __UNIDEN_REP_H__

