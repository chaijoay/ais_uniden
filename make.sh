#!/usr/bin/ksh
INCLUDES="-I. -I/usr/include -I../../include -I/home/cfms/sqlite-autoconf-3330000"
CC=gcc
PROC="${ORACLE_HOME}/bin/proc"
CFLAGS="-g -Wall -m64"
PROCINCLUDE="include=../../include include=${ORACLE_HOME}/precomp/public include=${ORACLE_HOME}/rdbms/public include=/home/cfms/sqlite-autoconf-3330000"
PROCFLAGS="lines=yes sqlcheck=syntax mode=oracle maxopencursors=200 dbms=v8 ${PROCINCLUDE}"
ORACLE_INCLUDES="-I${ORACLE_HOME}/precomp/public -I${ORACLE_HOME}/rdbms/public"
BIN_DIR=./bin
OBJ_DIR=./obj
LIB_DIR=../../libs/c


${CC} ${CFLAGS} -o ${OBJ_DIR}/procsig.o   -c ${LIB_DIR}/procsig.c   ${INCLUDES}

${CC} ${CFLAGS} -o ${OBJ_DIR}/strlogutl.o -c ${LIB_DIR}/strlogutl.c ${INCLUDES}

${CC} ${CFLAGS} -o ${OBJ_DIR}/minIni.o    -c ${LIB_DIR}/minIni.c    ${INCLUDES}

${CC} ${CFLAGS} -o ${OBJ_DIR}/mp_feed.o   -c ./mp_feed.c            ${INCLUDES}

${PROC} ${PROCFLAGS} mp_feed_dbu.pc ${PROCINCLUDE}

${CC} -o ${OBJ_DIR}/mp_feed_dbu.o ${CFLAGS} ${ORACLE_INCLUDES} ${INCLUDES} -c ./mp_feed_dbu.c

${CC} ${CFLAGS} -lm -lsqlite3 -L${ORACLE_HOME}/lib -lclntsh -o ${BIN_DIR}/mp_feed.exe ${OBJ_DIR}/minIni.o ${OBJ_DIR}/strlogutl.o ${OBJ_DIR}/procsig.o ${OBJ_DIR}/mp_feed.o ${OBJ_DIR}/mp_feed_dbu.o

    rm -f /app/hpe/devsrc/preproc/mp_feed/output/dir?/*
    rm -f /app/hpe/devsrc/preproc/mp_feed/reject/*
    rm -f /tmp/.mp_feed_0.lock
    ${CC} ${CFLAGS} -o ${OBJ_DIR}/mp_feed.o   -c ./mp_feed.c            ${INCLUDES}
    ${CC} ${CFLAGS} -lm -lsqlite3 -L${ORACLE_HOME}/lib -lclntsh -o ${BIN_DIR}/mp_feed.exe ${OBJ_DIR}/minIni.o ${OBJ_DIR}/strlogutl.o ${OBJ_DIR}/procsig.o ${OBJ_DIR}/mp_feed.o ${OBJ_DIR}/mp_feed_dbu.o
    rm  state/* temp/* log/*
    ./bin/mp_feed.exe -n ./mp_feed_test.ini &


${CC} -o ${OBJ_DIR}/mp_feed_dbu.o -g -m64 ${ORACLE_INCLUDES} ${INCLUDES} -c ./mp_feed_dbu.c


# set INCLUDES="-I. -I/usr/include -I../include -I/home/cfms/sqlite-autoconf-3330000"
# set CC=gcc
# set PROC="${ORACLE_HOME}/bin/proc"
# set CFLAGS="-g -m64"
# set PROCINCLUDE="include=../../include include=${ORACLE_HOME}/precomp/public include=${ORACLE_HOME}/rdbms/public include=/home/cfms/sqlite-autoconf-3330000"
# set PROCFLAGS="lines=yes sqlcheck=syntax mode=oracle maxopencursors=200 dbms=v8 ${PROCINCLUDE}"
# set ORACLE_INCLUDES="-I${ORACLE_HOME}/precomp/public -I${ORACLE_HOME}/rdbms/public"
# set BIN_DIR=./bin
# set OBJ_DIR=./obj
# set LIB_DIR=../libs/c
#
# ${CC} ${CFLAGS} -o uniden_rep.o  -c   uniden_rep.c   ${INCLUDES}
# ${CC} ${CFLAGS} -o sqlite_util.o -c   sqlite_util.c  ${INCLUDES}
# ${CC} ${CFLAGS} -o mssql_util.o  -c   mssql_util.c   ${INCLUDES} -lodbc
# ${CC} ${CFLAGS} -lm -lsqlite3 -lodbc -o ./uniden.exe  mssql_util.o  sqlite_util.o  uniden_rep.o minIni.o   procsig.o  strlogutl.o