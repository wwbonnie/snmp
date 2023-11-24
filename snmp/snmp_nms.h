#ifndef SNMP_NMS_H
#define SNMP_NMS_H

#include "g_include.h"

#ifndef LOG_INFO
#define LOG_EMERG       0       /* system is unusable */
#define LOG_ALERT       1       /* action must be taken immediately */
#define LOG_CRIT        2       /* critical conditions */
#define LOG_ERR         3       /* error conditions */
#define LOG_WARNING     4       /* warning conditions */
#define LOG_NOTICE      5       /* normal but significant condition */
#define LOG_INFO        6       /* informational */
#define LOG_DEBUG       7       /* debug-level messages */
#endif

#ifndef HAVE_PLAT
#define HAVE_PLAT
#endif

#ifndef HAVE_PLAT

#if defined (WIN32)

#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <io.h>
#include <time.h>
#include <errno.h>
#include <direct.h>
#include <winbase.h>

#else

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <semaphore.h>
#include <sys/sysinfo.h>
#include <sys/times.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>
#endif

#ifdef WIN32
#define GetSockErr              WSAGetLastError
#define SetSockErr              WSASetLastError
#define SEWOULDBLOCK            WSAEWOULDBLOCK
#define SEINPROGRESS            WSAEINPROGRESS
#define SEALREADY               WSAEALREADY
#define SENOTSOCK               WSAENOTSOCK
#define SEDESTADDRREQ           WSAEDESTADDRREQ
#define SEMSGSIZE               WSAEMSGSIZE
#define SEPROTOTYPE             WSAEPROTOTYPE
#define SENOPROTOOPT            WSAENOPROTOOPT
#define SEPROTONOSUPPORT        WSAEPROTONOSUPPORT
#define SESOCKTNOSUPPORT        WSAESOCKTNOSUPPORT
#define SEOPNOTSUPP             WSAEOPNOTSUPP
#define SEPFNOSUPPORT           WSAEPFNOSUPPORT
#define SEAFNOSUPPORT           WSAEAFNOSUPPORT
#define SEADDRINUSE             WSAEADDRINUSE
#define SEADDRNOTAVAIL          WSAEADDRNOTAVAIL
#define SENETDOWN               WSAENETDOWN
#define SENETUNREACH            WSAENETUNREACH
#define SENETRESET              WSAENETRESET
#define SECONNABORTED           WSAECONNABORTED
#define SECONNRESET             WSAECONNRESET
#define SENOBUFS                WSAENOBUFS
#define SEISCONN                WSAEISCONN
#define SENOTCONN               WSAENOTCONN
#define SESHUTDOWN              WSAESHUTDOWN
#define SETOOMANYREFS           WSAETOOMANYREFS
#define SETIMEDOUT              WSAETIMEDOUT
#define SECONNREFUSED           WSAECONNREFUSED
#define SELOOP                  WSAELOOP
#define SEHOSTDOWN              WSAEHOSTDOWN
#define SEHOSTUNREACH           WSAEHOSTUNREACH
#define SEPROCLIM               WSAEPROCLIM
#define SEUSERS                 WSAEUSERS
#define SEDQUOT                 WSAEDQUOT
#define SESTALE                 WSAESTALE
#define SEREMOTE                WSAEREMOTE
#define SEFAULT                 WSAEFAULT
#define SEINVAL                 WSAEINVAL
#define SENOTINITIALISED        WSANOTINITIALISED
#define SEINVALFD               WSAENOTSOCK

#else
#define SOCKET_ERROR            (-1)
#define ioctlsocket(a, b, c)    ioctl(a, b, (INT)c)
#define GetSockErr()            errno
#define SetSockErr(err)         {errno = err;}

#define SENOTINITIALISED        0    /* 用于适配, vxworks下无需初始化 */
#define SEWOULDBLOCK            EWOULDBLOCK
#define SEINPROGRESS            EINPROGRESS
#define SEALREADY               EALREADY
#define SENOTSOCK               ENOTSOCK
#define SEDESTADDRREQ           EDESTADDRREQ
#define SEMSGSIZE               EMSGSIZE
#define SEPROTOTYPE             EPROTOTYPE
#define SENOPROTOOPT            ENOPROTOOPT
#define SEPROTONOSUPPORT        EPROTONOSUPPORT
#define SESOCKTNOSUPPORT        ESOCKTNOSUPPORT
#define SEOPNOTSUPP             EOPNOTSUPP
#define SEPFNOSUPPORT           EPFNOSUPPORT
#define SEAFNOSUPPORT           EAFNOSUPPORT
#define SEADDRINUSE             EADDRINUSE
#define SEADDRNOTAVAIL          EADDRNOTAVAIL
#define SENETDOWN               ENETDOWN
#define SENETUNREACH            ENETUNREACH
#define SENETRESET              ENETRESET
#define SECONNABORTED           ECONNABORTED
#define SECONNRESET             ECONNRESET
#define SENOBUFS                ENOBUFS
#define SEISCONN                EISCONN
#define SENOTCONN               ENOTCONN
#define SESHUTDOWN              ESHUTDOWN
#define SETOOMANYREFS           ETOOMANYREFS
#define SETIMEDOUT              ETIMEDOUT
#define SECONNREFUSED           ECONNREFUSED
#define SELOOP                  ELOOP
#define SEHOSTDOWN              EHOSTDOWN
#define SEHOSTUNREACH           EHOSTUNREACH
#define SEPROCLIM               EPROCLIM
#define SEUSERS                 EUSERS
#define SEDQUOT                 EDQUOT
#define SESTALE                 ESTALE
#define SEREMOTE                EREMOTE
#define SEFAULT                 EFAULT
#define SEINVAL                 EINVAL
#define SEINVALFD               ENOTSOCK
#endif

#ifdef WIN32
typedef int socklen_t;
#else
typedef unsigned int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;

#define INVALID_SOCKET          (SOCKET)(~0)
#define closesocket             close

#endif

#endif // #ifndef HAVE_PLAT

#ifndef SNMP_OID
#define SNMP_OID unsigned int
#endif

#ifdef HAVE_SNMP_INCLUDE
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/mib_modules.h>
#else
typedef unsigned int oid;

#define ASN_UNIVERSAL                   0x00
#define ASN_APPLICATION                 0x40
#define ASN_CONTEXT                     0x80
#define ASN_PRIVATE                     0xC0

#define ASN_PRIMITIVE                   0x00
#define ASN_CONSTRUCTOR                 0x20

#define ASN_BOOLEAN         (0x01)
#define ASN_INTEGER         (0x02)
#define ASN_BIT_STR         (0x03)
#define ASN_OCTET_STR       (0x04)
#define ASN_NULL            (0x05)
#define ASN_OBJECT_ID       (0x06)
#define ASN_SEQUENCE        (0x10)
#define ASN_SET             (0x11)

#define ASN_IPADDRESS       (ASN_APPLICATION | 0)
#define ASN_COUNTER         (ASN_APPLICATION | 1)
#define ASN_GAUGE           (ASN_APPLICATION | 2)
#define ASN_UNSIGNED        (ASN_APPLICATION | 2)
#define ASN_TIMETICKS       (ASN_APPLICATION | 3)
#define ASN_OPAQUE          (ASN_APPLICATION | 4)
#define ASN_COUNTER64       (ASN_APPLICATION | 6)

#ifdef OPAQUE_SPECIAL_TYPES // defined types from draft-perkins-opaque-01.txt
#define ASN_FLOAT           (ASN_APPLICATION | 8)
#define ASN_DOUBLE          (ASN_APPLICATION | 9)
#define ASN_INTEGER64       (ASN_APPLICATION | 10)
#define ASN_UNSIGNED64      (ASN_APPLICATION | 11)
#endif

#define SNMP_MSG_GET        (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x0) /* a0=160 */
#define SNMP_MSG_GETNEXT    (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x1) /* a1=161 */
#define SNMP_MSG_RESPONSE   (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x2) /* a2=162 */
#define SNMP_MSG_SET        (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x3) /* a3=163 */
#define SNMP_MSG_TRAP       (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x4) /* a4=164 */
#define SNMP_MSG_GETBULK    (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x5) /* a5=165 */
#define SNMP_MSG_INFORM     (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x6) /* a6=166 */
#define SNMP_MSG_TRAP2      (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x7) /* a7=167 */
#define SNMP_MSG_REPORT     (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x8) /* a8=168 */

#define SNMP_END_OF_MIBVIEW (ASN_CONTEXT | 0x2)

#define SNMP_ERR_NOERROR            (0)
#define SNMP_ERR_TOOBIG             (1)
#define SNMP_ERR_NOSUCHNAME         (2)
#define SNMP_ERR_BADVALUE           (3)
#define SNMP_ERR_READONLY           (4)
#define SNMP_ERR_GENERR             (5)

#define SNMP_ERR_NOACCESS           (6)
#define SNMP_ERR_WRONGTYPE          (7)
#define SNMP_ERR_WRONGLENGTH        (8)
#define SNMP_ERR_WRONGENCODING      (9)
#define SNMP_ERR_WRONGVALUE         (10)
#define SNMP_ERR_NOCREATION         (11)
#define SNMP_ERR_INCONSISTENTVALUE  (12)
#define SNMP_ERR_RESOURCEUNAVAILABLE (13)
#define SNMP_ERR_COMMITFAILED       (14)
#define SNMP_ERR_UNDOFAILED         (15)
#define SNMP_ERR_AUTHORIZATIONERROR (16)
#define SNMP_ERR_NOTWRITABLE        (17)
#define SNMP_ERR_INCONSISTENTNAME   (18)
#define MAX_SNMP_ERR                18

#define SNMP_ERR_NOSUCHINSTANCE     (0x81)

#define STAT_SUCCESS        0
#define STAT_ERROR          1
#define STAT_TIMEOUT        2

#endif

#ifndef EINTR
#define EINTR 4
#endif

#ifndef IP_FMT
#define IP_FMT      "%d.%d.%d.%d"
#define IP_ARG(x)   ((unsigned char*)(x))[0],((unsigned char*)(x))[1],((unsigned char*)(x))[2],((unsigned char*)(x))[3]
#endif

#ifndef MAC_FMT
#define MAC_FMT     "%02X-%02X-%02X-%02X-%02X-%02X"
#define MAC_ARG(x)  ((unsigned char*)(x))[0],((unsigned char*)(x))[1],((unsigned char*)(x))[2],((unsigned char*)(x))[3],((unsigned char*)(x))[4],((unsigned char*)(x))[5]
#endif

#define SNMP_ID_SEQUENCE        0x30
#define SNMP_V1                 0
#define SNMP_V2                 1

//#define SNMP_GET_VER            SNMP_V1
#define SNMP_TRAP_VER           SNMP_V2

//#define SNMP_SET_VER            SNMP_V1

#define SNMP_MAX_COMMUNITY_LEN  32

#define SNMP_TRAP_OID       1,3,6,1,6,3,1,1,4,1,0
#define SNMP_TIMESTAMP_OID  1,3,6,1,2,1,1,3,0

#define SNMP_SOCKET         unsigned int

//#define SNMP_MALLOC         malloc
//#define SNMP_FREE           free

#define TYPE_NULL       0x00
#define TYPE_CHAR       0x01    /* 字符串，多字节 */
#define TYPE_BYTE       0x02
#define TYPE_SHORT      0x03
#define TYPE_LONG       0x04
#define TYPE_BOOL       0x05    /* 对应TruthValue */
#define TYPE_INT        0x06
#define TYPE_CNT        0x07    // COUNTER
#define TYPE_CNT64      0x08    // COUNTER64

#define TYPE_BIN        0x20
#define TYPE_IP         0x21
#define TYPE_MAC        0x22
#define TYPE_TIME       0x23
#define TYPE_OID        0x50

#define SNMP_PORT       161
#define SNMP_TRAP_PORT  162

typedef struct
{
    unsigned int        ulLen;

    unsigned char       ucLenNum;
    unsigned char       aucLen[7];
}SNMP_LEN_T;

typedef struct SNMP_VALUE_T
{
    unsigned char       ucType;
    unsigned int        ulLen;
    unsigned char       *pucValue;
    struct SNMP_VALUE_T *pstNext;
}SNMP_VALUE_T;

typedef struct SNMP_OID_T
{
    unsigned int        astOID[32];
    unsigned int        ulOIDSize;
    struct SNMP_OID_T   *pstNext;
}SNMP_OID_T;

typedef struct
{
    unsigned int        ulColumnNum;

    SNMP_OID_T          *pstOID;
    SNMP_VALUE_T        *pstValue;
}SNMP_TABLE_VALUE_T;

typedef struct
{
    unsigned char       ucType;

    unsigned char       ucLenNum;
    unsigned char       aucLen[8];

    unsigned int        ulLen;
    unsigned char       ucNeedFree;
    unsigned char       *pucValue;
}SNMP_TLV_T;

typedef struct
{
    unsigned char       ucPDUType;
    unsigned char       ucPDULen;     //需要考虑超过128

    unsigned char       ucVerType;
    unsigned char       ucVerLen;
    unsigned char       ucVerValue;

    unsigned char       ucCommunityType;
    unsigned char       ucCommunityLen;
    unsigned char       aucCommunity[64];

    unsigned char       ucSnmpPDUType;
    unsigned char       ucSnmpPDULen;

    unsigned char       ucReqIDType;
    unsigned char       ucReqIDLen;
    unsigned short      usReqID;
    unsigned char       aucReqIDValue[2];

    unsigned char       ucResv1Type;
    unsigned char       ucResv1Len;
    unsigned char       ucResv1Value;

    unsigned char       ucResv2Type;
    unsigned char       ucResv2Len;
    unsigned char       ucResv2Value;

    unsigned char       ucParasType;
    unsigned char       ucParasLen;

    unsigned char       ucPara1Type;
    unsigned char       ucPara1Len;

    unsigned char       ucOIDType;
    unsigned char       ucOIDSize;
    unsigned char       aucOID[64];   //OID
    unsigned char       ucOIDValue;

    unsigned char       ucEndMark;
}SNMP_GET_PDU_T;

typedef struct
{
    SNMP_TLV_T          stPDU;
    SNMP_TLV_T          stVer;
    SNMP_TLV_T          stCommunity;
    SNMP_TLV_T          stSnmpPDU;
    SNMP_TLV_T          stReqID;
    SNMP_TLV_T          stErrStatus;
    SNMP_TLV_T          stErrIndex;
    SNMP_TLV_T          stParas;
    SNMP_TLV_T          stPara1;
    SNMP_TLV_T          stOID;
    unsigned char       ucOIDValue;
    unsigned char       ucEndMark;
}SNMP_GET_SCALAR_PDU_T;

typedef struct
{
    SNMP_TLV_T          stPDU;
    SNMP_TLV_T          stVer;
    SNMP_TLV_T          stCommunity;
    SNMP_TLV_T          stSnmpPDU;
    SNMP_TLV_T          stReqID;
    SNMP_TLV_T          stErrStatus;
    SNMP_TLV_T          stErrIndex;
    SNMP_TLV_T          stParas;
    SNMP_TLV_T          stPara1;
    SNMP_TLV_T          stOID;
    SNMP_TLV_T          stValue;
}SNMP_GET_SCALAR_RSP_PDU_T;

typedef SNMP_GET_SCALAR_PDU_T SNMP_GET_TABLE_PDU_T;
/*
typedef struct
{
    SNMP_TLV_T          stPDU;
    SNMP_TLV_T          stVer;
    SNMP_TLV_T          stCommunity;
    SNMP_TLV_T          stSnmpPDU;
    SNMP_TLV_T          stReqID;
    SNMP_TLV_T          stErrStatus;
    SNMP_TLV_T          stErrIndex;
    SNMP_TLV_T          stParas;
    SNMP_TLV_T          stPara1;
    SNMP_TLV_T          stOID;
//  SNMP_TLV_T          stValue;
    unsigned char       ucOIDValue;
    unsigned char       ucEndMark;
}SNMP_GET_TABLE_PDU_T;
*/

typedef struct
{
    SNMP_TLV_T          stPDU;
    SNMP_TLV_T          stVer;
    SNMP_TLV_T          stCommunity;
    SNMP_TLV_T          stSnmpPDU;
    SNMP_TLV_T          stReqID;
    SNMP_TLV_T          stErrStatus;
    SNMP_TLV_T          stErrIndex;
    SNMP_TLV_T          stParas;

    unsigned int        ulOIDNum;
    SNMP_TLV_T          *pstPara;
    SNMP_TLV_T          *pstOID;

    unsigned char       ucOIDValue;
    unsigned char       ucEndMark;
}SNMP_GET_BATCH_PDU_T;

typedef struct
{
    SNMP_TLV_T          stPDU;
    SNMP_TLV_T          stVer;
    SNMP_TLV_T          stCommunity;
    SNMP_TLV_T          stSnmpPDU;
    SNMP_TLV_T          stReqID;
    SNMP_TLV_T          stNoRepeaters;
    SNMP_TLV_T          stMaxRepetitions;
    SNMP_TLV_T          stParas;
    SNMP_TLV_T          stPara1;
    SNMP_TLV_T          stOID;
    unsigned char       ucOIDValue;
    unsigned char       ucEndMark;
}SNMP_GET_BULK_PDU_T;

typedef struct SNMP_OID_TLV_T
{
    SNMP_TLV_T          stPara;
    SNMP_TLV_T          stOID;
    SNMP_TLV_T          stValue;
    struct SNMP_OID_TLV_T   *pstNext;
}SNMP_OID_TLV_T;

typedef struct
{
    SNMP_TLV_T          stPDU;
    SNMP_TLV_T          stVer;
    SNMP_TLV_T          stCommunity;
    SNMP_TLV_T          stSnmpPDU;
    SNMP_TLV_T          stReqID;
    SNMP_TLV_T          stErrStatus;
    SNMP_TLV_T          stErrIndex;
    SNMP_TLV_T          stParas;
    SNMP_TLV_T          stPara1;
    SNMP_TLV_T          stOID;
    SNMP_TLV_T          stValue;
}SNMP_SET_PDU_T;

typedef struct
{
    SNMP_TLV_T          stPDU;
    SNMP_TLV_T          stVer;
    SNMP_TLV_T          stCommunity;
    SNMP_TLV_T          stSnmpPDU;
    SNMP_TLV_T          stReqID;
    SNMP_TLV_T          stErrStatus;
    SNMP_TLV_T          stErrIndex;
    SNMP_TLV_T          stParas;

    UINT32              ulOIDNum;
    SNMP_OID_TLV_T      *pstPara;
}SNMP_SET_BULK_PDU_T;

#define SNMP_MAX_OID_SIZE   32

typedef struct
{
    SNMP_OID            astOID[SNMP_MAX_OID_SIZE];
    unsigned int        ulOIDSize;
    unsigned char       *pucReqData;
    unsigned int        ulReqDataType;
    unsigned int        ulReqDataLen;
}SNMP_SET_VAR_T;


/*
stPDU;              30 81 94
stVer;              02 01 01
stCommunity;        04 06 70 75 62 6c 69 63
stPDUType;          a7 81 86
reqID               02 02 3a b2
resv1               02 01 00
resv2               02 01 00
stParas             30 7a
timestamp:  30 0e : 06 08 2b 06 01 02 01 01 03 00 : 43 02 27 63
alarmOID:   30 1a : 06 0a 2b 06 01 06 03 01 01 04 01 00 : 06 0c 2b 06 01 04 01 81 e0 33 03 05 02 02
alarmSN:    30 1b : 06 0e 2b 06 01 04 01 81 e0 33 03 05 02 01 01 00 : 04 09 30 30 30 30 30 30 30 30 32
neName :    30 1a : 06 0e 2b 06 01 04 01 81 e0 33 03 05 02 01 02 00 : 04 08 4e 65 20 2d 20 41 43 31
alarmLevel: 30 13 : 06 0e 2b 06 01 04 01 81 e0 33 03 05 02 01 03 00 : 42 01 02
*/

typedef struct SNMP_VAR_T
{
    SNMP_TLV_T          stPara;
    SNMP_TLV_T          stOID;
    SNMP_TLV_T          stValue;

    struct SNMP_VAR_T   *pstNext;
}SNMP_VAR_T;

typedef struct
{
    SNMP_TLV_T          stPDU;          // 30 81 94
    SNMP_TLV_T          stVer;          // 02 01 01
    SNMP_TLV_T          stCommunity;    // 04 06 70 75 62 6c 69 63
    SNMP_TLV_T          stSnmpPDU;      // a7 81 86
    SNMP_TLV_T          stReqID;        // 02 02 3a b2
    SNMP_TLV_T          stErrStatus;    // 02 01 00
    SNMP_TLV_T          stErrIndex;     // 02 01 00
    SNMP_TLV_T          stParas;        // 30 7a
    SNMP_VAR_T          *pstVarList;    // 30 1a : 06 0a 2b 06 01 06 03 01 01 04 01 00 : 06 0c 2b 06 01 04 01 81 e0 33 03 05 02 02
}SNMP_GET_REQ_PDU_T, SNMP_SET_REQ_PDU_T;

typedef struct
{
    SNMP_TLV_T          stPDU;          // 30 81 94
    SNMP_TLV_T          stVer;          // 02 01 01
    SNMP_TLV_T          stCommunity;    // 04 06 70 75 62 6c 69 63
    SNMP_TLV_T          stSnmpPDU;      // a7 81 86
    SNMP_TLV_T          stReqID;        // 02 02 3a b2
    SNMP_TLV_T          stErrStatus;    // 02 01 00
    SNMP_TLV_T          stErrIndex;     // 02 01 00
    SNMP_TLV_T          stParas;        // 30 7a
//    SNMP_VAR_T            stTimestamp;    // 30 0e : 06 08 2b 06 01 02 01 01 03 00 : 43 02 27 63
    SNMP_VAR_T          *pstVarList;    // 30 1a : 06 0a 2b 06 01 06 03 01 01 04 01 00 : 06 0c 2b 06 01 04 01 81 e0 33 03 05 02 02
}SNMP_GET_RSP_PDU_T, SNMP_SET_RSP_PDU_T;

typedef struct
{
    SNMP_TLV_T          stPDU;          // 30 81 94
    SNMP_TLV_T          stVer;          // 02 01 01
    SNMP_TLV_T          stCommunity;    // 04 06 70 75 62 6c 69 63
    SNMP_TLV_T          stSnmpPDU;      // a7 81 86
    SNMP_TLV_T          stReqID;        // 02 02 3a b2
    SNMP_TLV_T          stErrStatus;    // 02 01 00
    SNMP_TLV_T          stErrIndex;     // 02 01 00
    SNMP_TLV_T          stParas;        // 30 7a
    SNMP_VAR_T          stTimestamp;    // 30 0e : 06 08 2b 06 01 02 01 01 03 00 : 43 02 27 63
    SNMP_VAR_T          *pstVarList;    // 30 1a : 06 0a 2b 06 01 06 03 01 01 04 01 00 : 06 0c 2b 06 01 04 01 81 e0 33 03 05 02 02
}SNMP_TRAP2_PDU_T;

typedef struct
{
    unsigned char   aucAgentIP[4];
    unsigned short  usAgentPort;
    char            acReadCommunity[SNMP_MAX_COMMUNITY_LEN+1];
    char            acWriteCommunity[SNMP_MAX_COMMUNITY_LEN+1];
    unsigned int    ulGetVer;
    unsigned int    ulSetVer;
    unsigned int    ulTimeout;
    SOCKET          stSock;
}SNMP_SESSION_T;

#ifdef __cplusplus
extern "C" {
#endif

typedef void* (*SNMP_MALLOC)(unsigned int ulSize);
typedef void (*SNMP_FREE)(void *p);

void snmp_set_malloc_free(SNMP_MALLOC pfMalloc, SNMP_FREE pfFree);
void* snmp_malloc(unsigned int ulSize);
void* snmp_calloc(unsigned int ulBlock, unsigned int ulSize);
char* snmp_strdup(const char *s);
void snmp_free(void *p);

//typedef SNMP_GET_TABLE_FUNC (int)(SNMP_VALUE_T *pstValue, void *pData);
void snmp_log(int iLogLevel, const char *szLogFormat, ...);
void snmp_free_value(SNMP_VALUE_T *pstValue);
SNMP_SESSION_T* snmp_init_session(unsigned char *pucAgengIP,
                                  unsigned short usAgentPort,
                                  char *szReadCommunity,
                                  char *szWriteCommunity,
                                  unsigned int ulGetVer,
                                  unsigned int ulSetVer,
                                  unsigned int ulTimeout);
void snmp_free_session(SNMP_SESSION_T *pstSession);

int snmp_get_scalar(SNMP_SESSION_T *pstSession, SNMP_OID *pOID,
                    unsigned int ulOIDSize, /*unsigned char ucType,*/
                    unsigned int ulMaxDataLen, void *pData, unsigned int *pulDataLen);
int snmp_set_data(SNMP_SESSION_T *pstSession, SNMP_OID *pOID,
                  unsigned int ulOIDSize, unsigned char ucType,
                  unsigned int ulDataLen, void *pData);
int snmp_set_bulk(SNMP_SESSION_T *pstSession, SNMP_SET_VAR_T *pstSetVar, unsigned int ulColumnNum);
int snmp_get_next(SNMP_SESSION_T *pstSession, SNMP_OID *pOID, unsigned int ulOIDSize,
                  SNMP_OID *pNextOID, unsigned int *pulOIDSize, SNMP_VALUE_T *pstValue);

int snmp_get_bulk(SNMP_SESSION_T *pstSession, SNMP_TABLE_VALUE_T *pstValue, SNMP_OID_T *pstTableOID);
int snmp_get_batch(SNMP_SESSION_T *pstSession, SNMP_TABLE_VALUE_T *pstValue);

int snmp_varlist_add_var(SNMP_VAR_T **pstVarList, SNMP_OID *pOID, unsigned int ulOIDSize,
                         unsigned char ucReqDataType, unsigned char *pucReqData,  unsigned int ulReqDataLen);
int snmp_varlist_add_var_ex(SNMP_VAR_T **pstVarList, SNMP_OID *pOID, unsigned int ulOIDSize, unsigned char bAppendZero,
                         unsigned char ucReqDataType, unsigned char *pucReqData, unsigned int ulReqDataLen);

int snmp_send_trap(SOCKET stSock, unsigned char *pucServerIP, unsigned short usServerPort, char *szCommunity, unsigned int ulTimeout, unsigned int ulTrapType, SNMP_VAR_T *pstVarList);
int snmp_send_trap2(unsigned char *pucServerIP, unsigned short usServerPort, char *szCommunity, unsigned int ulTimeout, SNMP_VAR_T *pstVarList);
int snmp_send_inform(unsigned char *pucServerIP, unsigned short usServerPort, char *szCommunity, unsigned int ulTimeout, SNMP_VAR_T *pstVarList);
int snmp_parse_trap2_pdu(unsigned char *pucRspData,
                        unsigned int ulRspDataLen,
                        SNMP_TRAP2_PDU_T *pstPDU);
int snmp_parse_oid_var(unsigned char *pucVarData, unsigned char ucVarLen,
                       SNMP_OID *pOID, unsigned int *pulOIDSize);
int snmp_free_get_pdu(SNMP_GET_SCALAR_PDU_T *pstPDU);
int snmp_free_getbulk_pdu(SNMP_GET_BULK_PDU_T *pstPDU);
int snmp_free_trap2_pdu(SNMP_TRAP2_PDU_T *pstPDU);
void snmp_free_get_batch_pdu(SNMP_GET_BATCH_PDU_T *pstPDU);
int snmp_init_socket(void);
int snmp_recv_from(SNMP_SOCKET stSock, void *pMsgBuf, unsigned int ulMsgBufLen, unsigned int ulTimeout);
unsigned char snmp_get_asn1_type(unsigned char usType);

unsigned int snmp_get_int_value(unsigned char *pucValue, unsigned char ucSize);
int snmp_init_var(SNMP_VAR_T *pstVar, SNMP_OID *pOID, unsigned int ulOIDSize,
                  unsigned char ucReqDataType, unsigned char *pucReqData,   unsigned int ulReqDataLen);
int snmp_set_len_var(SNMP_TLV_T *pVar);
int snmp_free_tlv(SNMP_TLV_T *pstVar);

void nms_get_data(UINT8 ucType, UINT32 ulLen, UINT8 *pucValue, VOID *pvOutData);

int snmp_recv_get_scalar(SNMP_SESSION_T *pstSession, SNMP_OID *pOID, unsigned int ulOIDSize, /*unsigned char ucType,*/
                         unsigned int ulMaxDataLen, void *pData, unsigned int *pulDataLen);
int snmp_create_get_rsp_pdu(SNMP_GET_SCALAR_RSP_PDU_T *pstPDU, unsigned char *pucReqPDU,
    unsigned int ulMaxReqPDULen, unsigned int *pulReqPDULen);

int snmp_init_get_rsp_pdu(char *szCommunity, unsigned char ucGetVer, SNMP_OID *pOID, unsigned int ulOIDSize,
    unsigned int ulReqSeq, unsigned char *pucRspData, unsigned char ucRspDataType,
    unsigned int ulRspDataLen, SNMP_GET_SCALAR_RSP_PDU_T *pstPDU);
int snmp_free_get_scalar_rsp_pdu(SNMP_GET_SCALAR_RSP_PDU_T *pstPDU);

int snmp_init_batch_get_rsp_pdu(unsigned char ucVersion, char *szCommunity, unsigned int ulReqID, SNMP_GET_RSP_PDU_T *pstPDU);
int snmp_create_batch_get_rsp_pdu(SNMP_GET_RSP_PDU_T *pstPDU, unsigned char *pucPDU,
    unsigned int ulMaxReqPDULen, unsigned int *pulPDULen);
int snmp_free_get_rsp_pdu(SNMP_GET_RSP_PDU_T *pstPDU);

int snmp_parse_get_scalar_req(unsigned char ucGetVer, unsigned char *pucRspData, unsigned int ulRspDataLen, unsigned int *pulReqSeq, SNMP_OID *pOID, unsigned int *pulOIDSize);
int snmp_parse_set_scalar_req(unsigned char ucSetVer, unsigned char *pucRspData, unsigned int ulRspDataLen, unsigned int *pulReqSeq, SNMP_OID *pOID, unsigned int *pulOIDSize);
int snmp_parse_get_scalar_rsp(unsigned char ucSetVer, unsigned char *pucRspData, unsigned int ulRspDataLen, unsigned int *pulReqSeq, SNMP_OID *pOID, unsigned int *pulOIDSize);
int snmp_send_get_scalar_rsp(SNMP_SESSION_T *pstSession, SNMP_OID *pOID, unsigned int ulOIDSize, unsigned int ulReqID, unsigned char ucDataType, unsigned int ulDataLen, void *pData);
int snmp_send_get_scalar_rsp_ex(SNMP_SESSION_T *pstSession, SOCKET stSock, SNMP_OID *pOID, unsigned int ulOIDSize, unsigned int ulReqID, unsigned char ucDataType, unsigned int ulDataLen, void *pData);
int snmp_send_get_rsp(SNMP_SESSION_T *pstSession, unsigned int ulReqID, SNMP_VAR_T *pstVarList);
int snmp_send_get_rsp_ex(SNMP_SESSION_T *pstSnmpSession, SOCKET stSock, unsigned int ulReqID, SNMP_VAR_T *pstVarList);

unsigned short snmp_get_last_req_id(void);

#ifdef __cplusplus
}
#endif


#endif

