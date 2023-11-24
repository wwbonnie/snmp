#ifndef SNMP_DEF_H
#define SNMP_DEF_H

/*
#if (!SNMP_HAVE_CHAR)
#define CHAR    char
#endif

#if (!SNMP_HAVE_UINT8)
#define UINT8   unsigned char
#endif

#if (!SNMP_HAVE_UINT16)
#define UINT16  unsigned short
#endif

#if (!SNMP_HAVE_UINT32)
#define UINT32  unsigned int
#endif

#if (!SNMP_HAVE_INT32)
#define INT32   int
#endif

#if (!SNMP_HAVE_VOID)
#define VOID    void
#endif

#if (!SNMP_HAVE_BOOL)
#define BOOL    int
#endif

*/

#ifndef SNMP_OID
#define SNMP_OID unsigned int
#endif

/*
#define TRUE    1
#define FALSE   0
#define NULL    0
*/

#define SNMP_LOG_DETAIL          (BYTE) 0
#define SNMP_LOG_MSG             (BYTE) 1
#define SNMP_LOG_WARN            (BYTE) 2
#define SNMP_LOG_ERROR           (BYTE) 3
#define SNMP_LOG_FAIL            (BYTE) 4
#define SNMP_LOG_FATAL           (BYTE) 5

/*
#define SNMP_AGENT_MALLOC   malloc
#define SNMP_AGENT_FREE(x)  \
{                           \
    if (x)                  \
    {                       \
        free(x);            \
        (x) = NULL;         \
    }                       \
}

#define OID     oid
*/

#define HANDLER_CAN_GETANDGETNEXT     0x01          /* must be able to do both */
#define HANDLER_CAN_SET               0x02          /* implies create, too */
#define HANDLER_CAN_GETBULK           0x04
#define HANDLER_CAN_NOT_CREATE        0x08          /* auto set if ! CAN_SET */
#define HANDLER_CAN_BABY_STEP         0x10
#define HANDLER_CAN_STASH             0x20


#define HANDLER_CAN_RONLY       (HANDLER_CAN_GETANDGETNEXT)
#define HANDLER_CAN_RWRITE      (HANDLER_CAN_GETANDGETNEXT | HANDLER_CAN_SET)
#define HANDLER_CAN_SET_ONLY    (HANDLER_CAN_SET | HANDLER_CAN_NOT_CREATE)
#define HANDLER_CAN_DEFAULT     (HANDLER_CAN_RONLY | HANDLER_CAN_NOT_CREATE)

#define MIB_R           HANDLER_CAN_RONLY
#define MIB_W           HANDLER_CAN_SET_ONLY
#define MIB_RW          HANDLER_CAN_RWRITE
#define MIB_RC          HANDLER_CAN_RWRITE

//#define SNMP_MAX_OID_SIZE 32

#define SNMP_COLUMN_STATE_NULL      0
#define SNMP_COLUMN_STATE_SET       1

#define SNMP_TABLE_STATE_NULL       0
#define SNMP_TABLE_STATE_SET        1   /* 部分列SET */
#define SNMP_TABLE_STATE_SETALL     2   /* 所有列都已经被SET */

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)   sizeof(a)/sizeof((a)[0])
#endif

#ifndef OID_LENGTH
#define OID_LENGTH(x)  (sizeof(x)/sizeof(oid))
#endif

#ifdef WIN32
#define vsnprintf   _vsnprintf
#define strcasecmp  stricmp
#endif

#define SNMP_MAX_TABLE_NUM              128
#define SNMP_MAX_TABLE_COLUMN_NUM       48
#define SNMP_MAX_KEY_COLUMN_NUM         4

#define MAX_TABLE_COLUMN_ENUM_NUM       20

#define SNMP_INIT_TRAP_VAR(pstVarList, oid_trap)                            \
{                                                                           \
    if (snmp_varlist_add_var(&pstVarList, SNMP_TRAP, OID_LENGTH(SNMP_TRAP), \
                             snmp_get_asn1_type(TYPE_OID),                  \
                             (UINT8*)oid_trap, sizeof(oid_trap)) < 0)       \
    {                                                                       \
        return FALSE;                                                       \
    }                                                                       \
}


#ifdef __cplusplus
#define INT2PTR(x)  ((CHAR*)intptr_t(x))
#else
#define INT2PTR(x)  ((CHAR*)(x))
#endif

#define SNMP_ADD_TRAP_VAR(pstVarList, pstOID, ucVarType, var)               \
{   UINT8   __ucVarType = ucVarType;                                        \
    if (TYPE_CHAR == __ucVarType)                                           \
    {                                                                       \
        if (snmp_varlist_add_var(&pstVarList, pstOID, OID_LENGTH(pstOID),   \
                                 snmp_get_asn1_type(__ucVarType),           \
                            (UINT8*)INT2PTR(var), strlen(INT2PTR(var))) < 0)\
        {                                                                   \
            return FALSE;                                                   \
        }                                                                   \
    }                                                                       \
    else                                                                    \
    {                                                                       \
        if (snmp_varlist_add_var(&pstVarList, pstOID, OID_LENGTH(pstOID),   \
                                 snmp_get_asn1_type(__ucVarType),           \
                                 (UINT8*)&(var), sizeof(var)))              \
        {                                                                   \
            return FALSE;                                                   \
        }                                                                   \
    }                                                                       \
}

#define SNMP_ADD_SCALAR_VAR(pstVarList, pstOID, ucVarType, var)             \
{   UINT8   __ucVarType = ucVarType;                                        \
    if (TYPE_CHAR == __ucVarType)                                           \
    {                                                                       \
        if (snmp_varlist_add_var_ex(&pstVarList, pstOID, OID_LENGTH(pstOID),\
                                    0, snmp_get_asn1_type(__ucVarType),  \
                            (UINT8*)INT2PTR(var), strlen(INT2PTR(var))) < 0)\
        {                                                                   \
            return FALSE;                                                   \
        }                                                                   \
    }                                                                       \
    else                                                                    \
    {                                                                       \
        if (snmp_varlist_add_var_ex(&pstVarList, pstOID, OID_LENGTH(pstOID),\
                                 0, snmp_get_asn1_type(__ucVarType),     \
                                 (UINT8*)&(var), sizeof(var)))              \
        {                                                                   \
            return FALSE;                                                   \
        }                                                                   \
    }                                                                       \
}

typedef struct
{
    UINT32   ulValue;
    CHAR     acDisValue[32];
}SNMP_COLUMN_ENUM_T;

typedef struct
{
    SNMP_OID        astOID[SNMP_MAX_OID_SIZE];
    UINT32          ulOIDSize;

    UINT16          usColumnID;
    UINT8           ucColumnType;
    UINT8           ucMibOper;

    UINT16          usColumnLen;
    UINT16          usOffSet;

    CHAR            acColumnName[64];
    UINT32          ulEnumNum;
    SNMP_COLUMN_ENUM_T astColumnEnums[MAX_TABLE_COLUMN_ENUM_NUM];
    INT32           lMin;
    INT32           lMax;
    CHAR            acUnits[32];
    CHAR            acDftVal[32];
}SNMP_COLUMN_T;

typedef BOOL (SNMP_KEY_VALID_FUNC)(VOID *pKey);
typedef BOOL (SNMP_TABLE_VALID_FUNC)(VOID *pData, VOID *pOriData);

typedef struct SNMP_TABLE_T
{
    struct SNMP_TABLE_T *pstNext;

    UINT32              ulTableID;
//    CHAR                acTableName[32];
    BOOL                bIsTable;

 //   UINT32              ulOIDSize;          /* OID个数 */
 //   SNMP_OID            astOID[SNMP_MAX_OID_SIZE];

    UINT32              ulMaxTableRecNum;   /* 最大记录数 */
    UINT32              ulTableRecLen;      /* 对应的表结构长度 */

    SNMP_KEY_VALID_FUNC     *pfKeyValidFunc;
    SNMP_TABLE_VALID_FUNC   *pfTableValidFunc;

    UINT32              ulColumnNum;
    UINT32              ulKeyColumnNum;

    SNMP_COLUMN_T       astColumns[SNMP_MAX_TABLE_COLUMN_NUM];
    SNMP_COLUMN_T       *apstKeyColumns[SNMP_MAX_KEY_COLUMN_NUM];

    VOID                *pTableData;
    UINT32              ulTableRecNum;      /* 当前记录数 */

    VOID                *pOriTableData;     /* 修改前的原始数据 */

    VOID                *pNewTableData;     /* 增加操作时的临时表记录 */

    UINT32              ulSetStartTime;                         /* table add记录的开始时间 */
    UINT32              aulColumnState[SNMP_MAX_TABLE_COLUMN_NUM];
}SNMP_TABLE_T;

typedef struct
{
    UINT32              ulTableNum;
    SNMP_TABLE_T        astTables[SNMP_MAX_TABLE_NUM];
}SNMP_TABLE_INFO_T;

#endif
