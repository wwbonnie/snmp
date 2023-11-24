#include "g_include.h"
#include "snmp_nms.h"

static SNMP_MALLOC     g_SnmpMalloc = NULL;
static SNMP_FREE       g_SnmpFree = NULL;


#define CHECK_REQ_ID(stReqID, pucRspData)  \
    if (stReqID.pucValue && (unsigned int)ucTmp >= stReqID.ulLen && ucTmp <= 4 &&\
        memcmp(pucRspData+ucTmp-stReqID.ulLen, stReqID.pucValue, stReqID.ulLen) != 0)

#define SET_LEN_VAR(x)          \
if (snmp_set_len_var(x) < 0)    \
{                               \
    return -1;                  \
}

#define TLV_LEN(x)  (1 + (x)->ucLenNum + (x)->ulLen)

#define INIT_VAR(x)                                     \
{                                                       \
    unsigned int    __i;                                \
    if ((++ulCurrPDULen) > ulMaxReqPDULen) return -1;   \
    *pucValue++ = (x)->ucType;                          \
    for (__i=0; __i<(unsigned int)(x)->ucLenNum; __i++) \
    {                                                   \
        if ((++ulCurrPDULen) >ulMaxReqPDULen) return -1;\
        *pucValue++ = (x)->aucLen[__i];                 \
    }                                                   \
    if ((x)->pucValue)                                  \
    {                                                   \
        ulCurrPDULen += (x)->ulLen;                     \
        if (ulCurrPDULen > ulMaxReqPDULen) return -1;   \
        memcpy(pucValue, (x)->pucValue, (x)->ulLen);    \
        pucValue += (x)->ulLen;                         \
    }                                                   \
}

#define DATA_DUP(x, data, len)      \
{                                   \
    (x)->pucValue = (unsigned char*)snmp_malloc(len); \
    if (!(x)->pucValue)             \
    {                               \
        return -1;                  \
    }                               \
    memcpy((x)->pucValue, data, len);\
    (x)->ucNeedFree = 1;            \
}

#define FREE_TLV(x)         \
if (snmp_free_tlv(x) < 0)   \
{                           \
    return -1;              \
}

void snmp_free_value(SNMP_VALUE_T *pstValue)
{
    for(; pstValue != NULL; pstValue = pstValue->pstNext)
    {
        if (pstValue->pucValue)
        {
            snmp_free(pstValue->pucValue);
            pstValue->pucValue = NULL;
        }
    }
}

void snmp_set_malloc_free(SNMP_MALLOC pfMalloc, SNMP_FREE pfFree)
{
    g_SnmpMalloc = pfMalloc;
    g_SnmpFree = pfFree;
}

void* snmp_malloc(unsigned int ulSize)
{
    if (g_SnmpMalloc)
    {
        return g_SnmpMalloc(ulSize);
    }
    else
    {
        return malloc(ulSize);
    }
}

void snmp_free(void *p)
{
    if (p)
    {        
        if (g_SnmpFree)
        {
            g_SnmpFree(p);
        }
        else
        {
            free(p);
        }
    }
}

void* snmp_calloc(unsigned int ulBlock, unsigned int ulSize)
{
    UINT64      u64Size = ulBlock*ulSize;
    void        *v;

    if (u64Size > 0xffffffff)
    {
        return NULL;
    }

    v = snmp_malloc(u64Size);
    if (v)
    {
        memset(v, 0, u64Size);
    }
    //snmp_log(LOG_INFO, "snmp_calloc %u %u, return 0x%X", ulBlock, ulSize, v);

    return v;
}

char* snmp_strdup(const char *s)
{
    char    *d;

    if (!s)
    {
        return NULL;
    }

    d = (char*)snmp_malloc(strlen(s)+1);
    if (d)
    {
        strcpy(d, s);
    }

    //snmp_log(LOG_INFO, "snmp_strdup return 0x%X", d);

    return d;
}

/*************************************************************************
* 函数名称: snmp_free_tlv
* 功    能: 释放PDU中的动态申请的内存
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_free_tlv(SNMP_TLV_T *pstVar)
{
    if (!pstVar)
    {
        return 0;
    }

    if (pstVar->ucNeedFree && pstVar->pucValue)
    {
        snmp_free(pstVar->pucValue);
        pstVar->pucValue = NULL;
        pstVar->ucNeedFree = 0;
    }

    return 0;
}

void nms_print_tlv(SNMP_TLV_T *pstTlv)
{
    unsigned long i=0;
    if (NULL == pstTlv)
    {
        return;
    }

    printf("ucType:%u\n", pstTlv->ucType);
    printf("ucLenNum:%u\n", pstTlv->ucLenNum);
    printf("aucLen:");
    for(i=0; i<8; i++)
    {
        printf("%02x ", pstTlv->aucLen[i]);
    }
    printf("\n");
    printf("ulLen:%u\n", pstTlv->ulLen);
    printf("ucNeedFree:%u\n", pstTlv->ucNeedFree);

    if (pstTlv->pucValue != NULL)
    {
        printf("pucValue:\n");
        for(i=0; i<pstTlv->ulLen; i++)
        {
            printf("%02x ", pstTlv->pucValue[i]);
            if ((i+1)%10 == 0)
            {
                printf("\n");
            }
        }
        printf("\n");
    }
}

void nms_print_binary(unsigned char   *pucRsp, unsigned int ulRspDataLen)
{
    unsigned long i=0;

    printf("RspData:\n");
    printf("ulRspDataLen:%u\n", ulRspDataLen);

    for (i=0;i<ulRspDataLen; i++)
    {
        printf("%02x ", pucRsp[i]);
        if ((i+1)%10 == 0)
        {
            printf("\n");
        }
    }

    printf("\n");

    printf("\n");
}

void nms_print_table_pdu(SNMP_GET_TABLE_PDU_T *pstPDU)
{
    printf("ReqData:\n");

    printf("stPDU:\n");
    nms_print_tlv(&pstPDU->stPDU);

    printf("stVer:\n");
    nms_print_tlv(&pstPDU->stVer);

    printf("stCommunity:\n");
    nms_print_tlv(&pstPDU->stCommunity);

    printf("stSnmpPDU:\n");
    nms_print_tlv(&pstPDU->stSnmpPDU);

    printf("stReqID:\n");
    nms_print_tlv(&pstPDU->stReqID);

    printf("stErrStatus:\n");
    nms_print_tlv(&pstPDU->stErrStatus);

    printf("stErrIndex:\n");
    nms_print_tlv(&pstPDU->stErrIndex);

    printf("stParas:\n");
    nms_print_tlv(&pstPDU->stParas);

    printf("stPara1:\n");
    nms_print_tlv(&pstPDU->stPara1);

    printf("stOID:\n");
    nms_print_tlv(&pstPDU->stOID);

    printf("ucOIDValue:%u\n", pstPDU->ucOIDValue);
    printf("ucEndMark:%u\n", pstPDU->ucEndMark);
}

/*************************************************************************
* 函数名称: snmp_create_oid_var
* 功    能: 生成OID的BER报文
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
#ifndef HAVE_SYSLOG
void snmp_log(int iLogLevel, const char *szLogFormat, ...)
{
    char    acMsg[1024];
    va_list vaMsg;
    char    *szLogLevel;

    va_start(vaMsg, szLogFormat);
    vsprintf(acMsg, szLogFormat, vaMsg);
    va_end(vaMsg);

         if (iLogLevel == LOG_EMERG)    szLogLevel = "Emerg";
    else if (iLogLevel == LOG_ALERT)    szLogLevel = "Alert";
    else if (iLogLevel == LOG_CRIT)     szLogLevel = "Crit";
    else if (iLogLevel == LOG_ERR)      szLogLevel = "Error";
    else if (iLogLevel == LOG_WARNING)  szLogLevel = "Warn";
    else if (iLogLevel == LOG_NOTICE)   szLogLevel = "Notice";
    else if (iLogLevel == LOG_INFO)     szLogLevel = "Info";
    else if (iLogLevel == LOG_DEBUG)    szLogLevel = "Debug";
    else szLogLevel = "";

    printf("%s: %s\n",
            szLogLevel,
            acMsg);

    return;
}
#else
#define snmp_log logu
#endif

/*************************************************************************
* 函数名称: snmp_create_oid_var
* 功    能: 生成OID的BER报文
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_create_oid_var(SNMP_OID *pOID, unsigned int ulOIDSize,
                        unsigned char *pucVarData, unsigned char *pucVarLen)
{
    unsigned int    i, j;
    unsigned int    ulOID;
    unsigned char   aucOID[5];
    unsigned char   ucOIDSize;
    unsigned char   *pucVar = pucVarData;

    //第1、2个字节合并为40*x + y ; 需要考虑超过128，目前固定为 43 = 40*1+3
    *pucVar++ = (unsigned char)(40*pOID[0] + pOID[1]);

    for (i=2; i<ulOIDSize; i++)
    {
        ulOID = pOID[i];

        ucOIDSize = 5;
        for (j=0; j<5; j++)
        {
            aucOID[j] = (unsigned char)(ulOID & 0x7F);
            ulOID = (ulOID >> 7);

            if (ulOID == 0)
            {
                ucOIDSize = (unsigned char)(j+1);
                break;
            }
        }

        //  0   1   2   3   4
        //  33  40  1   0   0
        // 28723 = (0x01*0x80 + 0x40) * 0x80 + 0x33 (81 e0 33)
        for (j=1; j<(unsigned int)ucOIDSize; j++)
        {
            *pucVar++ = (unsigned char)(aucOID[ucOIDSize-j] + 0x80);
        }

        *pucVar++ = aucOID[0];
    }

    *pucVarLen = (unsigned char)(pucVar - pucVarData);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_parse_oid_var
* 功    能: 根据BER格式的OID生成标准OID
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_parse_oid_var(unsigned char *pucVarData, unsigned char ucVarLen,
                       SNMP_OID *pOID, unsigned int *pulOIDSize)
{
    unsigned int    i;
    unsigned int    ulOID;
    unsigned int    ulOIDSize = 0;
    int             iVarLen = 0;
    unsigned char   *pucVar = pucVarData;

    *pulOIDSize = 0;

    //第1、2个字节合并为40*x + y ; 需要考虑超过128，目前固定为 43 = 40*1+3
    pOID[0] = pucVar[0] / 40;
    pOID[1] = pucVar[0] % 40;

    ulOIDSize = 2;
    iVarLen = 0;

    while(1)
    {
        /* 获取一个OID */
        ulOID = 0;
        for (i=0; ;i++)
        {
            iVarLen ++;
            if (iVarLen >= ucVarLen)
            {
                return -1;
            }

            if (pucVar[iVarLen] < 0x80)
            {
                ulOID = (ulOID << 7) + pucVar[iVarLen];
                break;
            }

            ulOID = (ulOID << 7) + pucVar[iVarLen] - 0x80;
        }

        pOID[ulOIDSize++] = ulOID;

        if ((iVarLen+1) == ucVarLen)
        {
            break;
        }
    }

    *pulOIDSize = ulOIDSize;

    return 0;
}

/*************************************************************************
* 函数名称: snmp_get_req_id
* 功    能: 获取snmp PDU报文中的req id
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
static unsigned short    g_usSnmpReqID = 0x100;

unsigned short snmp_get_last_req_id(void)
{
    return g_usSnmpReqID;
}

unsigned short snmp_get_req_id(void)
{
    g_usSnmpReqID++;

    if (g_usSnmpReqID < 0x100)
    {
        g_usSnmpReqID = 0x100;
    }

    return g_usSnmpReqID;
}

unsigned char snmp_set_req_id(unsigned short usReqIDValue, unsigned char *pucReqID)
{
    /*
    if (usReqIDValue <= 0xff)
    {
        pucReqID[0] = (unsigned char)usReqIDValue;
        *pulLen = 1;
    }
    else
    {
        pucReqID[0] = (unsigned char)(usReqIDValue >> 8);
        pucReqID[1] = (unsigned char)usReqIDValue;
        *pulLen = 2;
    }*/

    pucReqID[0] = (unsigned char)(usReqIDValue >> 8);
    pucReqID[1] = (unsigned char)usReqIDValue;

    return 2;
}

int snmp_set_req_id_ex(unsigned int ulReqIDValue, unsigned char *pucReqID)
{
    if (ulReqIDValue <= 0xff)
    {
        pucReqID[0] = (unsigned char)ulReqIDValue;
        return 1;
    }
    else if (ulReqIDValue <= 0xffffff)
    {
        pucReqID[0] = (unsigned char)(ulReqIDValue >> 8);
        pucReqID[1] = (unsigned char)ulReqIDValue;
        return 2;
    }
    else if (ulReqIDValue <= 0xffffff)
    {
        pucReqID[0] = (unsigned char)(ulReqIDValue >> 16);
        pucReqID[1] = (unsigned char)(ulReqIDValue >> 8);
        pucReqID[2] = (unsigned char)(ulReqIDValue);

        return 3;
    }
    else
    {
        pucReqID[0] = (unsigned char)(ulReqIDValue >> 24);
        pucReqID[1] = (unsigned char)(ulReqIDValue >> 16);
        pucReqID[2] = (unsigned char)(ulReqIDValue >> 8);
        pucReqID[3] = (unsigned char)(ulReqIDValue);

        return 4;
    }
}

/*************************************************************************
* 函数名称: snmp_get_int_value
* 功    能: 根据报文中整数参数转换成整数
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
unsigned int snmp_get_int_value(unsigned char *pucValue, unsigned char ucSize)
{
    unsigned int ulValue = 0;
    int i;

    for (i=0; i<ucSize; i++)
    {
        ulValue = (ulValue << 8) + pucValue[i];
    }

    return ulValue;
}

/*************************************************************************
* 函数名称: snmp_set_len_var
* 功    能: 生成报文中的整数字段
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_set_len_var(SNMP_TLV_T *pVar)
{
    unsigned char   aucLen[4];

    if (pVar->ulLen <= 0x7f)
    {
        pVar->ucLenNum = 1;
        pVar->aucLen[0] = pVar->ulLen;
    }
    else
    {
        pVar->ucLenNum = 4;
        *((int*)aucLen) = htonl(pVar->ulLen);
        if (aucLen[0] == 0)
        {
            pVar->ucLenNum --;

            if (aucLen[1] == 0)
            {
                pVar->ucLenNum --;
                if (aucLen[2] == 0)
                {
                    pVar->ucLenNum --;
                }
            }
        }

        memcpy(&pVar->aucLen[1], &aucLen[4-pVar->ucLenNum], pVar->ucLenNum);
        pVar->aucLen[0] = 0x80 + pVar->ucLenNum;
        pVar->ucLenNum++;
    }

    return 0;
}

/*************************************************************************
* 函数名称: snmp_init_var
* 功    能: 生成tlv报文
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_init_var(SNMP_VAR_T *pstVar, SNMP_OID *pOID, unsigned int ulOIDSize,
                  unsigned char ucReqDataType, unsigned char *pucReqData,
                  unsigned int ulReqDataLen)
{
    SNMP_TLV_T          *pstTLV;
    unsigned int        ulTmp;
    unsigned char       ucOIDLen = 0;
    unsigned char       aucOID[64];

    /* value */
    pstTLV = &pstVar->stValue;
    pstTLV->ucType = ucReqDataType;
    pstTLV->ulLen  = ulReqDataLen;
    pstTLV->ucNeedFree = 0;
    if (ucReqDataType == ASN_GAUGE      ||
        ucReqDataType == ASN_TIMETICKS  ||
        ucReqDataType == ASN_COUNTER    )
    {
        if( ulReqDataLen != 4)
        {
            return -1;
        }

        DATA_DUP(pstTLV, pucReqData, 4);
        ulTmp = *((unsigned int*)pstTLV->pucValue);
        if (ulTmp <= 0xff)
        {
            pstTLV->pucValue[0] = (unsigned char)ulTmp;
            pstTLV->ulLen = 1;
        }
        else if (ulTmp <= 0xffff)
        {
            pstTLV->pucValue[0] = (unsigned char)(ulTmp>>8);
            pstTLV->pucValue[1] = (unsigned char)(ulTmp);
            pstTLV->ulLen = 2;
        }
        else if (ulTmp <= 0xffffff)
        {
            pstTLV->pucValue[0] = (unsigned char)(ulTmp>>16);
            pstTLV->pucValue[1] = (unsigned char)(ulTmp>>8);
            pstTLV->pucValue[2] = (unsigned char)(ulTmp);
            pstTLV->ulLen = 3;
        }
        else
        {
            pstTLV->pucValue[0] = (unsigned char)(ulTmp>>24);
            pstTLV->pucValue[1] = (unsigned char)(ulTmp>>16);
            pstTLV->pucValue[2] = (unsigned char)(ulTmp>>8);
            pstTLV->pucValue[3] = (unsigned char)(ulTmp);
            pstTLV->ulLen = 4;
        }
    }
    else if (ucReqDataType == ASN_INTEGER)
    {
        if( ulReqDataLen != 4)
        {
            return -1;
        }

        DATA_DUP(pstTLV, pucReqData, 4);
        ulTmp = *((unsigned int*)pstTLV->pucValue);
        if (ulTmp <= 0x7f)
        {
            pstTLV->pucValue[0] = (unsigned char)ulTmp;
            pstTLV->ulLen = 1;
        }
        else if (ulTmp <= 0x7fff)
        {
            pstTLV->pucValue[0] = (unsigned char)(ulTmp>>8);
            pstTLV->pucValue[1] = (unsigned char)(ulTmp);
            pstTLV->ulLen = 2;
        }
        else if (ulTmp <= 0x7fffff)
        {
            pstTLV->pucValue[0] = (unsigned char)(ulTmp>>16);
            pstTLV->pucValue[1] = (unsigned char)(ulTmp>>8);
            pstTLV->pucValue[2] = (unsigned char)(ulTmp);
            pstTLV->ulLen = 3;
        }
        else
        {
            pstTLV->pucValue[0] = (unsigned char)(ulTmp>>24);
            pstTLV->pucValue[1] = (unsigned char)(ulTmp>>16);
            pstTLV->pucValue[2] = (unsigned char)(ulTmp>>8);
            pstTLV->pucValue[3] = (unsigned char)(ulTmp);
            pstTLV->ulLen = 4;
        }
    }
    else if (ucReqDataType == ASN_OBJECT_ID)
    {
        if (snmp_create_oid_var((SNMP_OID*)pucReqData, ulReqDataLen/sizeof(SNMP_OID), &aucOID[0], &ucOIDLen) < 0)
        {
            snmp_log(LOG_ERR, "snmp_init_var: create oid var failed");
            return -1;
        }
        pstTLV->ulLen = ucOIDLen;
        DATA_DUP(pstTLV, aucOID, ucOIDLen);
    }
    else
    {
        DATA_DUP(pstTLV, pucReqData, ulReqDataLen);
    }

    SET_LEN_VAR(pstTLV);

    /* oid */
    pstTLV = &pstVar->stOID;
    pstTLV->ucType = ASN_OBJECT_ID;
    if (snmp_create_oid_var(pOID, ulOIDSize, &aucOID[0], &ucOIDLen) < 0)
    {
        snmp_log(LOG_ERR, "snmp_init_var: create oid var failed");
        return -1;
    }
    pstTLV->ulLen = ucOIDLen;
    DATA_DUP(pstTLV, aucOID, ucOIDLen);
    SET_LEN_VAR(pstTLV);

    /* para */
    pstTLV = &pstVar->stPara;
    pstTLV->ucType = SNMP_ID_SEQUENCE;
    pstTLV->ulLen = TLV_LEN(&pstVar->stOID) +
                    TLV_LEN(&pstVar->stValue);
    SET_LEN_VAR(pstTLV);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_init_get_info
* 功    能: 生成get操作的PDU
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_init_get_info(char *szCommunity, unsigned char ucGetVer, SNMP_OID *pOID, unsigned int ulOIDSize,
                       SNMP_GET_PDU_T *pstPDU)
{
    unsigned short      usReqIDValue;

    /* oid */
    pstPDU->ucOIDType = ASN_OBJECT_ID;
    if (snmp_create_oid_var(pOID, ulOIDSize, &pstPDU->aucOID[0], &pstPDU->ucOIDSize) < 0)
    {
        return -1;
    }
    pstPDU->ucOIDValue = ASN_NULL;

    pstPDU->ucPara1Len = (1+1+pstPDU->ucOIDSize) +
                         (1+1);   //null & endmark
    pstPDU->ucPara1Type = SNMP_ID_SEQUENCE;

    pstPDU->ucParasLen = (1+1+pstPDU->ucPara1Len);
    pstPDU->ucParasType = SNMP_ID_SEQUENCE;

    /* resv */
    pstPDU->ucResv1Type = ASN_INTEGER;
    pstPDU->ucResv1Len  = 1;
    pstPDU->ucResv1Value= 0;

    pstPDU->ucResv2Type = ASN_INTEGER;
    pstPDU->ucResv2Len  = 1;
    pstPDU->ucResv2Value= 0;

    /* req id */
    pstPDU->ucReqIDType = ASN_INTEGER;

    usReqIDValue = snmp_get_req_id();
    pstPDU->ucReqIDLen = snmp_set_req_id(usReqIDValue, pstPDU->aucReqIDValue);
    /*
    if (usReqIDValue <= 0xff)
    {
        pstPDU->aucReqIDValue[0] = (unsigned char)usReqIDValue;

        pstPDU->ucReqIDLen = 1;
    }
    else
    {
        pstPDU->aucReqIDValue[0] = (unsigned char)(usReqIDValue >> 8);
        pstPDU->aucReqIDValue[1] = (unsigned char)usReqIDValue;

        pstPDU->ucReqIDLen = 2;
    }*/
    pstPDU->usReqID = usReqIDValue;

    pstPDU->ucSnmpPDULen = (1+1+pstPDU->ucReqIDLen) + // req id
                           (1+1+1) +  //resv1
                           (1+1+1) +  //resv2
                           (1+1+pstPDU->ucParasLen);
    pstPDU->ucSnmpPDUType = SNMP_MSG_GET;

    //community
    pstPDU->ucCommunityType = ASN_OCTET_STR;
    pstPDU->ucCommunityLen = strlen(szCommunity);
    if (pstPDU->ucCommunityLen > sizeof(pstPDU->aucCommunity))
    {
        return -1;
    }
    memcpy(pstPDU->aucCommunity, szCommunity, pstPDU->ucCommunityLen);

    //ver
    pstPDU->ucVerType = ASN_INTEGER;
    pstPDU->ucVerLen  = 1;
    pstPDU->ucVerValue= ucGetVer;

    pstPDU->ucPDUType = SNMP_ID_SEQUENCE;
    pstPDU->ucPDULen  = (1+1+1) + //ver
                        (1+1+pstPDU->ucCommunityLen) +
                        (1+1+pstPDU->ucSnmpPDULen);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_init_get_pdu
* 功    能: 生成get操作报文
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_init_get_pdu(char *szCommunity, unsigned char ucGetVer, SNMP_OID *pOID, unsigned int ulOIDSize,
                      unsigned char ucGetType, SNMP_GET_SCALAR_PDU_T *pstPDU)
{
    SNMP_TLV_T          *pstVar;
    unsigned short      usReqIDValue;
    unsigned char       ucOIDLen = 0;
    unsigned char       aucOID[64];
    unsigned char       aucReqID[2];
    unsigned char       ucErrorStatus = 0;
    unsigned char       ucErrorIndex  = 0;
    unsigned char       ucVersion     = ucGetVer;

    /* end mark */
    pstPDU->ucEndMark = 0;

    /* oid value */
    pstPDU->ucOIDValue = ASN_NULL;

    /* oid */
    pstVar = &pstPDU->stOID;
    pstVar->ucType = ASN_OBJECT_ID;
    if (snmp_create_oid_var(pOID, ulOIDSize, &aucOID[0], &ucOIDLen) < 0)
    {
        snmp_log(LOG_ERR, "snmp_init_set_pdu: create oid var failed");
        return -1;
    }
    pstVar->ulLen = ucOIDLen;
    DATA_DUP(pstVar, aucOID, ucOIDLen);
    SET_LEN_VAR(pstVar);

    /* para1 */
    pstVar = &pstPDU->stPara1;
    pstVar->ucType = SNMP_ID_SEQUENCE;
    pstVar->ulLen = TLV_LEN(&pstPDU->stOID) +
                    sizeof(pstPDU->ucOIDValue)+
                    sizeof(pstPDU->ucEndMark);
    SET_LEN_VAR(pstVar);

    /* paras */
    pstVar = &pstPDU->stParas;
    pstVar->ucType = SNMP_ID_SEQUENCE;
    pstVar->ulLen = TLV_LEN(&pstPDU->stPara1);
    SET_LEN_VAR(pstVar);

    /* error index */
    pstVar = &pstPDU->stErrIndex;
    pstVar->ucType = ASN_INTEGER;
    pstVar->ulLen = 1;
    DATA_DUP(pstVar, &ucErrorIndex, sizeof(ucErrorIndex));
    SET_LEN_VAR(pstVar);

    /* error status */
    pstVar = &pstPDU->stErrStatus;
    pstVar->ucType = ASN_INTEGER;
    pstVar->ulLen = 1;
    DATA_DUP(pstVar, &ucErrorStatus, sizeof(ucErrorStatus));
    SET_LEN_VAR(pstVar);

    /* req id */
    pstVar = &pstPDU->stReqID;
    pstVar->ucType = ASN_INTEGER;
    usReqIDValue = snmp_get_req_id();
    pstVar->ulLen = snmp_set_req_id(usReqIDValue, aucReqID);
    /*if (usReqIDValue <= 0xff)
    {
        aucReqID[0] = (unsigned char)usReqIDValue;
        pstVar->ulLen = 1;
    }
    else
    {
        aucReqID[0] = (unsigned char)(usReqIDValue >> 8);
        aucReqID[1] = (unsigned char)usReqIDValue;
        pstVar->ulLen = 2;
    }*/
    DATA_DUP(pstVar, aucReqID, pstVar->ulLen);
    SET_LEN_VAR(pstVar);

    /* snmp pdu */
    pstVar = &pstPDU->stSnmpPDU;
    pstVar->ucType = ucGetType; //SNMP_MSG_GET;
    pstVar->ulLen = TLV_LEN(&pstPDU->stReqID) +
                    TLV_LEN(&pstPDU->stErrStatus) +
                    TLV_LEN(&pstPDU->stErrIndex) +
                    TLV_LEN(&pstPDU->stParas);
    SET_LEN_VAR(pstVar);

    //community
    pstVar = &pstPDU->stCommunity;
    pstVar->ucType = ASN_OCTET_STR;
    pstVar->ulLen = strlen(szCommunity);
    if (pstVar->ulLen > SNMP_MAX_COMMUNITY_LEN)
    {
        return -1;
    }
    pstVar->pucValue = (unsigned char*)szCommunity;
    pstVar->ucNeedFree = 0;
    SET_LEN_VAR(pstVar);

    //version
    pstVar = &pstPDU->stVer;
    pstVar->ucType = ASN_INTEGER;
    pstVar->ulLen  = 1;
    DATA_DUP(pstVar, &ucVersion, sizeof(ucVersion));
    SET_LEN_VAR(pstVar);

    //PDU
    pstVar = &pstPDU->stPDU;
    pstVar->ucType = SNMP_ID_SEQUENCE;
    pstVar->ulLen  = TLV_LEN(&pstPDU->stVer) +
                     TLV_LEN(&pstPDU->stCommunity) +
                     TLV_LEN(&pstPDU->stSnmpPDU);
    SET_LEN_VAR(pstVar);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_init_get_rsp_pdu
* 功    能: 生成get rsp操作报文
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_init_get_rsp_pdu(char *szCommunity, unsigned char ucGetVer, SNMP_OID *pOID, unsigned int ulOIDSize,
                        unsigned int ulReqSeq, unsigned char *pucRspData, unsigned char ucRspDataType,
                        unsigned int ulRspDataLen, SNMP_GET_SCALAR_RSP_PDU_T *pstPDU)
{
    SNMP_TLV_T          *pstVar;
    unsigned char       ucOIDLen = 0;
    unsigned char       aucOID[64];
    unsigned char       aucReqID[4];
    unsigned char       ucErrorStatus = 0;
    unsigned char       ucErrorIndex  = 0;
    unsigned char       ucVersion     = ucGetVer;
    unsigned int        ulTmp;

    /* value */
    pstVar = &pstPDU->stValue;
    pstVar->ucType = ucRspDataType;
    pstVar->pucValue = pucRspData;
    pstVar->ulLen  = ulRspDataLen;
    pstVar->ucNeedFree = 0;

    if (ucRspDataType == ASN_GAUGE     ||
        ucRspDataType == ASN_TIMETICKS ||
        ucRspDataType == ASN_COUNTER   )
    {
        // modify
        //DATA_DUP(pstVar, pucReqData, ulReqDataLen);

        #if 1
        if( ulRspDataLen != 4)
        {
            return -1;
        }

        DATA_DUP(pstVar, pucRspData, 4);
        ulTmp = *((unsigned int*)pstVar->pucValue);
        if (ulTmp <= 0xff)
        {
            pstVar->pucValue[0] = (unsigned char)ulTmp;
            pstVar->ulLen = 1;
        }
        else if (ulTmp <= 0xffff)
        {
            pstVar->pucValue[0] = (unsigned char)(ulTmp>>8);
            pstVar->pucValue[1] = (unsigned char)(ulTmp);
            pstVar->ulLen = 2;
        }
        else if (ulTmp <= 0xffffff)
        {
            pstVar->pucValue[0] = (unsigned char)(ulTmp>>16);
            pstVar->pucValue[1] = (unsigned char)(ulTmp>>8);
            pstVar->pucValue[2] = (unsigned char)(ulTmp);
            pstVar->ulLen = 3;
        }
        else
        {
            pstVar->pucValue[0] = (unsigned char)(ulTmp>>24);
            pstVar->pucValue[1] = (unsigned char)(ulTmp>>16);
            pstVar->pucValue[2] = (unsigned char)(ulTmp>>8);
            pstVar->pucValue[3] = (unsigned char)(ulTmp);
            pstVar->ulLen = 4;
        }
        #endif
    }
    else if (ucRspDataType == ASN_INTEGER)
    {
        if( ulRspDataLen != 4)
        {
            return -1;
        }

        DATA_DUP(pstVar, pucRspData, 4);
        ulTmp = *((unsigned int*)pstVar->pucValue);
        if (ulTmp <= 0x7f)
        {
            pstVar->pucValue[0] = (unsigned char)ulTmp;
            pstVar->ulLen = 1;
        }
        else if (ulTmp <= 0x7fff)
        {
            pstVar->pucValue[0] = (unsigned char)(ulTmp>>8);
            pstVar->pucValue[1] = (unsigned char)(ulTmp);
            pstVar->ulLen = 2;
        }
        else if (ulTmp <= 0x7fffff)
        {
            pstVar->pucValue[0] = (unsigned char)(ulTmp>>16);
            pstVar->pucValue[1] = (unsigned char)(ulTmp>>8);
            pstVar->pucValue[2] = (unsigned char)(ulTmp);
            pstVar->ulLen = 3;
        }
        else
        {
            pstVar->pucValue[0] = (unsigned char)(ulTmp>>24);
            pstVar->pucValue[1] = (unsigned char)(ulTmp>>16);
            pstVar->pucValue[2] = (unsigned char)(ulTmp>>8);
            pstVar->pucValue[3] = (unsigned char)(ulTmp);
            pstVar->ulLen = 4;
        }
    }
    SET_LEN_VAR(pstVar);

    /* oid */
    pstVar = &pstPDU->stOID;
    pstVar->ucType = ASN_OBJECT_ID;
    if (snmp_create_oid_var(pOID, ulOIDSize, &aucOID[0], &ucOIDLen) < 0)
    {
        snmp_log(LOG_ERR, "snmp_init_get_rsp_pdu: create oid var failed");
        return -1;
    }
    pstVar->ulLen = ucOIDLen;
    DATA_DUP(pstVar, aucOID, ucOIDLen);
    SET_LEN_VAR(pstVar);

    /* para1 */
    pstVar = &pstPDU->stPara1;
    pstVar->ucType = SNMP_ID_SEQUENCE;
    pstVar->ulLen = TLV_LEN(&pstPDU->stOID) +
                    TLV_LEN(&pstPDU->stValue);
    SET_LEN_VAR(pstVar);

    /* paras */
    pstVar = &pstPDU->stParas;
    pstVar->ucType = SNMP_ID_SEQUENCE;
    pstVar->ulLen = TLV_LEN(&pstPDU->stPara1);
    SET_LEN_VAR(pstVar);

    /* error index */
    pstVar = &pstPDU->stErrIndex;
    pstVar->ucType = ASN_INTEGER;
    pstVar->ulLen = 1;
    DATA_DUP(pstVar, &ucErrorIndex, sizeof(ucErrorIndex));
    SET_LEN_VAR(pstVar);

    /* error status */
    pstVar = &pstPDU->stErrStatus;
    pstVar->ucType = ASN_INTEGER;
    pstVar->ulLen = 1;
    DATA_DUP(pstVar, &ucErrorStatus, sizeof(ucErrorStatus));
    SET_LEN_VAR(pstVar);

    /* req id */
    pstVar = &pstPDU->stReqID;
    pstVar->ucType = ASN_INTEGER;
    pstVar->ulLen = snmp_set_req_id_ex(ulReqSeq, aucReqID);
    DATA_DUP(pstVar, aucReqID, pstVar->ulLen);
    SET_LEN_VAR(pstVar);

    /* snmp pdu */
    pstVar = &pstPDU->stSnmpPDU;
    pstVar->ucType = SNMP_MSG_RESPONSE;
    pstVar->ulLen = TLV_LEN(&pstPDU->stReqID) +
                    TLV_LEN(&pstPDU->stErrStatus) +
                    TLV_LEN(&pstPDU->stErrIndex) +
                    TLV_LEN(&pstPDU->stParas);
    SET_LEN_VAR(pstVar);

    //community
    pstVar = &pstPDU->stCommunity;
    pstVar->ucType = ASN_OCTET_STR;
    pstVar->ulLen = strlen(szCommunity);
    if (pstVar->ulLen > SNMP_MAX_COMMUNITY_LEN)
    {
        return -1;
    }
    pstVar->pucValue = (unsigned char*)szCommunity;
    pstVar->ucNeedFree = 0;
    SET_LEN_VAR(pstVar);

    //version
    pstVar = &pstPDU->stVer;
    pstVar->ucType = ASN_INTEGER;
    pstVar->ulLen  = 1;
    DATA_DUP(pstVar, &ucVersion, sizeof(ucVersion));
    SET_LEN_VAR(pstVar);

    //PDU
    pstVar = &pstPDU->stPDU;
    pstVar->ucType = SNMP_ID_SEQUENCE;
    pstVar->ulLen  = TLV_LEN(&pstPDU->stVer) +
                     TLV_LEN(&pstPDU->stCommunity) +
                     TLV_LEN(&pstPDU->stSnmpPDU);
    SET_LEN_VAR(pstVar);

    return 0;
}


/*************************************************************************
* 函数名称: snmp_init_get_batch_pdu
* 功    能: 生成getbulk操作报文
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_init_get_batch_pdu(char *szCommunity, UINT32 ulOIDNum, SNMP_OID_T *pstOID, unsigned char ucGetType, UINT8 ucVersion, SNMP_GET_BATCH_PDU_T *pstPDU)
{
    SNMP_TLV_T          *pstVar;
    unsigned short      usReqIDValue;
    unsigned char       ucOIDLen = 0;
    unsigned char       aucOID[64];
    unsigned char       aucReqID[2];
    unsigned char       ucErrorStatus = 0;
    unsigned char       ucErrorIndex  = 0;
    UINT32              ulParaLen = 0;
    UINT32              i = 0;

    if (!pstOID || !pstPDU)
    {
        return -1;
    }

    /* end mark */
    pstPDU->ucEndMark = 0;

    /* oid value */
    pstPDU->ucOIDValue = ASN_NULL;


    pstPDU->ulOIDNum = ulOIDNum;

    if (ulOIDNum <= 0)
    {
        return -1;
    }

    pstPDU->pstOID = (SNMP_TLV_T*)snmp_calloc(ulOIDNum, sizeof(SNMP_TLV_T));
    if (NULL == pstPDU->pstOID)
    {
        snmp_log(LOG_ERR, "snmp_init_getbulk_pdu: snmp_calloc failed");
        return -1;
    }

    pstPDU->pstPara = (SNMP_TLV_T*)snmp_calloc(ulOIDNum, sizeof(SNMP_TLV_T));
    if (NULL == pstPDU->pstPara)
    {
        snmp_log(LOG_ERR, "snmp_init_getbulk_pdu: snmp_calloc failed");
        return -1;
    }

    for (i=0; i < ulOIDNum; i++)
    {
        pstVar = pstPDU->pstOID+i;
        pstVar->ucType = ASN_OBJECT_ID;
        if (snmp_create_oid_var(pstOID[i].astOID, pstOID[i].ulOIDSize, &aucOID[0], &ucOIDLen) < 0)
        {
            snmp_log(LOG_ERR, "snmp_init_getbulk_pdu: create oid var failed");
            return -1;
        }
        pstVar->ulLen = ucOIDLen;
        DATA_DUP(pstVar, aucOID, ucOIDLen);
        SET_LEN_VAR(pstVar);


        /* para */
        pstVar = &pstPDU->pstPara[i];
        pstVar->ucType = SNMP_ID_SEQUENCE;
        pstVar->ulLen = TLV_LEN(&pstPDU->pstOID[i]) + 2;
        SET_LEN_VAR(pstVar);

        ulParaLen += TLV_LEN(pstVar);
    }

    /* paras */
    pstVar = &pstPDU->stParas;
    pstVar->ucType = SNMP_ID_SEQUENCE;
    pstVar->ulLen = ulParaLen;
    SET_LEN_VAR(pstVar);

    /* error index */
    pstVar = &pstPDU->stErrIndex;
    pstVar->ucType = ASN_INTEGER;
    pstVar->ulLen = 1;
    DATA_DUP(pstVar, &ucErrorIndex, sizeof(ucErrorIndex));
    SET_LEN_VAR(pstVar);

    /* error status */
    pstVar = &pstPDU->stErrStatus;
    pstVar->ucType = ASN_INTEGER;
    pstVar->ulLen = 1;
    DATA_DUP(pstVar, &ucErrorStatus, sizeof(ucErrorStatus));
    SET_LEN_VAR(pstVar);

    /* req id */
    pstVar = &pstPDU->stReqID;
    pstVar->ucType = ASN_INTEGER;
    usReqIDValue = snmp_get_req_id();
    pstVar->ulLen = snmp_set_req_id(usReqIDValue, aucReqID);

    DATA_DUP(pstVar, aucReqID, pstVar->ulLen);
    SET_LEN_VAR(pstVar);

    /* snmp pdu */
    pstVar = &pstPDU->stSnmpPDU;
    pstVar->ucType = ucGetType; //SNMP_MSG_GET;
    pstVar->ulLen = TLV_LEN(&pstPDU->stReqID) +
                    TLV_LEN(&pstPDU->stErrStatus) +
                    TLV_LEN(&pstPDU->stErrIndex) +
                    TLV_LEN(&pstPDU->stParas);
    SET_LEN_VAR(pstVar);

    //community
    pstVar = &pstPDU->stCommunity;
    pstVar->ucType = ASN_OCTET_STR;
    pstVar->ulLen = strlen(szCommunity);
    if (pstVar->ulLen > SNMP_MAX_COMMUNITY_LEN)
    {
        return -1;
    }
    pstVar->pucValue = (unsigned char*)szCommunity;
    pstVar->ucNeedFree = 0;
    SET_LEN_VAR(pstVar);

    //version
    pstVar = &pstPDU->stVer;
    pstVar->ucType = ASN_INTEGER;
    pstVar->ulLen  = 1;
    DATA_DUP(pstVar, &ucVersion, sizeof(ucVersion));
    SET_LEN_VAR(pstVar);

    //PDU
    pstVar = &pstPDU->stPDU;
    pstVar->ucType = SNMP_ID_SEQUENCE;
    pstVar->ulLen  = TLV_LEN(&pstPDU->stVer) +
                     TLV_LEN(&pstPDU->stCommunity) +
                     TLV_LEN(&pstPDU->stSnmpPDU);
    SET_LEN_VAR(pstVar);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_create_getbulk_pdu
* 功    能: 生成getbulk操作报文
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_init_getbulk_pdu(char *szCommunity, SNMP_OID_T *pOID,  unsigned char ucGetType, SNMP_GET_BULK_PDU_T *pstPDU)
{
    SNMP_TLV_T          *pstVar;
    unsigned short      usReqIDValue;
    unsigned char       ucOIDLen = 0;
    unsigned char       aucOID[64];
    unsigned char       aucReqID[2];
    unsigned char       ucMaxRepetitions = 10;
    unsigned char       ucNoRepeaters  = 0;
    unsigned char       ucVersion      = SNMP_V2;

    /* end mark */
    pstPDU->ucEndMark = 0;

    /* oid value */
    pstPDU->ucOIDValue = ASN_NULL;

    /* oid */
    pstVar = &pstPDU->stOID;
    pstVar->ucType = ASN_OBJECT_ID;
    if (snmp_create_oid_var(pOID->astOID, pOID->ulOIDSize, &aucOID[0], &ucOIDLen) < 0)
    {
        snmp_log(LOG_ERR, "snmp_init_set_pdu: create oid var failed");
            return -1;
    }
    pstVar->ulLen = ucOIDLen;
    DATA_DUP(pstVar, aucOID, ucOIDLen);
    SET_LEN_VAR(pstVar);

    /* para1 */
    pstVar = &pstPDU->stPara1;
    pstVar->ucType = SNMP_ID_SEQUENCE;
    pstVar->ulLen = TLV_LEN(&pstPDU->stOID) +
                        sizeof(pstPDU->ucOIDValue)+
                        sizeof(pstPDU->ucEndMark);
    SET_LEN_VAR(pstVar);

    /* paras */
    pstVar = &pstPDU->stParas;
    pstVar->ucType = SNMP_ID_SEQUENCE;
    pstVar->ulLen = TLV_LEN(&pstPDU->stPara1);
    SET_LEN_VAR(pstVar);

    /* No  Repeaters */
    pstVar = &pstPDU->stNoRepeaters;
    pstVar->ucType = ASN_INTEGER;
    pstVar->ulLen = 1;
    DATA_DUP(pstVar, &ucNoRepeaters, sizeof(ucNoRepeaters));
    SET_LEN_VAR(pstVar);

    /* error status */
    pstVar = &pstPDU->stMaxRepetitions;
    pstVar->ucType = ASN_INTEGER;
    pstVar->ulLen = 1;
    DATA_DUP(pstVar, &ucMaxRepetitions, sizeof(ucMaxRepetitions));
    SET_LEN_VAR(pstVar);

    /* req id */
    pstVar = &pstPDU->stReqID;
    pstVar->ucType = ASN_INTEGER;
    usReqIDValue = snmp_get_req_id();
    pstVar->ulLen = snmp_set_req_id(usReqIDValue, aucReqID);
    /*if (usReqIDValue <= 0xff)
    {
        aucReqID[0] = (unsigned char)usReqIDValue;
        pstVar->ulLen = 1;
    }
    else
    {
        aucReqID[0] = (unsigned char)(usReqIDValue >> 8);
        aucReqID[1] = (unsigned char)usReqIDValue;
        pstVar->ulLen = 2;
    }*/
    DATA_DUP(pstVar, aucReqID, pstVar->ulLen);
    SET_LEN_VAR(pstVar);
    //pstPDU->usReqID = usReqIDValue;

    /* snmp pdu */
    pstVar = &pstPDU->stSnmpPDU;
    pstVar->ucType = ucGetType; //SNMP_MSG_GET;
    pstVar->ulLen = TLV_LEN(&pstPDU->stReqID) +
                    TLV_LEN(&pstPDU->stNoRepeaters) +
                    TLV_LEN(&pstPDU->stMaxRepetitions) +
                    TLV_LEN(&pstPDU->stParas);
    SET_LEN_VAR(pstVar);

    //community
    pstVar = &pstPDU->stCommunity;
    pstVar->ucType = ASN_OCTET_STR;
    pstVar->ulLen = strlen(szCommunity);
    if (pstVar->ulLen > SNMP_MAX_COMMUNITY_LEN)
    {
        return -1;
    }
    pstVar->pucValue = (unsigned char*)szCommunity;
    pstVar->ucNeedFree = 0;
    SET_LEN_VAR(pstVar);

    //version
    pstVar = &pstPDU->stVer;
    pstVar->ucType = ASN_INTEGER;
    pstVar->ulLen  = 1;
    DATA_DUP(pstVar, &ucVersion, sizeof(ucVersion));
    SET_LEN_VAR(pstVar);

    //PDU
    pstVar = &pstPDU->stPDU;
    pstVar->ucType = SNMP_ID_SEQUENCE;
    pstVar->ulLen  = TLV_LEN(&pstPDU->stVer) +
                     TLV_LEN(&pstPDU->stCommunity) +
                     TLV_LEN(&pstPDU->stSnmpPDU);
    SET_LEN_VAR(pstVar);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_create_get_pdu
* 功    能: 生成get操作报文
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_create_get_pdu(SNMP_GET_SCALAR_PDU_T *pstPDU, unsigned char *pucReqPDU,
                        unsigned int ulMaxReqPDULen, unsigned int *pulReqPDULen)
{
    unsigned char       *pucValue = pucReqPDU;
    unsigned int        ulCurrPDULen = 0;

    /* 根据结构生成PDU */
    INIT_VAR(&pstPDU->stPDU);
    INIT_VAR(&pstPDU->stVer);
    INIT_VAR(&pstPDU->stCommunity);
    INIT_VAR(&pstPDU->stSnmpPDU);
    INIT_VAR(&pstPDU->stReqID);
    INIT_VAR(&pstPDU->stErrStatus);
    INIT_VAR(&pstPDU->stErrIndex);
    INIT_VAR(&pstPDU->stParas);
    INIT_VAR(&pstPDU->stPara1);
    INIT_VAR(&pstPDU->stOID);

    *pucValue++ = pstPDU->ucOIDValue;
    *pucValue++ = pstPDU->ucEndMark;

    *pulReqPDULen = TLV_LEN(&pstPDU->stPDU);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_create_get_pdu
* 功    能: 生成get操作报文
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_create_get_batch_pdu(SNMP_GET_BATCH_PDU_T *pstPDU, unsigned char *pucReqPDU,
                              unsigned int ulMaxReqPDULen, unsigned int *pulReqPDULen)
{
    unsigned char       *pucValue = pucReqPDU;
    unsigned int        i;
    unsigned int        ulCurrPDULen = 0;

    /* 根据结构生成PDU */
    INIT_VAR(&pstPDU->stPDU);
    INIT_VAR(&pstPDU->stVer);
    INIT_VAR(&pstPDU->stCommunity);
    INIT_VAR(&pstPDU->stSnmpPDU);
    INIT_VAR(&pstPDU->stReqID);
    INIT_VAR(&pstPDU->stErrStatus);
    INIT_VAR(&pstPDU->stErrIndex);
    INIT_VAR(&pstPDU->stParas);

    for (i=0; i<pstPDU->ulOIDNum; i++)
    {
        INIT_VAR((pstPDU->pstPara+i));
        INIT_VAR((pstPDU->pstOID+i));

        *pucValue++ = pstPDU->ucOIDValue;
        *pucValue++ = pstPDU->ucEndMark;
    }

    *pulReqPDULen = TLV_LEN(&pstPDU->stPDU);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_create_get_pdu
* 功    能: 生成get操作报文
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_create_getbulk_pdu(SNMP_GET_BULK_PDU_T *pstPDU, unsigned char *pucReqPDU,
                            unsigned int ulMaxReqPDULen, unsigned int *pulReqPDULen)
{
    unsigned char       *pucValue = pucReqPDU;
    unsigned int        ulCurrPDULen = 0;

    /* 根据结构生成PDU */
    INIT_VAR(&pstPDU->stPDU);
    INIT_VAR(&pstPDU->stVer);
    INIT_VAR(&pstPDU->stCommunity);
    INIT_VAR(&pstPDU->stSnmpPDU);
    INIT_VAR(&pstPDU->stReqID);
    INIT_VAR(&pstPDU->stNoRepeaters);
    INIT_VAR(&pstPDU->stMaxRepetitions);
    INIT_VAR(&pstPDU->stParas);
    INIT_VAR(&pstPDU->stPara1);
    INIT_VAR(&pstPDU->stOID);

    *pucValue++ = pstPDU->ucOIDValue;
    *pucValue++ = pstPDU->ucEndMark;

    *pulReqPDULen = TLV_LEN(&pstPDU->stPDU);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_create_set_pdu
* 功    能: 生成set操作报文
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_init_set_pdu(char *szCommunity, unsigned char ucSetVer, SNMP_OID *pOID, unsigned int ulOIDSize,
                       unsigned char *pucReqData, unsigned char ucReqDataType,
                       unsigned int ulReqDataLen,SNMP_SET_PDU_T *pstPDU)
{
    SNMP_TLV_T          *pstVar;
    unsigned short      usReqIDValue;
    unsigned char       ucOIDLen = 0;
    unsigned char       aucOID[64];
    unsigned char       aucReqID[2];
    unsigned char       ucErrorStatus = 0;
    unsigned char       ucErrorIndex  = 0;
    unsigned char       ucVersion     = ucSetVer;
    unsigned int        ulTmp;

    /* value */
    pstVar = &pstPDU->stValue;
    pstVar->ucType = ucReqDataType;
    pstVar->pucValue = pucReqData;
    pstVar->ulLen  = ulReqDataLen;
    pstVar->ucNeedFree = 0;
    if (ucReqDataType == ASN_GAUGE     ||
        ucReqDataType == ASN_TIMETICKS ||
        ucReqDataType == ASN_COUNTER   )
    {
        // modify
        //DATA_DUP(pstVar, pucReqData, ulReqDataLen);

        #if 1
        if( ulReqDataLen != 4)
        {
            return -1;
        }

        DATA_DUP(pstVar, pucReqData, 4);
        ulTmp = *((unsigned int*)pstVar->pucValue);
        if (ulTmp <= 0xff)
        {
            pstVar->pucValue[0] = (unsigned char)ulTmp;
            pstVar->ulLen = 1;
        }
        else if (ulTmp <= 0xffff)
        {
            pstVar->pucValue[0] = (unsigned char)(ulTmp>>8);
            pstVar->pucValue[1] = (unsigned char)(ulTmp);
            pstVar->ulLen = 2;
        }
        else if (ulTmp <= 0xffffff)
        {
            pstVar->pucValue[0] = (unsigned char)(ulTmp>>16);
            pstVar->pucValue[1] = (unsigned char)(ulTmp>>8);
            pstVar->pucValue[2] = (unsigned char)(ulTmp);
            pstVar->ulLen = 3;
        }
        else
        {
            pstVar->pucValue[0] = (unsigned char)(ulTmp>>24);
            pstVar->pucValue[1] = (unsigned char)(ulTmp>>16);
            pstVar->pucValue[2] = (unsigned char)(ulTmp>>8);
            pstVar->pucValue[3] = (unsigned char)(ulTmp);
            pstVar->ulLen = 4;
        }
        #endif
    }
    else if (ucReqDataType == ASN_INTEGER)
    {
        if( ulReqDataLen != 4)
        {
            return -1;
        }

        DATA_DUP(pstVar, pucReqData, 4);
        ulTmp = *((unsigned int*)pstVar->pucValue);
        if (ulTmp <= 0x7f)
        {
            pstVar->pucValue[0] = (unsigned char)ulTmp;
            pstVar->ulLen = 1;
        }
        else if (ulTmp <= 0x7fff)
        {
            pstVar->pucValue[0] = (unsigned char)(ulTmp>>8);
            pstVar->pucValue[1] = (unsigned char)(ulTmp);
            pstVar->ulLen = 2;
        }
        else if (ulTmp <= 0x7fffff)
        {
            pstVar->pucValue[0] = (unsigned char)(ulTmp>>16);
            pstVar->pucValue[1] = (unsigned char)(ulTmp>>8);
            pstVar->pucValue[2] = (unsigned char)(ulTmp);
            pstVar->ulLen = 3;
        }
        else
        {
            pstVar->pucValue[0] = (unsigned char)(ulTmp>>24);
            pstVar->pucValue[1] = (unsigned char)(ulTmp>>16);
            pstVar->pucValue[2] = (unsigned char)(ulTmp>>8);
            pstVar->pucValue[3] = (unsigned char)(ulTmp);
            pstVar->ulLen = 4;
        }
    }
    SET_LEN_VAR(pstVar);

    /* oid */
    pstVar = &pstPDU->stOID;
    pstVar->ucType = ASN_OBJECT_ID;
    if (snmp_create_oid_var(pOID, ulOIDSize, &aucOID[0], &ucOIDLen) < 0)
    {
        snmp_log(LOG_ERR, "snmp_init_set_pdu: create oid var failed");
        return -1;
    }
    pstVar->ulLen = ucOIDLen;
    DATA_DUP(pstVar, aucOID, ucOIDLen);
    SET_LEN_VAR(pstVar);

    /* para1 */
    pstVar = &pstPDU->stPara1;
    pstVar->ucType = SNMP_ID_SEQUENCE;
    pstVar->ulLen = TLV_LEN(&pstPDU->stOID) +
                    TLV_LEN(&pstPDU->stValue);
    SET_LEN_VAR(pstVar);

    /* paras */
    pstVar = &pstPDU->stParas;
    pstVar->ucType = SNMP_ID_SEQUENCE;
    pstVar->ulLen = TLV_LEN(&pstPDU->stPara1);
    SET_LEN_VAR(pstVar);

    /* error index */
    pstVar = &pstPDU->stErrIndex;
    pstVar->ucType = ASN_INTEGER;
    pstVar->ulLen = 1;
    DATA_DUP(pstVar, &ucErrorIndex, sizeof(ucErrorIndex));
    SET_LEN_VAR(pstVar);

    /* error status */
    pstVar = &pstPDU->stErrStatus;
    pstVar->ucType = ASN_INTEGER;
    pstVar->ulLen = 1;
    DATA_DUP(pstVar, &ucErrorStatus, sizeof(ucErrorStatus));
    SET_LEN_VAR(pstVar);

    /* req id */
    pstVar = &pstPDU->stReqID;
    pstVar->ucType = ASN_INTEGER;
    usReqIDValue = snmp_get_req_id();
    pstVar->ulLen = snmp_set_req_id(usReqIDValue, aucReqID);
    /*if (usReqIDValue <= 0xff)
    {
        aucReqID[0] = (unsigned char)usReqIDValue;
        pstVar->ulLen = 1;
    }
    else
    {
        aucReqID[0] = (unsigned char)(usReqIDValue >> 8);
        aucReqID[1] = (unsigned char)usReqIDValue;
        pstVar->ulLen = 2;
    }*/
    DATA_DUP(pstVar, aucReqID, pstVar->ulLen);
    SET_LEN_VAR(pstVar);
    //pstPDU->usReqID = usReqIDValue;

    /* snmp pdu */
    pstVar = &pstPDU->stSnmpPDU;
    pstVar->ucType = SNMP_MSG_SET;
    pstVar->ulLen = TLV_LEN(&pstPDU->stReqID) +
                    TLV_LEN(&pstPDU->stErrStatus) +
                    TLV_LEN(&pstPDU->stErrIndex) +
                    TLV_LEN(&pstPDU->stParas);
    SET_LEN_VAR(pstVar);

    //community
    pstVar = &pstPDU->stCommunity;
    pstVar->ucType = ASN_OCTET_STR;
    pstVar->ulLen = strlen(szCommunity);
    if (pstVar->ulLen > SNMP_MAX_COMMUNITY_LEN)
    {
        return -1;
    }
    pstVar->pucValue = (unsigned char*)szCommunity;
    pstVar->ucNeedFree = 0;
    SET_LEN_VAR(pstVar);

    //version
    pstVar = &pstPDU->stVer;
    pstVar->ucType = ASN_INTEGER;
    pstVar->ulLen  = 1;
    DATA_DUP(pstVar, &ucVersion, sizeof(ucVersion));
    SET_LEN_VAR(pstVar);

    //PDU
    pstVar = &pstPDU->stPDU;
    pstVar->ucType = SNMP_ID_SEQUENCE;
    pstVar->ulLen  = TLV_LEN(&pstPDU->stVer) +
                     TLV_LEN(&pstPDU->stCommunity) +
                     TLV_LEN(&pstPDU->stSnmpPDU);
    SET_LEN_VAR(pstVar);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_create_set_pdu
* 功    能: 生成set操作报文
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_create_set_pdu(SNMP_SET_PDU_T *pstPDU, unsigned char *pucReqPDU,
                        unsigned int ulMaxReqPDULen, unsigned int *pulReqPDULen)
{
    unsigned char       *pucValue = pucReqPDU;
    unsigned int        ulCurrPDULen = 0;

    /* 根据结构生成PDU */
    INIT_VAR(&pstPDU->stPDU);
    INIT_VAR(&pstPDU->stVer);
    INIT_VAR(&pstPDU->stCommunity);
    INIT_VAR(&pstPDU->stSnmpPDU);
    INIT_VAR(&pstPDU->stReqID);
    INIT_VAR(&pstPDU->stErrStatus);
    INIT_VAR(&pstPDU->stErrIndex);
    INIT_VAR(&pstPDU->stParas);
    INIT_VAR(&pstPDU->stPara1);
    INIT_VAR(&pstPDU->stOID);
    INIT_VAR(&pstPDU->stValue);

    *pulReqPDULen = TLV_LEN(&pstPDU->stPDU);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_create_get_rsp_pdu
* 功    能: 生成get rsp操作报文
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_create_get_rsp_pdu(SNMP_GET_SCALAR_RSP_PDU_T *pstPDU, unsigned char *pucReqPDU,
                            unsigned int ulMaxReqPDULen, unsigned int *pulReqPDULen)
{
    unsigned char       *pucValue = pucReqPDU;
    unsigned int        ulCurrPDULen = 0;

    /* 根据结构生成PDU */
    INIT_VAR(&pstPDU->stPDU);
    INIT_VAR(&pstPDU->stVer);
    INIT_VAR(&pstPDU->stCommunity);
    INIT_VAR(&pstPDU->stSnmpPDU);
    INIT_VAR(&pstPDU->stReqID);
    INIT_VAR(&pstPDU->stErrStatus);
    INIT_VAR(&pstPDU->stErrIndex);
    INIT_VAR(&pstPDU->stParas);
    INIT_VAR(&pstPDU->stPara1);
    INIT_VAR(&pstPDU->stOID);
    INIT_VAR(&pstPDU->stValue);

    *pulReqPDULen = TLV_LEN(&pstPDU->stPDU);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_create_trap2_pdu
* 功    能: 生成trap2操作报文
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_create_trap2_pdu(SNMP_TRAP2_PDU_T *pstPDU, unsigned char *pucReqPDU,
                          unsigned int ulMaxReqPDULen, unsigned int *pulReqPDULen)
{
    unsigned char       *pucValue = pucReqPDU;
    unsigned int        ulCurrPDULen = 0;
    SNMP_VAR_T          *pstVar;

    /* 根据结构生成PDU */
    INIT_VAR(&pstPDU->stPDU);
    INIT_VAR(&pstPDU->stVer);
    INIT_VAR(&pstPDU->stCommunity);
    INIT_VAR(&pstPDU->stSnmpPDU);
    INIT_VAR(&pstPDU->stReqID);
    INIT_VAR(&pstPDU->stErrStatus);
    INIT_VAR(&pstPDU->stErrIndex);
    INIT_VAR(&pstPDU->stParas);

    INIT_VAR(&pstPDU->stTimestamp.stPara);
    INIT_VAR(&pstPDU->stTimestamp.stOID);
    INIT_VAR(&pstPDU->stTimestamp.stValue);

    for(pstVar=pstPDU->pstVarList; pstVar; pstVar=pstVar->pstNext)
    {
        INIT_VAR(&pstVar->stPara);
        INIT_VAR(&pstVar->stOID);
        INIT_VAR(&pstVar->stValue);
    }

    *pulReqPDULen = TLV_LEN(&pstPDU->stPDU);

    return 0;
}

/******************************************************************************
* 方法名称: snmp_get_uptime_10ms
* 功    能: 获取上电时间(单位: 10ms)
* 访问属性:
* 参    数:
* 参数名称            类型                  输入/输出        描述
* 函数返回:
* 说    明:
******************************************************************************/
unsigned int snmp_get_uptime_10ms(void)
{
#ifdef WIN32
    return GetTickCount()/10;
#else
    static int              iInit = 0;
    static struct timespec  stStartTime = {0};
    struct timespec         stCurrTime;
    int             sec;
    int             i10ms;      /* 毫秒(=1000微秒)*/
    int             nsec;       /* 纳秒*/

    clock_gettime(CLOCK_MONOTONIC, &stCurrTime);
    sec  = stCurrTime.tv_sec - stStartTime.tv_sec;
    nsec = stCurrTime.tv_nsec - stStartTime.tv_nsec;

    i10ms = sec * 100;
    i10ms += (nsec / 10000000);

    return (unsigned int)i10ms;
#endif
}

/*************************************************************************
* 函数名称: snmp_init_trap2_pdu
* 功    能: 生成trap2操作报文
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_init_trap2_pdu(char *szCommunity, unsigned int ulTrapType, SNMP_TRAP2_PDU_T *pstPDU)
{
    SNMP_TLV_T          *pstTLV;
    unsigned short      usReqIDValue;
    unsigned char       aucReqID[2];
    unsigned char       ucErrorStatus = 0;
    unsigned char       ucErrorIndex  = 0;
    unsigned char       ucVersion     = SNMP_TRAP_VER;
    SNMP_VAR_T          *pstVar;
    unsigned int        ulParaLen = 0;
    SNMP_OID            astTimestampOID[] = {SNMP_TIMESTAMP_OID};
    unsigned int        ulTimeStamp = snmp_get_uptime_10ms();

    for(pstVar=pstPDU->pstVarList; pstVar; pstVar=pstVar->pstNext)
    {
        ulParaLen += TLV_LEN(&pstVar->stPara);
    }

    if (ulParaLen == 0)
    {
        return -1;
    }

    if (snmp_init_var(&pstPDU->stTimestamp, astTimestampOID, sizeof(astTimestampOID)/sizeof(SNMP_OID),
                      ASN_TIMETICKS, (unsigned char*)&ulTimeStamp, sizeof(ulTimeStamp)) < 0)
    {
        snmp_log(LOG_ERR, "snmp_init_trap2_pdu: snmp_init_var of timestamp failed");
        return -1;
    }

    /* paras */
    pstTLV = &pstPDU->stParas;
    pstTLV->ucType = SNMP_ID_SEQUENCE;
    pstTLV->ulLen = ulParaLen + TLV_LEN(&pstPDU->stTimestamp.stPara);
    SET_LEN_VAR(pstTLV);

    /* error index */
    pstTLV = &pstPDU->stErrIndex;
    pstTLV->ucType = ASN_INTEGER;
    pstTLV->ulLen = 1;
    DATA_DUP(pstTLV, &ucErrorIndex, sizeof(ucErrorIndex));
    SET_LEN_VAR(pstTLV);

    /* error status */
    pstTLV = &pstPDU->stErrStatus;
    pstTLV->ucType = ASN_INTEGER;
    pstTLV->ulLen = 1;
    DATA_DUP(pstTLV, &ucErrorStatus, sizeof(ucErrorStatus));
    SET_LEN_VAR(pstTLV);

    /* req id */
    pstTLV = &pstPDU->stReqID;
    pstTLV->ucType = ASN_INTEGER;
    usReqIDValue = snmp_get_req_id();
    pstTLV->ulLen = snmp_set_req_id(usReqIDValue, aucReqID);
    /*
    if (usReqIDValue <= 0xff)
    {
        aucReqID[0] = (unsigned char)usReqIDValue;
        pstTLV->ulLen = 1;
    }
    else
    {
        aucReqID[0] = (unsigned char)(usReqIDValue >> 8);
        aucReqID[1] = (unsigned char)usReqIDValue;
        pstTLV->ulLen = 2;
    }*/
    DATA_DUP(pstTLV, aucReqID, pstTLV->ulLen);
    SET_LEN_VAR(pstTLV);
    //pstPDU->usReqID = usReqIDValue;

    /* snmp pdu */
    pstTLV = &pstPDU->stSnmpPDU;
    pstTLV->ucType = ulTrapType; //SNMP_MSG_TRAP2;//INFORM;
    pstTLV->ulLen = TLV_LEN(&pstPDU->stReqID) +
                    TLV_LEN(&pstPDU->stErrStatus) +
                    TLV_LEN(&pstPDU->stErrIndex) +
                    TLV_LEN(&pstPDU->stParas);
    SET_LEN_VAR(pstTLV);

    //community
    pstTLV = &pstPDU->stCommunity;
    pstTLV->ucType = ASN_OCTET_STR;
    pstTLV->ulLen = strlen(szCommunity);
    if (pstTLV->ulLen > SNMP_MAX_COMMUNITY_LEN)
    {
        return -1;
    }
    pstTLV->pucValue = (unsigned char*)szCommunity;
    pstTLV->ucNeedFree = 0;
    SET_LEN_VAR(pstTLV);

    //version
    pstTLV = &pstPDU->stVer;
    pstTLV->ucType = ASN_INTEGER;
    pstTLV->ulLen  = 1;
    DATA_DUP(pstTLV, &ucVersion, sizeof(ucVersion));
    SET_LEN_VAR(pstTLV);

    //PDU
    pstTLV = &pstPDU->stPDU;
    pstTLV->ucType = SNMP_ID_SEQUENCE;
    pstTLV->ulLen  = TLV_LEN(&pstPDU->stVer) +
                     TLV_LEN(&pstPDU->stCommunity) +
                     TLV_LEN(&pstPDU->stSnmpPDU);
    SET_LEN_VAR(pstTLV);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_free_varlist
* 功    能: 释放varlist
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
void snmp_free_varlist(SNMP_VAR_T *pstVarList)
{
    SNMP_VAR_T  *pstVar = pstVarList;
    SNMP_VAR_T  *pstNextVar;

    while(pstVar)
    {
        pstNextVar = pstVar->pstNext;

        snmp_free_tlv(&pstVar->stPara);
        snmp_free_tlv(&pstVar->stOID);
        snmp_free_tlv(&pstVar->stValue);
        snmp_free(pstVar);
        pstVar = NULL;

        pstVar = pstNextVar;
    }
}

/*************************************************************************
* 函数名称: snmp_free_trap2_pdu
* 功    能: 释放PDU中的动态申请的内存
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_free_trap2_pdu(SNMP_TRAP2_PDU_T *pstPDU)
{
    FREE_TLV(&pstPDU->stPDU);
    FREE_TLV(&pstPDU->stVer);
    FREE_TLV(&pstPDU->stCommunity);
    FREE_TLV(&pstPDU->stSnmpPDU);
    FREE_TLV(&pstPDU->stReqID);
    FREE_TLV(&pstPDU->stErrStatus);
    FREE_TLV(&pstPDU->stErrIndex);
    FREE_TLV(&pstPDU->stParas);

    FREE_TLV(&pstPDU->stTimestamp.stPara);
    FREE_TLV(&pstPDU->stTimestamp.stOID);
    FREE_TLV(&pstPDU->stTimestamp.stValue);

    snmp_free_varlist(pstPDU->pstVarList);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_free_get_rsp_pdu
* 功    能: 释放PDU中的动态申请的内存
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_free_get_rsp_pdu(SNMP_GET_RSP_PDU_T *pstPDU)
{
    FREE_TLV(&pstPDU->stPDU);
    FREE_TLV(&pstPDU->stVer);
    FREE_TLV(&pstPDU->stCommunity);
    FREE_TLV(&pstPDU->stSnmpPDU);
    FREE_TLV(&pstPDU->stReqID);
    FREE_TLV(&pstPDU->stErrStatus);
    FREE_TLV(&pstPDU->stErrIndex);
    FREE_TLV(&pstPDU->stParas);

    snmp_free_varlist(pstPDU->pstVarList);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_varlist_add_var
* 功    能: 生成trap2操作报文
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_varlist_add_var_ex(SNMP_VAR_T **pstVarList, SNMP_OID *pOID, unsigned int ulOIDSize, unsigned char bAppendZero,
                         unsigned char ucReqDataType, unsigned char *pucReqData, unsigned int ulReqDataLen)
{
    SNMP_VAR_T  *pstVar;
    SNMP_OID    astOID[64];

    if (ulOIDSize >= sizeof(astOID)/sizeof(astOID[0]))
    {
        snmp_log(LOG_ERR, "snmp_varlist_add_var: oid is too long");
        return -1;
    }

    memset(astOID, 0, sizeof(astOID));
    memcpy(astOID, pOID, ulOIDSize*sizeof(SNMP_OID));
    if (bAppendZero)
    {
        ulOIDSize++;
    }

    if (pstVarList == NULL)
    {
        snmp_log(LOG_ERR, "snmp_varlist_add_var: pstVarList is null");
        return -1;
    }

    if (*pstVarList == NULL)
    {
        *pstVarList = (SNMP_VAR_T*)snmp_malloc(sizeof(SNMP_VAR_T));
        if ((*pstVarList) == NULL)
        {
            snmp_log(LOG_ERR, "snmp_varlist_add_var: alloc pstVarList failed");
            goto fail;
        }

        pstVar = *pstVarList;
        memset(pstVar, 0, sizeof(SNMP_VAR_T));
    }
    else
    {
        for (pstVar=*pstVarList; pstVar->pstNext; pstVar=pstVar->pstNext);

        pstVar->pstNext = (SNMP_VAR_T*)snmp_malloc(sizeof(SNMP_VAR_T));
        if (pstVar->pstNext == NULL)
        {
            snmp_log(LOG_ERR, "snmp_varlist_add_var: alloc pstVar failed");
            goto fail;
        }

        pstVar = pstVar->pstNext;
        memset(pstVar, 0, sizeof(SNMP_VAR_T));
    }

    if (snmp_init_var(pstVar, astOID, ulOIDSize, ucReqDataType, pucReqData, ulReqDataLen) < 0)
    {
        goto fail;
    }

    return 0;

fail:
    snmp_free_varlist(*pstVarList);

    return -1;
}

int snmp_varlist_add_var(SNMP_VAR_T **pstVarList, SNMP_OID *pOID, unsigned int ulOIDSize,
                         unsigned char ucReqDataType, unsigned char *pucReqData,
                         unsigned int ulReqDataLen)
{
    return snmp_varlist_add_var_ex(pstVarList, pOID, ulOIDSize, 0, ucReqDataType, pucReqData, ulReqDataLen);

}

int _snmp_parse_get_scalar_rsp(SNMP_GET_SCALAR_PDU_T *pstPDU, unsigned char ucGetVer, unsigned char *pucRspData,
                              unsigned int ulRspDataLen, unsigned char *pucValue,
                              unsigned int ulMaxValueLen, unsigned int *pulValueLen)
{
    unsigned int    ulPDULen;
    unsigned int    ulDataLen;
    unsigned char   ucOIDSize;
    unsigned char   ucDataType;
    unsigned char   ucTmp;
    unsigned char   *pucRsp = pucRspData;

    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        ulPDULen = snmp_get_int_value(pucRspData, ucTmp);
        pucRspData += ucTmp;

        if (ulRspDataLen != (1+1+ucTmp+ulPDULen))
        {
            return -1;
        }
    }
    else
    {
        ulPDULen = pucRspData[0];
        pucRspData ++;

        if (ulRspDataLen != (1+1+ulPDULen))
        {
            return -1;
        }
    }

    //ver check
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1 ||
        pucRspData[2] != ucGetVer)
    {
        return -1;
    }
    pucRspData += (1+1+1);

    //community
    if (pucRspData[0] != ASN_OCTET_STR ||
        pucRspData[1] == 0)
    {
        return -1;
    }
    pucRspData += (1+1+pucRspData[1]);

    //snmp pdu type : get response
    if ((*pucRspData++) != SNMP_MSG_RESPONSE)
    {
        return -1;
    }

    //snmp pdu len
    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //req id
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] == 0)
    {
        snmp_log(LOG_ERR, "snmp_parse_get_scalra_rsp: pucRspData[0]:%u,pucRspData[1]:%u.",
            pucRspData[0],
            pucRspData[1]);
        snmp_log(LOG_ERR, "snmp_parse_get_scalra_rsp: check reqid failed");
        return -1;
    }

    ucTmp = pucRspData[1];
    pucRspData += 2;

    CHECK_REQ_ID(pstPDU->stReqID, pucRspData)
    {
        snmp_log(LOG_ERR, "snmp_parse_get_scalra_rsp: pucRspData:%02x %02x,pstPDU->stReqID.pucValue:%02x %02x.",
            pucRspData[0], pucRspData[1],
            pstPDU->stReqID.pucValue[0],
            pstPDU->stReqID.pucValue[1]);

        // add print
        nms_print_binary(pucRsp, ulRspDataLen);
        //nms_print_table_pdu(pstPDU);

        snmp_log(LOG_ERR, "snmp_parse_get_scalra_rsp: check reqid failed");
        return -1;
    }

    pucRspData += ucTmp;

    //error status
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1)
    {
        return -1;
    }

    pucRspData += 2;

    if ((*pucRspData++) != SNMP_ERR_NOERROR)
    {
        return -2;
    }

    //error index
    pucRspData += (1+1+1);

    //paras
    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //para1
    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //oid type
    if (pucRspData[0] != ASN_OBJECT_ID ||
        pucRspData[1] == 0)
    {
        return -1;
    }

    ucOIDSize = pucRspData[1];
    pucRspData += 2;

    //check oid
    pucRspData += ucOIDSize;

    //octet string type
    ucDataType = *pucRspData++;

    //octet string len
    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        ulDataLen = snmp_get_int_value(pucRspData, ucTmp);
        pucRspData += ucTmp;
    }
    else
    {
        ulDataLen = pucRspData[0];
        pucRspData ++;
    }

    //check len
    if (ulDataLen > ulMaxValueLen)
    {
        if (ucDataType == ASN_GAUGE && ulDataLen == (ulMaxValueLen+1))
        {
            pucRspData ++;
            ulDataLen --;
        }
        else if (ucDataType == ASN_OCTET_STR)
        {
            ulDataLen = strlen((char*)pucRspData)+1;
            if (ulDataLen > ulMaxValueLen)
            {
                ulDataLen = ulMaxValueLen;
            }
        }
        else
        {
            return -1;
        }
    }

    *pulValueLen = ulDataLen;
    nms_get_data(ucDataType, ulDataLen, pucRspData, pucValue);

    return 0;
}

int snmp_parse_get_scalar_req(unsigned char ucGetVer, unsigned char *pucRspData, unsigned int ulRspDataLen,
                       unsigned int *pulReqSeq, SNMP_OID *pOID, unsigned int *pulOIDSize)
{
    unsigned int    ulPDULen;
    unsigned char   ucOIDSize;
    unsigned char   ucTmp;

    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        ulPDULen = snmp_get_int_value(pucRspData, ucTmp);
        pucRspData += ucTmp;

        if (ulRspDataLen != (1+1+ucTmp+ulPDULen))
        {
            return -1;
        }
    }
    else
    {
        ulPDULen = pucRspData[0];
        pucRspData ++;

        if (ulRspDataLen != (1+1+ulPDULen))
        {
            return -1;
        }
    }

    //ver check
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1 ||
        pucRspData[2] != ucGetVer)
    {
        return -1;
    }
    pucRspData += (1+1+1);

    //community
    if (pucRspData[0] != ASN_OCTET_STR ||
        pucRspData[1] == 0)
    {
        return -1;
    }
    pucRspData += (1+1+pucRspData[1]);

    //snmp pdu type : get response
    if ((*pucRspData++) != SNMP_MSG_GET)
    {
        return -1;
    }

    //snmp pdu len
    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //req id
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] == 0)
    {
        snmp_log(LOG_ERR, "snmp_parse_get_scalar_req: pucRspData[0]:%u,pucRspData[1]:%u.",
            pucRspData[0],
            pucRspData[1]);
        snmp_log(LOG_ERR, "snmp_parse_get_scalar_req: check reqid failed");
        return -1;
    }

    ucTmp = pucRspData[1];
    pucRspData += 2;

    *pulReqSeq = snmp_get_int_value(pucRspData, ucTmp);
    pucRspData += ucTmp;

    //error status
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1)
    {
        return -1;
    }

    pucRspData += 2;

    if ((*pucRspData++) != SNMP_ERR_NOERROR)
    {
        return -2;
    }

    //error index
    pucRspData += (1+1+1);

    //paras
    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //para1
    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //oid type
    if (pucRspData[0] != ASN_OBJECT_ID ||
        pucRspData[1] == 0)
    {
        return -1;
    }

    ucOIDSize = pucRspData[1];
    pucRspData += 2;

    //check oid
    if (snmp_parse_oid_var(pucRspData, ucOIDSize, pOID, pulOIDSize) < 0)
    {
        return -1;
    }

    pucRspData += ucOIDSize;

    return 0;
}

int snmp_parse_set_scalar_req(unsigned char ucSetVer, unsigned char *pucRspData, unsigned int ulRspDataLen,
                       unsigned int *pulReqSeq, SNMP_OID *pOID, unsigned int *pulOIDSize)
{
    unsigned int    ulPDULen;
    unsigned char   ucOIDSize;
    unsigned char   ucTmp;

    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        ulPDULen = snmp_get_int_value(pucRspData, ucTmp);
        pucRspData += ucTmp;

        if (ulRspDataLen != (1+1+ucTmp+ulPDULen))
        {
            return -1;
        }
    }
    else
    {
        ulPDULen = pucRspData[0];
        pucRspData ++;

        if (ulRspDataLen != (1+1+ulPDULen))
        {
            return -1;
        }
    }

    //ver check
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1 ||
        pucRspData[2] != ucSetVer)
    {
        return -1;
    }
    pucRspData += (1+1+1);

    //community
    if (pucRspData[0] != ASN_OCTET_STR ||
        pucRspData[1] == 0)
    {
        return -1;
    }
    pucRspData += (1+1+pucRspData[1]);

    //snmp pdu type : get response
    if ((*pucRspData++) != SNMP_MSG_SET)
    {
        return -1;
    }

    //snmp pdu len
    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //req id
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] == 0)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_scalar_req: pucRspData[0]:%u,pucRspData[1]:%u.",
            pucRspData[0],
            pucRspData[1]);
        snmp_log(LOG_ERR, "snmp_parse_set_scalar_req: check reqid failed");
        return -1;
    }

    ucTmp = pucRspData[1];
    pucRspData += 2;

    *pulReqSeq = snmp_get_int_value(pucRspData, ucTmp);
    pucRspData += ucTmp;

    //error status
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1)
    {
        return -1;
    }

    pucRspData += 2;

    if ((*pucRspData++) != SNMP_ERR_NOERROR)
    {
        return -2;
    }

    //error index
    pucRspData += (1+1+1);

    //paras
    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //para1
    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //oid type
    if (pucRspData[0] != ASN_OBJECT_ID ||
        pucRspData[1] == 0)
    {
        return -1;
    }

    ucOIDSize = pucRspData[1];
    pucRspData += 2;

    //check oid
    if (snmp_parse_oid_var(pucRspData, ucOIDSize, pOID, pulOIDSize) < 0)
    {
        return -1;
    }

    pucRspData += ucOIDSize;

    return 0;
}

int snmp_parse_get_scalar_rsp(unsigned char ucSetVer, unsigned char *pucRspData, unsigned int ulRspDataLen,
                              unsigned int *pulReqSeq, SNMP_OID *pOID, unsigned int *pulOIDSize)
{
    unsigned int    ulPDULen;
    unsigned char   ucOIDSize;
    unsigned char   ucTmp;

    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        ulPDULen = snmp_get_int_value(pucRspData, ucTmp);
        pucRspData += ucTmp;

        if (ulRspDataLen != (1+1+ucTmp+ulPDULen))
        {
            return -1;
        }
    }
    else
    {
        ulPDULen = pucRspData[0];
        pucRspData ++;

        if (ulRspDataLen != (1+1+ulPDULen))
        {
            return -1;
        }
    }

    //ver check
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1 ||
        pucRspData[2] != ucSetVer)
    {
        return -1;
    }
    pucRspData += (1+1+1);

    //community
    if (pucRspData[0] != ASN_OCTET_STR ||
        pucRspData[1] == 0)
    {
        return -1;
    }
    pucRspData += (1+1+pucRspData[1]);

    //snmp pdu type : get response
    if ((*pucRspData++) != SNMP_MSG_RESPONSE)
    {
        return -1;
    }

    //snmp pdu len
    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //req id
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] == 0)
    {
        snmp_log(LOG_ERR, "snmp_parse_get_scalar_rsp: pucRspData[0]:%u,pucRspData[1]:%u.",
            pucRspData[0],
            pucRspData[1]);
        snmp_log(LOG_ERR, "snmp_parse_get_scalar_rsp: check reqid failed");
        return -1;
    }

    ucTmp = pucRspData[1];
    pucRspData += 2;

    *pulReqSeq = snmp_get_int_value(pucRspData, ucTmp);
    pucRspData += ucTmp;

    //error status
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1)
    {
        return -1;
    }

    pucRspData += 2;

    if ((*pucRspData++) != SNMP_ERR_NOERROR)
    {
        return -2;
    }

    //error index
    pucRspData += (1+1+1);

    //paras
    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //para1
    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //oid type
    if (pucRspData[0] != ASN_OBJECT_ID ||
        pucRspData[1] == 0)
    {
        return -1;
    }

    ucOIDSize = pucRspData[1];
    pucRspData += 2;

    //check oid
    if (snmp_parse_oid_var(pucRspData, ucOIDSize, pOID, pulOIDSize) < 0)
    {
        return -1;
    }

    pucRspData += ucOIDSize;

    return 0;
}

int snmp_parse_get_next_rsp(SNMP_GET_TABLE_PDU_T *pstPDU, unsigned char ucGetVer, unsigned char *pucRspData,
                            unsigned int ulRspDataLen, SNMP_OID *pNextOID, unsigned int *pulNextOIDSize,
                            SNMP_VALUE_T *pstValue)
{
    unsigned int    ulPDULen;
    unsigned int    ulDataLen;
    unsigned char   ucOIDSize;
    unsigned char   ucDataType;
    unsigned char   ucTmp;
    unsigned char   *pucRsp = pucRspData;

    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        ulPDULen = snmp_get_int_value(pucRspData, ucTmp);
        pucRspData += ucTmp;

        if (ulRspDataLen != (1+1+ucTmp+ulPDULen))
        {
            return -1;
        }
    }
    else
    {
        ulPDULen = pucRspData[0];
        pucRspData ++;

        if (ulRspDataLen != (1+1+ulPDULen))
        {
            return -1;
        }
    }

    //ver check
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1 ||
        pucRspData[2] != ucGetVer)
    {
        return -1;
    }
    pucRspData += (1+1+1);

    //community
    if (pucRspData[0] != ASN_OCTET_STR ||
        pucRspData[1] == 0)
    {
        return -1;
    }
    pucRspData += (1+1+pucRspData[1]);

    //snmp pdu type : get response
    if ((*pucRspData++) != SNMP_MSG_RESPONSE)
    {
        return -1;
    }

    //snmp pdu len
    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //req id
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] == 0)
    {
        snmp_log(LOG_ERR, "snmp_parse_get_next_rsp: pucRspData[0]:%u,pucRspData[1]:%u.",
            pucRspData[0],
            pucRspData[1]);
        snmp_log(LOG_ERR, "snmp_parse_get_next_rsp: check reqid failed");
        return -1;
    }

    ucTmp = pucRspData[1];
    pucRspData += 2;

    CHECK_REQ_ID(pstPDU->stReqID, pucRspData)
    {
        snmp_log(LOG_ERR, "snmp_parse_get_next_rsp: pucRspData:%02x %02x,pstPDU->stReqID.pucValue:%02x %02x.",
            pucRspData[0], pucRspData[1],
            pstPDU->stReqID.pucValue[0],
            pstPDU->stReqID.pucValue[1]);

        // add print
        nms_print_binary(pucRsp, ulRspDataLen);
        //nms_print_table_pdu(pstPDU);

        snmp_log(LOG_ERR, "snmp_parse_get_next_rsp: check reqid failed");
        return -1;
    }
    pucRspData += ucTmp;

    //error status
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1)
    {
        return -1;
    }

    pucRspData += 2;

    if ((*pucRspData++) != SNMP_ERR_NOERROR)
    {
        return -2;
    }

    //error index
    pucRspData += (1+1+1);

    //paras
    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //para1
    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //oid type
    if (pucRspData[0] != ASN_OBJECT_ID ||
        pucRspData[1] == 0)
    {
        return -1;
    }

    ucOIDSize = pucRspData[1];
    pucRspData += 2;

    //check oid
    if (snmp_parse_oid_var(pucRspData, ucOIDSize, pNextOID, pulNextOIDSize) < 0)
    {
        return -1;
    }
    pucRspData += ucOIDSize;

    //value type
    ucDataType = *pucRspData++;
    //if (ucDataType != ASN_OCTET_STR)
    {
        //return -1;
    }

    //octet string len
    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        ulDataLen = snmp_get_int_value(pucRspData, ucTmp);
        pucRspData += ucTmp;
    }
    else
    {
        ulDataLen = pucRspData[0];
        pucRspData ++;
    }

    pstValue->ucType = ucDataType;
    pstValue->ulLen  = ulDataLen;

    if (ucDataType == ASN_OCTET_STR)
    {
        pstValue->ulLen = strlen((char*)pucRspData)+1;
        if (pstValue->ulLen > ulDataLen)
        {
            pstValue->ulLen = ulDataLen;
        }
    }

    if (ulDataLen == 0)
    {
        pstValue->pucValue = NULL;
    }
    else
    {
        if (ucDataType == ASN_GAUGE     ||
            ucDataType == ASN_TIMETICKS ||
            ucDataType == ASN_COUNTER   ||
            ucDataType == ASN_INTEGER   )
        {
            pstValue->pucValue = (unsigned char*)snmp_malloc(sizeof(unsigned int));
            if (pstValue->pucValue == NULL)
            {
                return -1;
            }

            if (ulDataLen == sizeof(unsigned int)+1)
            {
                pucRspData ++;
                pstValue->ulLen --;
            }

            /*
            pulTmp = (unsigned int*)pstValue->pucValue;
            pulTmp[0] = 0;
            for (i=0; i<pstValue->ulLen; i++)
            {
                pulTmp[0] = (pulTmp[0]<<8) + pucRspData[i];
            }

            if (ucDataType == ASN_INTEGER)
            {
                iTmp = pulTmp[0];
                if (ulDataLen == 1 && iTmp > 0x80)
                {
                    pulTmp[0] = iTmp - 0x100;
                }
                else if (ulDataLen == 2 && iTmp > 0x8000)
                {
                    pulTmp[0] = iTmp - 0x10000;
                }
                else if (ulDataLen == 3 && iTmp > 0x800000)
                {
                    pulTmp[0] = iTmp - 0x1000000;
                }
            } */

            nms_get_data(ucDataType, pstValue->ulLen, pucRspData, pstValue->pucValue);
        }
        else
        {
            pstValue->pucValue = (unsigned char*)snmp_malloc(ulDataLen);
            if (pstValue->pucValue == NULL)
            {
                return -1;
            }
            memcpy(pstValue->pucValue, pucRspData, ulDataLen);
        }
    }

    return 0;
}

int snmp_parse_get_batch_rsp(SNMP_GET_BATCH_PDU_T *pstPDU, unsigned char ucGetVer, unsigned char *pucRspData,
    unsigned int ulRspDataLen, SNMP_TABLE_VALUE_T *pstValue)
{
    unsigned int    ulPDULen;
    unsigned int    ulDataLen;
    unsigned char   ucOIDSize;
    unsigned char   ucDataType;
    unsigned char   ucTmp;
    unsigned int    i;
    unsigned char   *pucRsp = pucRspData;

    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        ulPDULen = snmp_get_int_value(pucRspData, ucTmp);
        pucRspData += ucTmp;

        if (ulRspDataLen != (1+1+ucTmp+ulPDULen))
        {
            return -1;
        }
    }
    else
    {
        ulPDULen = pucRspData[0];
        pucRspData ++;

        if (ulRspDataLen != (1+1+ulPDULen))
        {
            return -1;
        }
    }

    //ver check
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1 ||
        pucRspData[2] != ucGetVer)
    {
        return -1;
    }
    pucRspData += (1+1+1);

    //community
    if (pucRspData[0] != ASN_OCTET_STR ||
        pucRspData[1] == 0)
    {
        return -1;
    }
    pucRspData += (1+1+pucRspData[1]);

    //snmp pdu type : get response
    if ((*pucRspData++) != SNMP_MSG_RESPONSE)
    {
        return -1;
    }

    //snmp pdu len
    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //req id
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] == 0)
    {
        snmp_log(LOG_ERR, "snmp_parse_get_scalra_rsp: pucRspData[0]:%u,pucRspData[1]:%u.",
            pucRspData[0],
            pucRspData[1]);
        snmp_log(LOG_ERR, "snmp_parse_get_scalra_rsp: check reqid failed");
        return -1;
    }

    ucTmp = pucRspData[1];
    pucRspData += 2;

    CHECK_REQ_ID(pstPDU->stReqID, pucRspData)
    {
        snmp_log(LOG_ERR, "snmp_parse_get_scalra_rsp: pucRspData:%02x %02x,pstPDU->stReqID.pucValue:%02x %02x.",
            pucRspData[0], pucRspData[1],
            pstPDU->stReqID.pucValue[0],
            pstPDU->stReqID.pucValue[1]);

        // add print
        nms_print_binary(pucRsp, ulRspDataLen);
        //nms_print_table_pdu(pstPDU);

        snmp_log(LOG_ERR, "snmp_parse_get_scalra_rsp: check reqid failed");
        return -1;
    }

    pucRspData += ucTmp;

    //error status
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1)
    {
        return -1;
    }

    pucRspData += 2;

    if ((*pucRspData++) != SNMP_ERR_NOERROR)
    {
        return -2;
    }

    //error index
    pucRspData += (1+1+1);

    //paras
    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }


    for (i = 0; i < pstValue->ulColumnNum; i++)
    {
        //para
        if ((*pucRspData++) != SNMP_ID_SEQUENCE)
        {
            return -1;
        }

        if (pucRspData[0] > 0x80)
        {
            ucTmp = pucRspData[0] - 0x80;
            pucRspData ++;

            pucRspData += ucTmp;
        }
        else
        {
            pucRspData ++;
        }

        //oid type
        if (pucRspData[0] != ASN_OBJECT_ID ||
            pucRspData[1] == 0)
        {
            return -1;
        }

        ucOIDSize = pucRspData[1];
        pucRspData += 2;

        if (memcmp(pstPDU->pstOID[i].pucValue, pucRspData, pstPDU->pstOID[i].ulLen) != 0)
        {
            return -1;
        }
        //check oid
        pucRspData += ucOIDSize;

        //octet string type
        ucDataType = *pucRspData++;
        //if (ucDataType != ASN_OCTET_STR)
        {
            //return -1;
        }

        //octet string len
        if (pucRspData[0] > 0x80)
        {
            ucTmp = pucRspData[0] - 0x80;
            pucRspData ++;

            ulDataLen = snmp_get_int_value(pucRspData, ucTmp);
            pucRspData += ucTmp;
        }
        else
        {
            ulDataLen = pucRspData[0];
            pucRspData ++;
        }

        //check len
        if (ulDataLen > pstValue->pstValue[i].ulLen)
        {
            if ((ucDataType == ASN_COUNTER || ucDataType == ASN_GAUGE ) && ulDataLen == (pstValue->pstValue[i].ulLen+1))
            {
                pucRspData ++;
                ulDataLen --;
            }
            else if (ucDataType == ASN_OCTET_STR)
            {
                ulDataLen = strlen((char*)pucRspData)+1;
                if (ulDataLen > pstValue->pstValue[i].ulLen)
                {
                    ulDataLen = pstValue->pstValue[i].ulLen;
                }
            }
            else
            {
                return -1;
            }
        }

        pstValue->pstValue[i].ulLen = ulDataLen;

        nms_get_data(ucDataType, ulDataLen, pucRspData, pstValue->pstValue[i].pucValue);

        pucRspData += ulDataLen;

        if (pucRspData >= (pucRsp+ulRspDataLen))
        {
            break;
        }
    }
    return 0;
}

int snmp_parse_trap2_pdu(unsigned char *pucRspData,
                        unsigned int ulRspDataLen,
                        SNMP_TRAP2_PDU_T *pstPDU)
{
    unsigned int    ulPDULen;
    unsigned int    ulDataLen;
    unsigned char   ucOIDSize;
    unsigned char   ucDataType;
    unsigned char   ucTmp;
    int             iRet;
    unsigned char   *pucOriRspData = pucRspData;

    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        ulPDULen = snmp_get_int_value(pucRspData, ucTmp);
        pucRspData += ucTmp;

        if (ulRspDataLen != (1+1+ucTmp+ulPDULen))
        {
            return -1;
        }
    }
    else
    {
        ulPDULen = pucRspData[0];
        pucRspData ++;

        if (ulRspDataLen != (1+1+ulPDULen))
        {
            return -1;
        }
    }

    //ver check
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1 ||
        pucRspData[2] != SNMP_TRAP_VER)
    {
        return -1;
    }
    pucRspData += (1+1+1);

    //community
    if (pucRspData[0] != ASN_OCTET_STR ||
        pucRspData[1] == 0)
    {
        return -1;
    }
    pucRspData += (1+1+pucRspData[1]);

    //snmp pdu type : get response
    if ((*pucRspData) != SNMP_MSG_TRAP2 &&
        (*pucRspData) != SNMP_MSG_INFORM)
    {
        return -1;
    }

    pucRspData++;

    //snmp pdu len
    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //req id
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] == 0)
    {
        snmp_log(LOG_ERR, "snmp_parse_get_scalra_rsp: check reqid failed");
        return -1;
    }

    ucTmp = pucRspData[1];
    pucRspData += 2;
    CHECK_REQ_ID(pstPDU->stReqID, pucRspData)
    {
        snmp_log(LOG_ERR, "snmp_parse_get_scalra_rsp: check reqid failed");
        return -1;
    }
    pucRspData += ucTmp;

    //error status
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1)
    {
        return -1;
    }

    pucRspData += 2;

    if ((*pucRspData++) != SNMP_ERR_NOERROR)
    {
        return -2;
    }

    //error index
    pucRspData += (1+1+1);

    //paras
    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    pstPDU->pstVarList = NULL;
    while(pucRspData < pucOriRspData+ulRspDataLen)
    {
        SNMP_OID_T      stOID;
        SNMP_OID_T      *pstOID = &stOID;
        SNMP_VALUE_T    stValue = {0};
        SNMP_VALUE_T    *pstValue = &stValue;
        unsigned char   aucTmp[8];

        memset(&stValue, 0, sizeof(stValue));

        //para
        if ((*pucRspData++) != SNMP_ID_SEQUENCE)
        {
            return -1;
        }

        if (pucRspData[0] > 0x80)
        {
            ucTmp = pucRspData[0] - 0x80;
            pucRspData ++;

            pucRspData += ucTmp;
        }
        else
        {
            pucRspData ++;
        }

        //oid type
        if (pucRspData[0] != ASN_OBJECT_ID ||
            pucRspData[1] == 0)
        {
            return -1;
        }

        ucOIDSize = pucRspData[1];
        pucRspData += 2;

        //check oid
        if (snmp_parse_oid_var(pucRspData, ucOIDSize, pstOID->astOID, &pstOID->ulOIDSize) < 0)
        {
            return -1;
        }
        pucRspData += ucOIDSize;

        //value type
        ucDataType = *pucRspData++;

        //value len
        if (pucRspData[0] > 0x80)
        {
            ucTmp = pucRspData[0] - 0x80;
            pucRspData ++;

            ulDataLen = snmp_get_int_value(pucRspData, ucTmp);
            pucRspData += ucTmp;
        }
        else
        {
            ulDataLen = pucRspData[0];
            pucRspData ++;
        }

        pstValue->ucType = ucDataType;
        pstValue->ulLen  = ulDataLen;

        if (ulDataLen == 0)
        {
            pstValue->pucValue = NULL;
        }
        else
        {
            if (ucDataType == ASN_GAUGE     ||
                ucDataType == ASN_INTEGER   ||
                ucDataType == ASN_TIMETICKS ||
                ucDataType == ASN_COUNTER   )
            {
                if (pstValue->ulLen == sizeof(unsigned int)+1)
                {
                    pucRspData ++;
                    pstValue->ulLen --;
                }

                pstValue->pucValue = (unsigned char*)aucTmp;
                if (pstValue->ulLen > sizeof(unsigned int))
                {
                    return -1;
                }

                nms_get_data(ucDataType, pstValue->ulLen, pucRspData, pstValue->pucValue);
                pstValue->ulLen = sizeof(unsigned int);
            }
            else if (ucDataType == ASN_OBJECT_ID)
            {
                pstValue->pucValue = (unsigned char*)snmp_calloc(sizeof(SNMP_OID), SNMP_MAX_OID_SIZE);
                if (pstValue->pucValue == NULL)
                {
                    return -1;
                }

                if (snmp_parse_oid_var(pucRspData, ulDataLen, (UINT32*)pstValue->pucValue, &pstValue->ulLen) < 0)
                {
                    snmp_free(pstValue->pucValue);
                    pstValue->pucValue = NULL;
                    return -1;
                }

                pstValue->ulLen *= sizeof(SNMP_OID);
            }
            else
            {
                pstValue->pucValue = pucRspData;
            }

            pucRspData += ulDataLen;
        }

        iRet = snmp_varlist_add_var(&pstPDU->pstVarList, pstOID->astOID, pstOID->ulOIDSize,
                                 pstValue->ucType,
                                 pstValue->pucValue, pstValue->ulLen);

        // ucDataType == ASN_OBJECT_ID 是通过malloc申请内存，其它的类型使用的是临时变量。
        if (ucDataType == ASN_OBJECT_ID)
        {
            snmp_free(pstValue->pucValue);
        }

        if (iRet < 0)
        {
            return -1;
        }
    }

    return 0;
}

/*************************************************************************
* 函数名称: snmp_parse_set_rsp
* 功    能: 解析set应答报文
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_parse_set_rsp(SNMP_SET_PDU_T *pstPDU, unsigned char ucSetVer,
                       unsigned char *pucRspData, unsigned int ulRspDataLen)
{
    unsigned int    ulPDULen;
    unsigned char   ucOIDSize;
    unsigned char   ucErrorStatus;
    unsigned char   ucTmp;
    unsigned char   ucRspDataType;

    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check snmp pdu failed");
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        ulPDULen = snmp_get_int_value(pucRspData, ucTmp);
        pucRspData += ucTmp;

        if (ulRspDataLen != (1+1+ucTmp+ulPDULen))
        {
            snmp_log(LOG_ERR, "snmp_parse_set_rsp: check snmp pdu len failed");
            return -1;
        }
    }
    else
    {
        ulPDULen = pucRspData[0];
        pucRspData ++;

        if (ulRspDataLen != (1+1+ulPDULen))
        {
            snmp_log(LOG_ERR, "snmp_parse_set_rsp: check snmp pdu len failed");
            return -1;
        }
    }

    //ver check
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1 ||
        pucRspData[2] != ucSetVer)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check ver failed");
        return -1;
    }
    pucRspData += (1+1+1);

    //community
    if (pucRspData[0] != ASN_OCTET_STR ||
        pucRspData[1] == 0)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check community failed");
        return -1;
    }
    pucRspData += (1+1+pucRspData[1]);

    //snmp pdu type : get response
    if ((*pucRspData++) != SNMP_MSG_RESPONSE)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check snmp pdu type failed");
        return -1;
    }

    //snmp pdu len
    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //req id
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] == 0)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check reqid failed");
        return -1;
    }

    ucTmp = pucRspData[1];
    pucRspData += 2;
    CHECK_REQ_ID(pstPDU->stReqID, pucRspData)
        /*pstPDU->stReqID.pucValue &&
        memcmp(pucRspData, pstPDU->stReqID.pucValue, 2) != 0) */
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check reqid failed");
        return -1;
    }
    pucRspData += ucTmp;

    //error status
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check error status failed");
        return -1;
    }

    pucRspData += 2;
    ucErrorStatus = *pucRspData++;
    if (ucErrorStatus != SNMP_ERR_NOERROR)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: snmp set failed, error status is %d", ucErrorStatus);
        return -2;
    }

    //error index
    pucRspData += (1+1+1);

    //paras
    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check paras failed");
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //para1
    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check para failed");
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //oid type
    if (pucRspData[0] != ASN_OBJECT_ID ||
        pucRspData[1] == 0)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check oid failed");
        return -1;
    }

    ucOIDSize = pucRspData[1];
    pucRspData += 2;
    if ((unsigned int)ucOIDSize != pstPDU->stOID.ulLen &&
        (unsigned int)ucOIDSize != (pstPDU->stOID.ulLen+1) )
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check oid size failed");
        return -1;
    }

    if (pstPDU->stOID.pucValue &&
        memcmp(pucRspData, pstPDU->stOID.pucValue, pstPDU->stOID.ulLen) != 0)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check oid value failed");
        return -1;
    }

    pucRspData += ucOIDSize;

    //value type
    ucRspDataType = *pucRspData++;
    if (ucRspDataType != pstPDU->stValue.ucType)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check set data type(%d <-> %d) failed",
                     ucRspDataType, pstPDU->stValue.ucType);
        return -1;
    }

    //octet string len
    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    return 0;
}

/*************************************************************************
* 函数名称: snmp_get_socket_err
* 功    能: 获取socket错误原因
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
char* snmp_get_socket_err(void)
{
    int iErrCode;
#ifdef HAVE_TYPEDEF
    static char acErrInfo[32];
#endif

#ifdef WIN32
    iErrCode = WSAGetLastError();
#else
    iErrCode = errno;
#endif

#ifdef HAVE_TYPEDEF
    sprintf(acErrInfo, "Socket error: %d", iErrCode);
    return acErrInfo;
#else

    switch (iErrCode)
    {
    case SEDESTADDRREQ:
        return "Socket error: Destination address required!";

    case SEPROTOTYPE:
        return "Socket error: Protocol wrong type for socket!";

    case SENOPROTOOPT:
        return "Socket error: Protocol not available!";

    case SEPROTONOSUPPORT:
        return "Socket error: Protocol not supported!";

    case SESOCKTNOSUPPORT:
        return "Socket error: Socket type not supported!";

    case SEOPNOTSUPP:
        return "Socket error: Operation not supported on socket!";

    case SEPFNOSUPPORT:
        return "Socket error: Protocol family not supported!";

    case SEAFNOSUPPORT:
        return "Socket error: Addr family not supported!";

    case SEADDRINUSE:
        return "Socket error: Port already in use!";

    case SEADDRNOTAVAIL:
        return "Socket error: Can't assign requested address!";

    case SENOTSOCK:
        return "Socket error: Socket operation on non-socket!";

    case SENETUNREACH:
        return "Socket error: Network is unreachable!";

    case SENETRESET:
        return "Socket error: Network dropped connection on reset!";

    case SECONNABORTED:
        return "Socket error: Software caused connection abort!";

    case SECONNRESET:
        return "Socket error: Connection reset by peer!";

    case SENOBUFS:
        return "Socket error: No buffer space available!";

    case SEISCONN:
        return "Socket error: Socket is already connected!";

    case SENOTCONN:
        return "Socket error: Socket is not connected!";

    case SETOOMANYREFS:
        return "Socket error: Too many references: can't splice!";

    case SESHUTDOWN:
        return "Socket error: Can't send after socket shutdown!";

    case SETIMEDOUT:
        return "Socket error: Connection timed out!";

    case SECONNREFUSED:
        return "Socket error: Connection refused!";

    case SENETDOWN:
        return "Socket error: Network is down!";

    case SELOOP:
        return "Socket error: Too many levels of symbolic links!";

    case SEHOSTUNREACH:
        return "Socket error: No route to host!";

    case SEHOSTDOWN:
        return "Socket error: Peer is down!";

    case SEINPROGRESS:
        return "Socket error: Operation now in progress!";

    case SEALREADY:
        return "Socket error: Operation already in progress!";

    case SEWOULDBLOCK:
        return "Socket error: Operation would block!";

    case SENOTINITIALISED:
        return "Socket error: Not initialized!";

    case SEFAULT:
        return "Socket error: Bad ip address!";

    case SEINVAL:
        return "Socket error: Invalid parameter!";

    default:
        return "Scoket error: unknown error!";
    }
#endif
}

/*************************************************************************
* 函数名称: snmp_init_socket
* 功    能: 初始化socket
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_init_socket(void)
{
#ifdef WIN32
    WORD        wVersionRequested;
    WSADATA     wsaData;

    wVersionRequested = MAKEWORD (2, 2);
    if (WSAStartup (wVersionRequested, &wsaData) != 0)
    {
        return -1;
    }

    if (LOBYTE (wsaData.wVersion) != 2 ||
        HIBYTE (wsaData.wVersion) != 2 )
    {
        WSACleanup ();
        return -1;
    }

    return 0;
#else
    return 0;
#endif
}

/*************************************************************************
* 函数名称: snmp_recv_from
* 功    能: 接受socket消息（允许超时）
* 参    数:
* 参数名称          输入/输出               描述
  ulTimeout                                 单位ms
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_recv_from(SNMP_SOCKET stSock, void *pMsgBuf, unsigned int ulMsgBufLen, unsigned int ulTimeout)
{
    fd_set fds;
    struct timeval timeout = {0};
    int iRet;
    int iRcvLen;

    /* 同步调用的超时时间必定大于0，异步调用不需要recv */
    if (ulTimeout == 0)
    {
        //snmp_log(LOG_ERR, "snmp_recv_from: timeout(%u) must be large than 0", ulTimeout);
        //return -1;

        iRcvLen = recv(stSock, (char*)pMsgBuf, ulMsgBufLen, 0);
        return iRcvLen;
    }

again:
    timeout.tv_sec = ulTimeout/1000;
    timeout.tv_usec = 1000*(ulTimeout%1000);
    FD_ZERO(&fds);
    FD_SET(stSock, &fds);

    iRet = select(stSock + 1, &fds, NULL, NULL, &timeout);
    if (iRet == 0)
    {
//        snmp_log(LOG_ERR, "snmp_recv_from: select timeout(%u)", ulTimeout);
        return 0;
    }

    if (iRet < 0)
    {
        if (errno == EINTR)
            goto again;

        snmp_log(LOG_ERR, "snmp_recv_from: select() failed(%d)", iRet);
        return -1;
    }

    if (FD_ISSET(stSock, &fds))
    {
        iRcvLen = recv(stSock, (char*)pMsgBuf, ulMsgBufLen, 0);
        if (iRcvLen <= 0)
        {
            snmp_log(LOG_ERR, "snmp_recv_from: recv() failed, recv Len is %d", iRcvLen);
        }

        FD_CLR(stSock, &fds);

        return iRcvLen;
    }

    snmp_log(LOG_ERR, "snmp_recv_from: select failed, unknown socket");

    FD_CLR(stSock, &fds);
    return -1;
}

/*************************************************************************
* 函数名称: snmp_check_rsp
* 功    能: 发送UDP消息，并接受socket消息（允许超时）
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_check_rsp(unsigned char  ucGetVer,
                   unsigned char  *pucData,
                   unsigned int   ulDataLen,
                   SNMP_TLV_T     *pstReqID)
{
    unsigned char ucTmp = 0;
    unsigned int  ulPDULen = 0;
    unsigned char *pucRspData = pucData;
    unsigned int  ulRspDataLen = ulDataLen;

    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        ulPDULen = snmp_get_int_value(pucRspData, ucTmp);
        pucRspData += ucTmp;

        if (ulRspDataLen != (1+1+ucTmp+ulPDULen))
        {
            return -1;
        }
    }
    else
    {
        ulPDULen = pucRspData[0];
        pucRspData ++;

        if (ulRspDataLen != (1+1+ulPDULen))
        {
            return -1;
        }
    }

    //ver check
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1 ||
        pucRspData[2] != ucGetVer)
    {
        return -1;
    }
    pucRspData += (1+1+1);

    //community
    if (pucRspData[0] != ASN_OCTET_STR ||
        pucRspData[1] == 0)
    {
        return -1;
    }
    pucRspData += (1+1+pucRspData[1]);

    //snmp pdu type : get response
    if ((*pucRspData++) != SNMP_MSG_RESPONSE)
    {
        return -1;
    }

    //snmp pdu len
    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //req id
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] == 0)
    {
        snmp_log(LOG_ERR, "snmp_check_rsp: pucRspData[0]:%u,pucRspData[1]:%u.",
            pucRspData[0],
            pucRspData[1]);
        snmp_log(LOG_ERR, "snmp_check_rsp: check reqid failed");
        return -1;
    }

    ucTmp = pucRspData[1];
    pucRspData += 2;

    // pstReqID->pucValue 0101  pucRspData 00000101
    if (pstReqID->pucValue && (unsigned int)ucTmp >= pstReqID->ulLen && ucTmp <= 4 &&
        memcmp(pucRspData+ucTmp-pstReqID->ulLen, pstReqID->pucValue, pstReqID->ulLen) != 0)
    {
        snmp_log(LOG_ERR, "snmp_check_rsp: check reqid failed");
        return -1;
    }

    return 0;
}

int snmp_create_socket(SNMP_SESSION_T *pstSession)
{
    pstSession->stSock = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (pstSession->stSock == INVALID_SOCKET)
    {
        snmp_log(LOG_ERR, "snmp create socket failed");

        return -1;
    }
/*
    memset(&stLocalAddr, 0, sizeof(stLocalAddr));
    stLocalAddr.sin_family = PF_INET;
    stLocalAddr.sin_port   = htons(pstSession->usAgentPort);
    memcpy(&stLocalAddr.sin_addr.s_addr, aucLocalAddr, 4);

    if (setsockopt(pstSession->stSock, SOL_SOCKET, SO_REUSEADDR, (char*)&ulFlag, sizeof(ulFlag)) == SOCKET_ERROR)
    {
        CLOSE_SOCKET(pstSession->stSock);

        return -1;
    }

    if (bind (pstSession->stSock, (SOCKADDR *) &stLocalAddr, sizeof (SOCKADDR)) == SOCKET_ERROR)
    {
        CLOSE_SOCKET (pstSession->stSock);

        return -1;
    }
*/
    return 0;
}

void snmp_close_socket(SNMP_SESSION_T *pstSession)
{
    if (pstSession)
    {
        CLOSE_SOCKET(pstSession->stSock);
    }
}

/*************************************************************************
* 函数名称: snmp_udp_send
* 功    能: 发送UDP消息，并接受socket消息（允许超时）
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_udp_send(SNMP_SESSION_T *pstSession,
                  void *pstReqMsg, unsigned int ulReqMsgLen,
                  void *pRcvMsg, unsigned int ulMaxRcvMsgLen, unsigned int *pulRcvMsgLen,
                  SNMP_TLV_T *pstReqID)
{
    int             iRet = 0;
    SOCKADDR_IN     stSockAddr = {0};
    SOCKADDR        *pstAddr = (SOCKADDR *)&stSockAddr;
    int             iAddrLen = sizeof(SOCKADDR);
    int             iRcvSize;
    unsigned int    i = 0;
    unsigned int    ulRecvTimeout = pstSession->ulTimeout; // 10
    unsigned int    ulSendTime = 1;//(ulTimeout+ulRecvTimeout-1)/ulRecvTimeout;

    for (i=0; i<ulSendTime; i++)
    {
        if (pstSession->stSock == INVALID_SOCKET)
        {
            if (snmp_create_socket(pstSession) < 0)
            {
                return -1;
            }
        }

        stSockAddr.sin_family = AF_INET;
        stSockAddr.sin_port   = htons(pstSession->usAgentPort);
        memcpy(&stSockAddr.sin_addr.s_addr, pstSession->aucAgentIP, 4);

        //nms_print_binary(pstReqMsg, ulReqMsgLen);

        iRet = sendto(pstSession->stSock, (const char*)pstReqMsg, ulReqMsgLen, 0, pstAddr, iAddrLen);
        if (iRet < 0)
        {
            snmp_log(LOG_ERR, "snmp_udp_send: sendto() failed");
            snmp_close_socket(pstSession);
            return -1;
        }

        if (pRcvMsg && pulRcvMsgLen)
        {
            iRcvSize = snmp_recv_from(pstSession->stSock, (char*)pRcvMsg, ulMaxRcvMsgLen, ulRecvTimeout);
            if (iRcvSize <= 0)
            {
                snmp_log(LOG_ERR, "snmp_udp_send: snmp_recv_from() failed(%s)", snmp_get_socket_err());
                snmp_close_socket(pstSession);
                return -1;
            }

            if ((unsigned int)iRcvSize > ulMaxRcvMsgLen)
            {
                snmp_log(LOG_ERR, "snmp_udp_send: recv size(%d) is too big", iRcvSize);

                snmp_close_socket(pstSession);
                return -1;
            }

            *pulRcvMsgLen = iRcvSize;
        }
        //closesocket(stSock);

        if (pRcvMsg && pulRcvMsgLen)
        {
            if (-1 == snmp_check_rsp((unsigned char)pstSession->ulGetVer, (unsigned char*)pRcvMsg, *pulRcvMsgLen, pstReqID))
            {
                //snmp_log(LOG_ERR, "snmp_check_rsp: check failed");
                continue;
            }
        }
        // add temp
        //nms_print_binary(pRcvMsg, *pulRcvMsgLen);

        snmp_close_socket(pstSession);
        return 0;
    }

    snmp_close_socket(pstSession);
    snmp_log(LOG_ERR, "snmp_udp_send: check rsp failed");
    return -1;
}

int snmp_udp_send_ex(SNMP_SESSION_T *pstSession, SOCKET stSock,
                  void *pstReqMsg, unsigned int ulReqMsgLen)
{
    int             iRet = 0;
    SOCKADDR_IN     stSockAddr = {0};
    int             iAddrLen = sizeof(SOCKADDR);

    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port   = htons(pstSession->usAgentPort);
    memcpy(&stSockAddr.sin_addr.s_addr, pstSession->aucAgentIP, 4);

    iRet = sendto(stSock, (const char*)pstReqMsg, ulReqMsgLen, 0, (SOCKADDR *)&stSockAddr, iAddrLen);
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_udp_send: sendto() failed");
        return -1;
    }

    return 0;
}

/*************************************************************************
* 函数名称: snmp_udp_recv
* 功    能: 接收UDP消息
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_udp_recv(SNMP_SESSION_T *pstSession,
                  void *pstReqMsg, unsigned int ulReqMsgLen,
                  void *pRcvMsg, unsigned int ulMaxRcvMsgLen, unsigned int *pulRcvMsgLen,
                  SNMP_TLV_T *pstReqID)
{
    SOCKADDR_IN     stSockAddr = {0};
    int             iRcvSize;
    unsigned int    ulRecvTimeout = pstSession->ulTimeout; // 10

    if (pstSession->stSock == INVALID_SOCKET)
    {
        if (snmp_create_socket(pstSession) < 0)
        {
            return -1;
        }
    }

    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port   = htons(pstSession->usAgentPort);
    memcpy(&stSockAddr.sin_addr.s_addr, pstSession->aucAgentIP, 4);

    iRcvSize = snmp_recv_from(pstSession->stSock, (char*)pRcvMsg, ulMaxRcvMsgLen, ulRecvTimeout);
    snmp_close_socket(pstSession);
    if (iRcvSize <= 0)
    {
        snmp_log(LOG_ERR, "snmp_udp_send: snmp_recv_from() failed(%s)", snmp_get_socket_err());
        return -1;
    }

    if ((unsigned int)iRcvSize > ulMaxRcvMsgLen)
    {
        snmp_log(LOG_ERR, "snmp_udp_send: recv size(%d) is too big", iRcvSize);

        return -1;
    }

    *pulRcvMsgLen = iRcvSize;

    if (-1 == snmp_check_rsp((unsigned char)pstSession->ulGetVer, (unsigned char*)pRcvMsg, *pulRcvMsgLen, pstReqID))
    {
        snmp_log(LOG_ERR, "snmp_udp_recv: check rsp failed");
        return -1;
    }

    return 0;
}

/*************************************************************************
* 函数名称: snmp_init_session
* 功    能: 创建session（对于每个Agent，不需要重复创建）
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
SNMP_SESSION_T* snmp_init_session(unsigned char *pucAgengIP,
                                  unsigned short usAgentPort,
                                  char  *szReadCommunity,
                                  char  *szWriteCommunity,
                                  unsigned int ulGetVer,
                                  unsigned int ulSetVer,
                                  unsigned int ulTimeout)
{
    SNMP_SESSION_T  *pstSession = NULL;

    if (strlen(szReadCommunity) >= sizeof(pstSession->acReadCommunity))
    {
        return NULL;
    }

    if (strlen(szWriteCommunity) >= sizeof(pstSession->acWriteCommunity))
    {
        return NULL;
    }

    if (pstSession == NULL)
    {
        pstSession = (SNMP_SESSION_T*)snmp_malloc(sizeof(SNMP_SESSION_T));
    }

    if (pstSession)
    {
        memset(pstSession, 0, sizeof(SNMP_SESSION_T));

        memcpy(pstSession->aucAgentIP, pucAgengIP, sizeof(pstSession->aucAgentIP));
        pstSession->usAgentPort = usAgentPort;
        pstSession->ulGetVer = ulGetVer;
        pstSession->ulSetVer = ulSetVer;
        pstSession->ulTimeout = ulTimeout;
        pstSession->stSock = INVALID_SOCKET;

        strcpy(pstSession->acReadCommunity, szReadCommunity);
        strcpy(pstSession->acWriteCommunity, szWriteCommunity);
    }

    return pstSession;
}

/*************************************************************************
* 函数名称: snmp_free_session
* 功    能: 释放session
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
VOID snmp_free_session(SNMP_SESSION_T *pstSession)
{
    if (pstSession)
    {
        CLOSE_SOCKET(pstSession->stSock);
        snmp_free(pstSession);
    }
}

/*************************************************************************
* 函数名称: snmp_free_get_pdu
* 功    能: 释放PDU中的动态申请的内存
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_free_get_pdu(SNMP_GET_SCALAR_PDU_T *pstPDU)
{
    FREE_TLV(&pstPDU->stPDU);
    FREE_TLV(&pstPDU->stVer);
    FREE_TLV(&pstPDU->stCommunity);
    FREE_TLV(&pstPDU->stSnmpPDU);
    FREE_TLV(&pstPDU->stReqID);
    FREE_TLV(&pstPDU->stErrStatus);
    FREE_TLV(&pstPDU->stErrIndex);
    FREE_TLV(&pstPDU->stParas);
    FREE_TLV(&pstPDU->stPara1);
    FREE_TLV(&pstPDU->stOID);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_free_getbulk_pdu
* 功    能: 释放PDU中的动态申请的内存
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_free_getbulk_pdu(SNMP_GET_BULK_PDU_T *pstPDU)
{
    FREE_TLV(&pstPDU->stPDU);
    FREE_TLV(&pstPDU->stVer);
    FREE_TLV(&pstPDU->stCommunity);
    FREE_TLV(&pstPDU->stSnmpPDU);
    FREE_TLV(&pstPDU->stReqID);
    FREE_TLV(&pstPDU->stNoRepeaters);
    FREE_TLV(&pstPDU->stMaxRepetitions);
    FREE_TLV(&pstPDU->stParas);
    FREE_TLV(&pstPDU->stPara1);
    FREE_TLV(&pstPDU->stOID);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_free_set_pdu
* 功    能: 释放PDU中的动态申请的内存
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_free_set_pdu(SNMP_SET_PDU_T *pstPDU)
{
    FREE_TLV(&pstPDU->stPDU);
    FREE_TLV(&pstPDU->stVer);
    FREE_TLV(&pstPDU->stCommunity);
    FREE_TLV(&pstPDU->stSnmpPDU);
    FREE_TLV(&pstPDU->stReqID);
    FREE_TLV(&pstPDU->stErrStatus);
    FREE_TLV(&pstPDU->stErrIndex);
    FREE_TLV(&pstPDU->stParas);
    FREE_TLV(&pstPDU->stPara1);
    FREE_TLV(&pstPDU->stOID);
    FREE_TLV(&pstPDU->stValue);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_free_get_scalar_rsp_pdu
* 功    能: 释放PDU中的动态申请的内存
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_free_get_scalar_rsp_pdu(SNMP_GET_SCALAR_RSP_PDU_T *pstPDU)
{
    FREE_TLV(&pstPDU->stPDU);
    FREE_TLV(&pstPDU->stVer);
    FREE_TLV(&pstPDU->stCommunity);
    FREE_TLV(&pstPDU->stSnmpPDU);
    FREE_TLV(&pstPDU->stReqID);
    FREE_TLV(&pstPDU->stErrStatus);
    FREE_TLV(&pstPDU->stErrIndex);
    FREE_TLV(&pstPDU->stParas);
    FREE_TLV(&pstPDU->stPara1);
    FREE_TLV(&pstPDU->stOID);
    FREE_TLV(&pstPDU->stValue);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_get_scalar
* 功    能: 实现snmp get scalar操作
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_get_scalar(SNMP_SESSION_T *pstSession, SNMP_OID *pOID, unsigned int ulOIDSize, /*unsigned char ucType,*/
                    unsigned int ulMaxDataLen, void *pData, unsigned int *pulDataLen)
{
    int                     iRet;
    SNMP_GET_SCALAR_PDU_T   stPDU = {0};
    unsigned char   aucReqMsg[1024];
    unsigned int    ulReqMsgLen = 0;
    unsigned int    ulRecvMsgLen;
    unsigned int    ulDataLen = 0;
    unsigned char   *pBuf = NULL;
    unsigned int    ulBufLen = 64*1024;

    if (!pstSession)
    {
        return -1;
    }

    iRet = snmp_init_get_pdu(pstSession->acReadCommunity, (UINT8)pstSession->ulGetVer, pOID, ulOIDSize, SNMP_MSG_GET, &stPDU);
    if (iRet < 0)
    {
        goto end;
    }

    iRet = snmp_create_get_pdu(&stPDU, aucReqMsg, sizeof(aucReqMsg)-1, &ulReqMsgLen);
    if (iRet < 0)
    {
        goto end;
    }

    iRet = snmp_init_socket();
    if (iRet< 0)
    {
        goto end;
    }

    pBuf = (unsigned char*)snmp_malloc(ulBufLen);
    if (NULL == pBuf)
    {
        iRet = -1;
        goto end;
    }

    iRet = snmp_udp_send(pstSession,
                         aucReqMsg, ulReqMsgLen,
                         pBuf, ulBufLen, &ulRecvMsgLen,
                         &(stPDU.stReqID));
    if (iRet < 0)
    {
        goto end;
    }

    iRet = _snmp_parse_get_scalar_rsp(&stPDU, (UINT8)pstSession->ulGetVer, pBuf, ulRecvMsgLen, (unsigned char*)pData, ulMaxDataLen, &ulDataLen);
    if (iRet < 0)
    {
        goto end;
    }

    if (pulDataLen)
    {
        *pulDataLen = ulDataLen;
    }

end:
    if (pBuf)
    {
        snmp_free(pBuf);
    }
    snmp_free_get_pdu(&stPDU);

    if (iRet < 0)
    {
        return -1;
    }

    return 0;
}

/*************************************************************************
* 函数名称: snmp_recv_get_scalar
* 功    能: 实现snmp get scalar操作
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_recv_get_scalar(SNMP_SESSION_T *pstSession, SNMP_OID *pOID, unsigned int ulOIDSize, /*unsigned char ucType,*/
                         unsigned int ulMaxDataLen, void *pData, unsigned int *pulDataLen)
{
    int                     iRet;
    SNMP_GET_SCALAR_PDU_T   stPDU = {0};
    unsigned char   aucReqMsg[1024];
    unsigned int    ulReqMsgLen = 0;
    unsigned int    ulRecvMsgLen;
    unsigned int    ulDataLen = 0;
    unsigned char   *pBuf = NULL;
    unsigned int    ulBufLen = 64*1024;

    if (!pstSession)
    {
        return -1;
    }

    iRet = snmp_init_socket();
    if (iRet< 0)
    {
        goto end;
    }

    pBuf = (unsigned char*)snmp_malloc(ulBufLen);
    if (NULL == pBuf)
    {
        iRet = -1;
        goto end;
    }

    iRet = snmp_udp_recv(pstSession,
                         aucReqMsg, ulReqMsgLen,
                         pBuf, ulBufLen, &ulRecvMsgLen,
                         &(stPDU.stReqID));
    if (iRet < 0)
    {
        goto end;
    }

    iRet = _snmp_parse_get_scalar_rsp(&stPDU, (UINT8)pstSession->ulGetVer, pBuf, ulRecvMsgLen, (unsigned char*)pData, ulMaxDataLen, &ulDataLen);
    if (iRet < 0)
    {
        goto end;
    }

    if (pulDataLen)
    {
        *pulDataLen = ulDataLen;
    }

end:
    if (pBuf)
    {
        snmp_free(pBuf);
    }
    snmp_free_get_pdu(&stPDU);

    if (iRet < 0)
    {
        return -1;
    }

    return 0;
}

/*************************************************************************
* 函数名称: snmp_get_scalar
* 功    能: 实现snmp get scalar操作
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_get_batch(SNMP_SESSION_T *pstSession, SNMP_TABLE_VALUE_T *pstValue)
{
    int                     iRet;
    SNMP_GET_BATCH_PDU_T    stPDU = {0};
    unsigned char           aucReqMsg[1024];
    unsigned int            ulReqMsgLen = 0;
    unsigned int            ulRecvMsgLen;
    unsigned char           *pBuf = NULL;
    unsigned int            ulBufLen = 64*1024;

    if (!pstSession)
    {
        return -1;
    }

    iRet = snmp_init_get_batch_pdu(pstSession->acReadCommunity, pstValue->ulColumnNum, pstValue->pstOID,
                                    SNMP_MSG_GET, (UINT8)pstSession->ulGetVer, &stPDU);
    if (iRet < 0)
    {
        goto end;
    }

    iRet = snmp_create_get_batch_pdu(&stPDU, aucReqMsg, sizeof(aucReqMsg)-1, &ulReqMsgLen);
    if (iRet < 0)
    {
        goto end;
    }

    iRet = snmp_init_socket();
    if (iRet< 0)
    {
        goto end;
    }

    pBuf = (unsigned char*)snmp_malloc(ulBufLen);
    if (NULL == pBuf)
    {
        iRet = -1;
        goto end;
    }

    iRet = snmp_udp_send(pstSession, //->aucAgentIP, pstSession->usAgentPort, pstSession->ulTimeout,
        aucReqMsg, ulReqMsgLen,
        pBuf, ulBufLen, &ulRecvMsgLen,
        &(stPDU.stReqID));
    if (iRet < 0)
    {
        goto end;
    }


    iRet = snmp_parse_get_batch_rsp(&stPDU, (UINT8)pstSession->ulGetVer, pBuf, ulRecvMsgLen, pstValue);
    if (iRet < 0)
    {
        goto end;
    }

end:
    if (pBuf)
    {
        snmp_free(pBuf);
    }
    snmp_free_get_batch_pdu(&stPDU);

    if (iRet < 0)
    {
        return -1;
    }

    return 0;
}


/*************************************************************************
* 函数名称: snmp_get_next
* 功    能: 实现snmp get next操作
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_get_next(SNMP_SESSION_T *pstSession, SNMP_OID *pOID, unsigned int ulOIDSize,
                  SNMP_OID *pNextOID, unsigned int *pulNextOIDSize, SNMP_VALUE_T *pstValue)
{
    int                     iRet;
    SNMP_GET_TABLE_PDU_T    stPDU = {0};
    unsigned char   aucReqMsg[1024];
    unsigned int    ulReqMsgLen = 0;
    unsigned int    ulRecvMsgLen;
    unsigned char   *pBuf = NULL;
    unsigned int    ulBufLen = 64*1024;

    if (!pstSession)
    {
        return -1;
    }

    iRet = snmp_init_get_pdu(pstSession->acReadCommunity, (UINT8)pstSession->ulGetVer, pOID, ulOIDSize, SNMP_MSG_GETNEXT, &stPDU);
    if (iRet < 0)
    {
        goto end;
    }

    iRet = snmp_create_get_pdu(&stPDU, aucReqMsg, sizeof(aucReqMsg)-1, &ulReqMsgLen);
    if (iRet < 0)
    {
        goto end;
    }

    iRet = snmp_init_socket();
    if (iRet < 0)
    {
        goto end;
    }

    pBuf = (unsigned char*)snmp_malloc(ulBufLen);
    if (NULL == pBuf)
    {
        iRet = -1;
        goto end;
    }

    iRet = snmp_udp_send(pstSession, 
                         aucReqMsg, ulReqMsgLen,
                         pBuf, ulBufLen, &ulRecvMsgLen,
                         &(stPDU.stReqID));
    if (iRet < 0)
    {
        goto end;
    }

    iRet = snmp_parse_get_next_rsp(&stPDU, (UINT8)pstSession->ulGetVer, pBuf, ulRecvMsgLen, pNextOID, pulNextOIDSize, pstValue);
    if (iRet < 0)
    {
        goto end;
    }

end:
    if (pBuf)
    {
        snmp_free(pBuf);
    }

    snmp_free_get_pdu(&stPDU);

    if (iRet < 0)
    {
        return -1;
    }
    return 0;
}

void snmp_free_get_batch_pdu(SNMP_GET_BATCH_PDU_T *pstPDU)
{
    unsigned int    i;
    SNMP_TLV_T      *pstVar;

    if (pstPDU->pstPara)
    {
        for (i=0; i<pstPDU->ulOIDNum; i++)
        {
            pstVar = pstPDU->pstOID + i;
            snmp_free_tlv(pstVar);

            pstVar = pstPDU->pstPara + i;
            snmp_free_tlv(pstVar);
        }

        snmp_free(pstPDU->pstPara);
        pstPDU->pstPara = NULL;

        snmp_free(pstPDU->pstOID);
        pstPDU->pstOID = NULL;
    }

    snmp_free_tlv(&pstPDU->stPDU);
    snmp_free_tlv(&pstPDU->stVer);
    snmp_free_tlv(&pstPDU->stCommunity);
    snmp_free_tlv(&pstPDU->stSnmpPDU);
    snmp_free_tlv(&pstPDU->stReqID);
    snmp_free_tlv(&pstPDU->stErrStatus);
    snmp_free_tlv(&pstPDU->stErrIndex);
    snmp_free_tlv(&pstPDU->stParas);
}

void snmp_free_setbulk_pdu(SNMP_SET_BULK_PDU_T *pstPDU)
{
    unsigned int    i;
    SNMP_TLV_T      *pstVar;

    if (pstPDU->pstPara)
    {
        for (i=0; i<pstPDU->ulOIDNum; i++)
        {
            pstVar = &pstPDU->pstPara[i].stPara;
            snmp_free_tlv(pstVar);

            pstVar = &pstPDU->pstPara[i].stOID;
            snmp_free_tlv(pstVar);

            pstVar = &pstPDU->pstPara[i].stValue;
            snmp_free_tlv(pstVar);
        }

        snmp_free(pstPDU->pstPara);
        pstPDU->pstPara = NULL;
    }

    snmp_free_tlv(&pstPDU->stPDU);
    snmp_free_tlv(&pstPDU->stVer);
    snmp_free_tlv(&pstPDU->stCommunity);
    snmp_free_tlv(&pstPDU->stSnmpPDU);
    snmp_free_tlv(&pstPDU->stReqID);
    snmp_free_tlv(&pstPDU->stErrStatus);
    snmp_free_tlv(&pstPDU->stErrIndex);
    snmp_free_tlv(&pstPDU->stParas);
}

void snmp_free_table_value(SNMP_TABLE_VALUE_T *pstTableValue)
{
    SNMP_OID_T          *pstOID, *pstNextOID;
    SNMP_VALUE_T        *pstValue, *pstNextValue;

    for(pstOID = pstTableValue->pstOID; pstOID != NULL; )
    {
        pstNextOID = pstOID->pstNext;
        snmp_free(pstOID);
        pstOID = pstNextOID;
    }

    for(pstValue = pstTableValue->pstValue; pstValue != NULL; )
    {
        pstNextValue = pstValue->pstNext;
        snmp_free(pstValue);
        pstValue = pstNextValue;
    }
}

/*************************************************************************
* 函数名称: snmp_get_bulk
* 功    能: 实现snmp get bulk操作
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
#if 1
int snmp_get_bulk(SNMP_SESSION_T *pstSession, SNMP_TABLE_VALUE_T *pstValue, SNMP_OID_T  *pstOID)
{
    int             iRet;
    unsigned int    ulReqMsgLen = 0;
    unsigned int    ulRecvMsgLen;
    unsigned char   *pBuf = NULL;
    unsigned char   *pucReqMsg;
    unsigned int    ulBufLen = 64*1024;
    SNMP_GET_BULK_PDU_T stPDU = {0};

    if (!pstSession)
    {
        return -1;
    }

    if (snmp_init_socket() < 0)
    {
        return -1;
    }

    memset(&stPDU, 0, sizeof(stPDU));

    iRet = snmp_init_getbulk_pdu(pstSession->acReadCommunity, pstOID, SNMP_MSG_GETBULK, &stPDU);
    if (iRet < 0)
    {
        goto fail;
    }

    pBuf = snmp_malloc(ulBufLen*2);
    if (NULL == pBuf)
    {
        goto fail;
    }
    pucReqMsg = pBuf + ulBufLen;

    iRet = snmp_create_getbulk_pdu(&stPDU, pucReqMsg, ulBufLen-1, &ulReqMsgLen);
    if (iRet < 0)
    {
        goto fail;
    }

    iRet = snmp_udp_send(pstSession, //->aucAgentIP, pstSession->usAgentPort, pstSession->ulTimeout,
                         pucReqMsg, ulReqMsgLen,
                         pBuf, ulBufLen, &ulRecvMsgLen, &stPDU.stReqID);
    if (iRet < 0)
    {
        goto fail;
    }
    goto end;

fail:
    iRet = -1;

end:
    if (pBuf)
    {
        snmp_free(pBuf);
    }

    snmp_free_getbulk_pdu(&stPDU);

    return iRet;
}
#endif

int snmp_init_setbulk_pdu(char *szCommunity, unsigned char ucSetVer, unsigned int ulOIDNum, SNMP_SET_VAR_T *pstSetVar, SNMP_SET_BULK_PDU_T *pstPDU)
{
    SNMP_TLV_T          *pstVar;
    unsigned int        i;
    unsigned int        ulParaLen = 0;
    unsigned short      usReqIDValue;
    unsigned char       ucOIDLen = 0;
    unsigned char       aucOID[64];
    unsigned char       aucReqID[2];
    unsigned char       ucErrorStatus = 0;
    unsigned char       ucErrorIndex  = 0;
    unsigned char       ucSetType     = SNMP_MSG_SET;
    unsigned char       ucVersion     = ucSetVer;
    unsigned int        ulTmp;

    /* oid & para */
    pstPDU->ulOIDNum = ulOIDNum;
    pstPDU->pstPara = (SNMP_OID_TLV_T*)snmp_calloc(1, ulOIDNum*sizeof(SNMP_OID_TLV_T));
    if (NULL == pstPDU->pstPara)
    {
        snmp_log(LOG_ERR, "snmp_init_setbulk_pdu: snmp_calloc failed");
        return -1;
    }

    for (i=0; i<ulOIDNum; i++)
    {
        /* value */
        pstVar = &pstPDU->pstPara[i].stValue;
        pstVar->ucType = pstSetVar[i].ulReqDataType;
        pstVar->pucValue = pstSetVar[i].pucReqData;
        pstVar->ulLen  = pstSetVar[i].ulReqDataLen;
        pstVar->ucNeedFree = 0;
        if (pstVar->ucType == ASN_GAUGE     ||
            pstVar->ucType == ASN_TIMETICKS ||
            pstVar->ucType == ASN_COUNTER   )
        {
            if(pstVar->ulLen != 4)
            {
                return -1;
            }

            DATA_DUP(pstVar, pstSetVar[i].pucReqData, 4);
            ulTmp = *((unsigned int*)pstVar->pucValue);
            if (ulTmp <= 0xff)
            {
                pstVar->pucValue[0] = (unsigned char)ulTmp;
                pstVar->ulLen = 1;
            }
            else if (ulTmp <= 0xffff)
            {
                pstVar->pucValue[0] = (unsigned char)(ulTmp>>8);
                pstVar->pucValue[1] = (unsigned char)(ulTmp);
                pstVar->ulLen = 2;
            }
            else if (ulTmp <= 0xffffff)
            {
                pstVar->pucValue[0] = (unsigned char)(ulTmp>>16);
                pstVar->pucValue[1] = (unsigned char)(ulTmp>>8);
                pstVar->pucValue[2] = (unsigned char)(ulTmp);
                pstVar->ulLen = 3;
            }
            else
            {
                pstVar->pucValue[0] = (unsigned char)(ulTmp>>24);
                pstVar->pucValue[1] = (unsigned char)(ulTmp>>16);
                pstVar->pucValue[2] = (unsigned char)(ulTmp>>8);
                pstVar->pucValue[3] = (unsigned char)(ulTmp);
                pstVar->ulLen = 4;
            }
        }
        else if (pstVar->ucType == ASN_INTEGER)
        {
            if (pstVar->ulLen != 4)
            {
                return -1;
            }

            DATA_DUP(pstVar, pstSetVar[i].pucReqData, 4);
            ulTmp = *((unsigned int*)pstVar->pucValue);
            if (ulTmp <= 0x7f)
            {
                pstVar->pucValue[0] = (unsigned char)ulTmp;
                pstVar->ulLen = 1;
            }
            else if (ulTmp <= 0x7fff)
            {
                pstVar->pucValue[0] = (unsigned char)(ulTmp>>8);
                pstVar->pucValue[1] = (unsigned char)(ulTmp);
                pstVar->ulLen = 2;
            }
            else if (ulTmp <= 0x7fffff)
            {
                pstVar->pucValue[0] = (unsigned char)(ulTmp>>16);
                pstVar->pucValue[1] = (unsigned char)(ulTmp>>8);
                pstVar->pucValue[2] = (unsigned char)(ulTmp);
                pstVar->ulLen = 3;
            }
            else
            {
                pstVar->pucValue[0] = (unsigned char)(ulTmp>>24);
                pstVar->pucValue[1] = (unsigned char)(ulTmp>>16);
                pstVar->pucValue[2] = (unsigned char)(ulTmp>>8);
                pstVar->pucValue[3] = (unsigned char)(ulTmp);
                pstVar->ulLen = 4;
            }
        }
        SET_LEN_VAR(pstVar);

        /* OID */
        pstVar = &pstPDU->pstPara[i].stOID;
        pstVar->ucType = ASN_OBJECT_ID;
        if (snmp_create_oid_var(pstSetVar[i].astOID, pstSetVar[i].ulOIDSize, &aucOID[0], &ucOIDLen) < 0)
        {
            snmp_log(LOG_ERR, "snmp_init_getbulk_pdu: create oid var failed");
            return -1;
        }
        pstVar->ulLen = ucOIDLen;
        DATA_DUP(pstVar, aucOID, ucOIDLen);
        SET_LEN_VAR(pstVar);

        /* para */
        pstVar = &pstPDU->pstPara[i].stPara;
        pstVar->ucType = SNMP_ID_SEQUENCE;
        pstVar->ulLen = TLV_LEN(&pstPDU->pstPara[i].stOID) +
                        TLV_LEN(&pstPDU->pstPara[i].stValue);
        SET_LEN_VAR(pstVar);

        ulParaLen += TLV_LEN(pstVar);
    }

    /* paras */
    pstVar = &pstPDU->stParas;
    pstVar->ucType = SNMP_ID_SEQUENCE;
    pstVar->ulLen = ulParaLen;
    SET_LEN_VAR(pstVar);

    /* error index */
    pstVar = &pstPDU->stErrIndex;
    pstVar->ucType = ASN_INTEGER;
    pstVar->ulLen = 1;
    DATA_DUP(pstVar, &ucErrorIndex, sizeof(ucErrorIndex));
    SET_LEN_VAR(pstVar);

    /* error status */
    pstVar = &pstPDU->stErrStatus;
    pstVar->ucType = ASN_INTEGER;
    pstVar->ulLen = 1;
    DATA_DUP(pstVar, &ucErrorStatus, sizeof(ucErrorStatus));
    SET_LEN_VAR(pstVar);

    /* req id */
    pstVar = &pstPDU->stReqID;
    pstVar->ucType = ASN_INTEGER;
    usReqIDValue = snmp_get_req_id();
    pstVar->ulLen = snmp_set_req_id(usReqIDValue, aucReqID);

    DATA_DUP(pstVar, aucReqID, pstVar->ulLen);
    SET_LEN_VAR(pstVar);

    /* snmp pdu */
    pstVar = &pstPDU->stSnmpPDU;
    pstVar->ucType = ucSetType;
    pstVar->ulLen = TLV_LEN(&pstPDU->stReqID) +
                    TLV_LEN(&pstPDU->stErrStatus) +
                    TLV_LEN(&pstPDU->stErrIndex) +
                    TLV_LEN(&pstPDU->stParas);
    SET_LEN_VAR(pstVar);

    //community
    pstVar = &pstPDU->stCommunity;
    pstVar->ucType = ASN_OCTET_STR;
    pstVar->ulLen = strlen(szCommunity);
    if (pstVar->ulLen > SNMP_MAX_COMMUNITY_LEN)
    {
        return -1;
    }
    pstVar->pucValue = (unsigned char*)szCommunity;
    pstVar->ucNeedFree = 0;
    SET_LEN_VAR(pstVar);

    //version
    pstVar = &pstPDU->stVer;
    pstVar->ucType = ASN_INTEGER;
    pstVar->ulLen  = 1;
    DATA_DUP(pstVar, &ucVersion, sizeof(ucVersion));
    SET_LEN_VAR(pstVar);

    //PDU
    pstVar = &pstPDU->stPDU;
    pstVar->ucType = SNMP_ID_SEQUENCE;
    pstVar->ulLen  = TLV_LEN(&pstPDU->stVer) +
                     TLV_LEN(&pstPDU->stCommunity) +
                     TLV_LEN(&pstPDU->stSnmpPDU);
    SET_LEN_VAR(pstVar);

    return 0;
}

int snmp_create_setbulk_pdu(SNMP_SET_BULK_PDU_T *pstPDU, unsigned char *pucReqPDU,
                            unsigned int ulMaxReqPDULen, unsigned int *pulReqPDULen)
{
    unsigned char       *pucValue = pucReqPDU;
    unsigned int        i;
    unsigned int        ulCurrPDULen = 0;

    /* 根据结构生成PDU */
    INIT_VAR(&pstPDU->stPDU);
    INIT_VAR(&pstPDU->stVer);
    INIT_VAR(&pstPDU->stCommunity);
    INIT_VAR(&pstPDU->stSnmpPDU);
    INIT_VAR(&pstPDU->stReqID);
    INIT_VAR(&pstPDU->stErrStatus);
    INIT_VAR(&pstPDU->stErrIndex);
    INIT_VAR(&pstPDU->stParas);

    for (i=0; i<pstPDU->ulOIDNum; i++)
    {
        INIT_VAR(&pstPDU->pstPara[i].stPara);
        INIT_VAR(&pstPDU->pstPara[i].stOID);
        INIT_VAR(&pstPDU->pstPara[i].stValue);
    }

    *pulReqPDULen = TLV_LEN(&pstPDU->stPDU);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_parse_setbulk_rsp
* 功    能: 解析set bulk应答报文
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_parse_setbulk_rsp(SNMP_SET_BULK_PDU_T *pstPDU, unsigned char ucGetVer,
                           unsigned char *pucRspData, unsigned int ulRspDataLen)
{
    unsigned int    ulPDULen;
    unsigned char   ucErrorStatus;
    unsigned char   ucTmp;

    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check snmp pdu failed");
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        ulPDULen = snmp_get_int_value(pucRspData, ucTmp);
        pucRspData += ucTmp;

        if (ulRspDataLen != (1+1+ucTmp+ulPDULen))
        {
            snmp_log(LOG_ERR, "snmp_parse_set_rsp: check snmp pdu len failed");
            return -1;
        }
    }
    else
    {
        ulPDULen = pucRspData[0];
        pucRspData ++;

        if (ulRspDataLen != (1+1+ulPDULen))
        {
            snmp_log(LOG_ERR, "snmp_parse_set_rsp: check snmp pdu len failed");
            return -1;
        }
    }

    //ver check
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1 ||
        pucRspData[2] != ucGetVer)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check ver failed");
        return -1;
    }
    pucRspData += (1+1+1);

    //community
    if (pucRspData[0] != ASN_OCTET_STR ||
        pucRspData[1] == 0)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check community failed");
        return -1;
    }
    pucRspData += (1+1+pucRspData[1]);

    //snmp pdu type : get response
    if ((*pucRspData++) != SNMP_MSG_RESPONSE)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check snmp pdu type failed");
        return -1;
    }

    //snmp pdu len
    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //req id
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] == 0)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check reqid failed");
        return -1;
    }

    ucTmp = pucRspData[1];
    pucRspData += 2;
    CHECK_REQ_ID(pstPDU->stReqID, pucRspData)
//if (pstPDU->stReqID.pucValue &&
//        memcmp(pucRspData, pstPDU->stReqID.pucValue, ucTmp) != 0)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check reqid failed");
        return -1;
    }
    pucRspData += ucTmp;

    //error status
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check error status failed");
        return -1;
    }

    pucRspData += 2;
    ucErrorStatus = *pucRspData++;
    if (ucErrorStatus != SNMP_ERR_NOERROR)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: snmp set failed, error status is %d", ucErrorStatus);
        return -2;
    }

    //error index
    pucRspData += (1+1+1);

#if 0
    //paras
    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check paras failed");
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //para1
    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check para failed");
        return -1;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        pucRspData += ucTmp;
    }
    else
    {
        pucRspData ++;
    }

    //oid type
    if (pucRspData[0] != ASN_OBJECT_ID ||
        pucRspData[1] == 0)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check oid failed");
        return -1;
    }

    ucOIDSize = pucRspData[1];
    pucRspData += 2;
    if (ucOIDSize != pstPDU->stOID.ulLen &&
        ucOIDSize != (pstPDU->stOID.ulLen+1))
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check oid size failed");
        return -1;
    }

    if (pstPDU->stOID.pucValue &&
        memcmp(pucRspData, pstPDU->stOID.pucValue, pstPDU->stOID.ulLen) != 0)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check oid value failed");
        return -1;
    }

    pucRspData += ucOIDSize;

    //value type
    ucRspDataType = *pucRspData++;
    if (ucRspDataType != pstPDU->stValue.ucType)
    {
        snmp_log(LOG_ERR, "snmp_parse_set_rsp: check set data type(%d <-> %d) failed",
                     ucRspDataType, pstPDU->stValue.ucType);
        return -1;
    }

    //octet string len
    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        ulDataLen = snmp_get_int_value(pucRspData, ucTmp);
        pucRspData += ucTmp;
    }
    else
    {
        ulDataLen = pucRspData[0];
        pucRspData ++;
    }
#endif

    return 0;
}

/*************************************************************************
* 函数名称: snmp_set_bulk
* 功    能: 实现snmp set bulk操作
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_set_bulk(SNMP_SESSION_T *pstSession, SNMP_SET_VAR_T *pstSetVar, unsigned int ulColumnNum)
{
    int             iRet;
    unsigned int    ulReqMsgLen = 0;
    unsigned int    ulRecvMsgLen;
    unsigned char   *pBuf = NULL;
    unsigned char   *pucReqMsg;
    unsigned int    ulBufLen = 64*1024;
    SNMP_SET_BULK_PDU_T stPDU = {0};

    if (!pstSession)
    {
        return -1;
    }

    if (snmp_init_socket() < 0)
    {
        return -1;
    }

    memset(&stPDU, 0, sizeof(stPDU));

    iRet = snmp_init_setbulk_pdu(pstSession->acWriteCommunity, (UINT8)pstSession->ulSetVer, ulColumnNum, pstSetVar, &stPDU);
    if (iRet < 0)
    {
        goto fail;
    }

    pBuf = snmp_malloc(ulBufLen*2);
    if (NULL == pBuf)
    {
        goto fail;
    }
    pucReqMsg = pBuf + ulBufLen;

    iRet = snmp_create_setbulk_pdu(&stPDU, pucReqMsg, ulBufLen-1, &ulReqMsgLen);
    if (iRet < 0)
    {
        goto fail;
    }

    iRet = snmp_udp_send(pstSession,
                         pucReqMsg, ulReqMsgLen,
                         pBuf, ulBufLen, &ulRecvMsgLen, &stPDU.stReqID);
    if (iRet < 0)
    {
        goto fail;
    }

    iRet = snmp_parse_setbulk_rsp(&stPDU, (UINT8)pstSession->ulSetVer, pBuf, ulRecvMsgLen);
    if (iRet < 0)
    {
        goto fail;
    }

    goto end;

fail:
    iRet = -1;

end:

    if (pBuf)
    {
        snmp_free(pBuf);
    }

    snmp_free_setbulk_pdu(&stPDU);

    return iRet;
}

/*************************************************************************
* 函数名称: snmp_set_data
* 功    能: 实现snmp set操作
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明: 目前仅支持一个OID的set操作
*************************************************************************/
int snmp_set_data(SNMP_SESSION_T *pstSession, SNMP_OID *pOID,
                  unsigned int ulOIDSize, unsigned char ucDataType,
                  unsigned int ulDataLen, void *pData)
{
    int             iRet;
    SNMP_SET_PDU_T  stPDU;
    unsigned int    ulRecvMsgLen;
    unsigned char   *pBuf = NULL;
    unsigned int    ulBufLen = 64*1024;
    unsigned int    ulReqPDULen;
    unsigned char   *pucReqPDU;
    unsigned char   *pucRspData = NULL;

    if (!pstSession)
    {
        snmp_log(LOG_ERR, "snmp_set_data: pstSession is null");
        return -1;
    }

    pBuf = (unsigned char *)snmp_malloc(ulBufLen);
    if (NULL == pBuf)
    {
        snmp_log(LOG_ERR, "snmp_set_data: snmp_malloc failed");
        return -1;
    }

    memset(&stPDU, 0, sizeof(stPDU));
    iRet = snmp_init_set_pdu(pstSession->acWriteCommunity, (UINT8)pstSession->ulSetVer, pOID, ulOIDSize,
                             (unsigned char *)pData, ucDataType, ulDataLen, &stPDU);
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_set_data: snmp_init_set_pdu failed");
        goto fail;
    }

    pucReqPDU = pBuf;
    iRet = snmp_create_set_pdu(&stPDU, pucReqPDU, ulBufLen, &ulReqPDULen);
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_set_data: snmp_create_set_pdu failed");
        goto fail;
    }

    if (snmp_init_socket() < 0)
    {
        snmp_log(LOG_ERR, "snmp_set_data: snmp_init_socket failed");
        goto fail;
    }

    if (pstSession->ulTimeout == 0)
    {
        pucRspData = NULL;
    }
    else
    {
        pucRspData = pBuf;
    }

    iRet = snmp_udp_send(pstSession, //->aucAgentIP, pstSession->usAgentPort, pstSession->ulTimeout,
                         pucReqPDU, ulReqPDULen,
                         pucRspData, ulBufLen, &ulRecvMsgLen,
                         &(stPDU.stReqID));
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_set_data: snmp_udp_send failed");
        goto fail;
    }

    if (pstSession->ulTimeout > 0)
    {
        /* 根据reqID和OID进行检查 */
        iRet = snmp_parse_set_rsp(&stPDU, (UINT8)pstSession->ulSetVer, pBuf, ulRecvMsgLen);
        if (iRet < 0)
        {
            snmp_log(LOG_ERR, "snmp_set_data: snmp_parse_set_rsp failed");
            goto fail;
        }
    }

    snmp_free(pBuf);
    snmp_free_set_pdu(&stPDU);

    return 0;

fail:
    if (pBuf)
    {
        snmp_free(pBuf);
    }

    snmp_free_set_pdu(&stPDU);

    return -1;
}

/*************************************************************************
* 函数名称: snmp_send_trap
* 功    能: 实现snmp inform操作
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_send_trap(SOCKET stSock, unsigned char *pucServerIP, unsigned short usServerPort,
                   char *szCommunity, unsigned int ulTimeout, unsigned int ulTrapType, SNMP_VAR_T *pstVarList)
{
    int                 iRet;
    SNMP_TRAP2_PDU_T    stPDU = {0};
    SOCKADDR_IN         stSockAddr = {0};
    unsigned char       *pBuf = NULL;
    unsigned int        ulBufLen = 64*1024;
    unsigned int        ulReqPDULen;
    unsigned char       *pucReqPDU;

    pBuf = (unsigned char *)snmp_malloc(ulBufLen);
    if (NULL == pBuf)
    {
        snmp_log(LOG_ERR, "snmp_send_trap: snmp_malloc failed");
        return -1;
    }

    memset(&stPDU, 0, sizeof(stPDU));
    stPDU.pstVarList = pstVarList;
    iRet = snmp_init_trap2_pdu(szCommunity, ulTrapType, &stPDU);
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_trap: snmp_init_set_pdu failed");
        goto fail;
    }

    pucReqPDU = pBuf;
    iRet = snmp_create_trap2_pdu(&stPDU, pucReqPDU, ulBufLen, &ulReqPDULen);
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_trap: snmp_create_trap2_pdu failed");
        goto fail;
    }

    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port   = htons(usServerPort);
    memcpy(&stSockAddr.sin_addr.s_addr, pucServerIP, 4);

    iRet = sendto(stSock, (const char*)pucReqPDU, ulReqPDULen, 0, (struct sockaddr*)&stSockAddr, sizeof(stSockAddr));
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_trap: sendto() failed");
        goto fail;
    }

    snmp_free(pBuf);
    snmp_free_trap2_pdu(&stPDU);

    return 0;

fail:
    if (pBuf)
    {
        snmp_free(pBuf);
    }

    snmp_free_trap2_pdu(&stPDU);

    return -1;
}

int _snmp_send_trap(unsigned char *pucServerIP, unsigned short usServerPort, char *szCommunity, unsigned int ulTimeout, unsigned int ulTrapType, SNMP_VAR_T *pstVarList)
{
    SOCKET          stSock;
    int             iRet;

    if (snmp_init_socket() < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_trap: snmp_init_socket failed");
        return -1;
    }

    stSock = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (stSock == INVALID_SOCKET)
    {
        snmp_log(LOG_ERR, "snmp_send_trap: create socket failed");
        return -1;
    }

    iRet = snmp_send_trap(stSock, pucServerIP, usServerPort, szCommunity, ulTimeout, ulTrapType, pstVarList);
    CLOSE_SOCKET(stSock);

    return iRet;
}

int snmp_send_trap2(unsigned char *pucServerIP, unsigned short usServerPort, char *szCommunity, unsigned int ulTimeout, SNMP_VAR_T *pstVarList)
{
    unsigned int    ulTrapType = SNMP_MSG_TRAP2;

    return _snmp_send_trap(pucServerIP, usServerPort, szCommunity, ulTimeout, ulTrapType, pstVarList);
}

int snmp_send_inform(unsigned char *pucServerIP, unsigned short usServerPort, char *szCommunity, unsigned int ulTimeout, SNMP_VAR_T *pstVarList)
{
    unsigned int    ulTrapType = SNMP_MSG_INFORM;

    return _snmp_send_trap(pucServerIP, usServerPort, szCommunity, ulTimeout, ulTrapType, pstVarList);
}

/*************************************************************************
* 函数名称: snmp_get_asn1_type
* 功    能: 获取NET-SNMP的ANS.1数据类型
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
unsigned char snmp_get_asn1_type(unsigned char ucType)
{
    switch(ucType)
    {
        case TYPE_NULL:
            return ASN_NULL;

        case TYPE_CHAR:
            return ASN_OCTET_STR;

        case TYPE_BYTE:
        case TYPE_SHORT:
        case TYPE_LONG:
            return ASN_GAUGE;

        case TYPE_INT:
            return ASN_INTEGER;

        case TYPE_CNT:
            return ASN_COUNTER;

        case TYPE_CNT64:
            return ASN_COUNTER64;

        case TYPE_BIN:
            return ASN_OCTET_STR;

        case TYPE_IP:
            return ASN_IPADDRESS;

        case TYPE_MAC:
            return ASN_OCTET_STR;

        case TYPE_OID:
            return ASN_OBJECT_ID;

        case TYPE_TIME:
            return ASN_TIMETICKS;

        case TYPE_BOOL:
            return ASN_BOOLEAN;

        default:
            return ASN_OCTET_STR;

    }
}

void nms_get_data(UINT8 ucType, UINT32 ulLen, UINT8 *pucValue, VOID *pvOutData)
{
    UINT32      i;
    UINT32      *pulTmp;
    INT32       iTmp;
    UINT64      *pu64Tmp;

    if (ucType == ASN_GAUGE ||
        ucType == ASN_TIMETICKS ||
        ucType == ASN_INTEGER ||
        ucType == ASN_COUNTER )
    {
        pulTmp = (UINT32*)pvOutData;
        pulTmp[0] = 0;

        for (i=0; i<ulLen; i++)
        {
            pulTmp[0] = (pulTmp[0]<<8) + pucValue[i];
        }

        if (ucType == ASN_INTEGER)
        {
            iTmp = pulTmp[0];
            if (ulLen == 1 && iTmp >= 0x80)
            {
                pulTmp[0] = iTmp - 0x100;
            }
            else if (ulLen == 2 && iTmp >= 0x8000)
            {
                pulTmp[0] = iTmp - 0x10000;
            }
            else if (ulLen == 3 && iTmp >= 0x800000)
            {
                pulTmp[0] = iTmp - 0x1000000;
            }
        }
    }
    else if (ucType == ASN_COUNTER64)
    {
        pu64Tmp = (UINT64*)pvOutData;
        pu64Tmp[0] = 0;

        for (i=0; i<ulLen; i++)
        {
            pu64Tmp[0] = (pu64Tmp[0]<<8) + pucValue[i];
        }
    }
    else
    {
        memcpy(pvOutData, pucValue, ulLen);
    }
}

/*************************************************************************
* 函数名称: snmp_send_get_scalar_rsp
* 功    能: 发送get rsp
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明: 目前仅支持一个OID的set操作
*************************************************************************/
int snmp_send_get_scalar_rsp(SNMP_SESSION_T *pstSession, SNMP_OID *pOID, unsigned int ulOIDSize,
                             unsigned int ulReqID, unsigned char ucDataType, unsigned int ulDataLen, void *pData)
{
    int             iRet;
    SNMP_GET_SCALAR_RSP_PDU_T  stPDU;
    unsigned int    ulRecvMsgLen;
    unsigned char   *pBuf = NULL;
    unsigned int    ulBufLen = 64*1024;
    unsigned int    ulReqPDULen;
    unsigned char   *pucReqPDU;

    if (!pstSession)
    {
        snmp_log(LOG_ERR, "snmp_send_get_scalar_rsp: pstSession is null");
        return -1;
    }

    pBuf = (unsigned char *)snmp_malloc(ulBufLen);
    if (NULL == pBuf)
    {
        snmp_log(LOG_ERR, "snmp_send_get_scalar_rsp: snmp_malloc failed");
        return -1;
    }

    memset(&stPDU, 0, sizeof(stPDU));
    iRet = snmp_init_get_rsp_pdu(pstSession->acReadCommunity, (UINT8)pstSession->ulGetVer, pOID, ulOIDSize,
                                 ulReqID, (unsigned char *)pData, ucDataType, ulDataLen, &stPDU);
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_get_scalar_rsp: snmp_init_set_pdu failed");
        goto fail;
    }

    pucReqPDU = pBuf;
    iRet = snmp_create_get_rsp_pdu(&stPDU, pucReqPDU, ulBufLen, &ulReqPDULen);
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_get_scalar_rsp: snmp_create_set_pdu failed");
        goto fail;
    }

    if (snmp_init_socket() < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_get_scalar_rsp: snmp_init_socket failed");
        goto fail;
    }

    iRet = snmp_udp_send(pstSession,
                         pucReqPDU, ulReqPDULen,
                         NULL, ulBufLen, &ulRecvMsgLen,
                         &(stPDU.stReqID));
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_get_scalar_rsp: snmp_udp_send failed");
        goto fail;
    }

    snmp_free(pBuf);
    snmp_free_get_scalar_rsp_pdu(&stPDU);

    return 0;

fail:
    if (pBuf)
    {
        snmp_free(pBuf);
    }

    snmp_free_get_scalar_rsp_pdu(&stPDU);

    return -1;
}

int snmp_send_get_scalar_rsp_ex(SNMP_SESSION_T *pstSession, SOCKET stSock, SNMP_OID *pOID, unsigned int ulOIDSize,
                             unsigned int ulReqID, unsigned char ucDataType, unsigned int ulDataLen, void *pData)
{
    int             iRet;
    SNMP_GET_SCALAR_RSP_PDU_T  stPDU;
    unsigned char   *pBuf = NULL;
    unsigned int    ulBufLen = 64*1024;
    unsigned int    ulReqPDULen;
    unsigned char   *pucReqPDU;

    if (!pstSession)
    {
        snmp_log(LOG_ERR, "snmp_send_get_scalar_rsp: pstSession is null");
        return -1;
    }

    pBuf = (unsigned char *)snmp_malloc(ulBufLen);
    if (NULL == pBuf)
    {
        snmp_log(LOG_ERR, "snmp_send_get_scalar_rsp: snmp_malloc failed");
        return -1;
    }

    memset(&stPDU, 0, sizeof(stPDU));
    iRet = snmp_init_get_rsp_pdu(pstSession->acReadCommunity, (UINT8)pstSession->ulGetVer, pOID, ulOIDSize,
                                 ulReqID, (unsigned char *)pData, ucDataType, ulDataLen, &stPDU);
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_get_scalar_rsp: snmp_init_set_pdu failed");
        goto fail;
    }

    pucReqPDU = pBuf;
    iRet = snmp_create_get_rsp_pdu(&stPDU, pucReqPDU, ulBufLen, &ulReqPDULen);
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_get_scalar_rsp: snmp_create_set_pdu failed");
        goto fail;
    }

    if (snmp_init_socket() < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_get_scalar_rsp: snmp_init_socket failed");
        goto fail;
    }

    iRet = snmp_udp_send_ex(pstSession, stSock,pucReqPDU, ulReqPDULen);
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_get_scalar_rsp: snmp_udp_send failed");
        goto fail;
    }

    snmp_free(pBuf);
    snmp_free_get_scalar_rsp_pdu(&stPDU);

    return 0;

fail:
    if (pBuf)
    {
        snmp_free(pBuf);
    }

    snmp_free_get_scalar_rsp_pdu(&stPDU);

    return -1;
}

/*************************************************************************
* 函数名称: snmp_send_set_scalar_rsp
* 功    能: 发送set rsp
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明: 目前仅支持一个OID的set操作
*************************************************************************/
int snmp_send_set_scalar_rsp(SNMP_SESSION_T *pstSession, SNMP_OID *pOID, unsigned int ulOIDSize,
                             unsigned int ulReqID, unsigned char ucDataType, unsigned int ulDataLen, void *pData)
{
    int             iRet;
    SNMP_GET_SCALAR_RSP_PDU_T  stPDU;
    unsigned int    ulRecvMsgLen;
    unsigned char   *pBuf = NULL;
    unsigned int    ulBufLen = 64*1024;
    unsigned int    ulReqPDULen;
    unsigned char   *pucReqPDU;

    if (!pstSession)
    {
        snmp_log(LOG_ERR, "snmp_send_get_scalar_rsp: pstSession is null");
        return -1;
    }

    pBuf = (unsigned char *)snmp_malloc(ulBufLen);
    if (NULL == pBuf)
    {
        snmp_log(LOG_ERR, "snmp_send_get_scalar_rsp: snmp_malloc failed");
        return -1;
    }

    memset(&stPDU, 0, sizeof(stPDU));
    iRet = snmp_init_get_rsp_pdu(pstSession->acReadCommunity, (UINT8)pstSession->ulGetVer, pOID, ulOIDSize,
                                 ulReqID, (unsigned char *)pData, ucDataType, ulDataLen, &stPDU);
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_get_scalar_rsp: snmp_init_set_pdu failed");
        goto fail;
    }

    pucReqPDU = pBuf;
    iRet = snmp_create_get_rsp_pdu(&stPDU, pucReqPDU, ulBufLen, &ulReqPDULen);
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_get_scalar_rsp: snmp_create_set_pdu failed");
        goto fail;
    }

    if (snmp_init_socket() < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_get_scalar_rsp: snmp_init_socket failed");
        goto fail;
    }

    iRet = snmp_udp_send(pstSession,
                         pucReqPDU, ulReqPDULen,
                         NULL, ulBufLen, &ulRecvMsgLen,
                         &(stPDU.stReqID));
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_get_scalar_rsp: snmp_udp_send failed");
        goto fail;
    }

    snmp_free(pBuf);
    snmp_free_get_scalar_rsp_pdu(&stPDU);

    return 0;

fail:
    if (pBuf)
    {
        snmp_free(pBuf);
    }

    snmp_free_get_scalar_rsp_pdu(&stPDU);

    return -1;
}

/*************************************************************************
* 函数名称: snmp_init_batch_get_rsp_pdu
* 功    能: 生成get 操作报文
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_init_batch_get_rsp_pdu(unsigned char ucVersion, char *szCommunity, unsigned int ulReqID, SNMP_GET_RSP_PDU_T *pstPDU)
{
    SNMP_TLV_T          *pstTLV;
    unsigned char       aucReqID[4];
    unsigned char       ucErrorStatus = 0;
    unsigned char       ucErrorIndex  = 0;
    SNMP_VAR_T          *pstVar;
    unsigned int        ulParaLen = 0;

    for(pstVar=pstPDU->pstVarList; pstVar; pstVar=pstVar->pstNext)
    {
        ulParaLen += TLV_LEN(&pstVar->stPara);
    }

    if (ulParaLen == 0)
    {
        return -1;
    }

 /*   if (snmp_init_var(&pstPDU->stTimestamp, astTimestampOID, sizeof(astTimestampOID)/sizeof(SNMP_OID),
                      ASN_TIMETICKS, (unsigned char*)&ulTimeStamp, sizeof(ulTimeStamp)) < 0)
    {
        snmp_log(LOG_ERR, "snmp_init_batch_get_rsp_pdu: snmp_init_var of timestamp failed");
        return -1;
    }*/

    /* paras */
    pstTLV = &pstPDU->stParas;
    pstTLV->ucType = SNMP_ID_SEQUENCE;
    pstTLV->ulLen = ulParaLen;
    SET_LEN_VAR(pstTLV);

    /* error index */
    pstTLV = &pstPDU->stErrIndex;
    pstTLV->ucType = ASN_INTEGER;
    pstTLV->ulLen = 1;
    DATA_DUP(pstTLV, &ucErrorIndex, sizeof(ucErrorIndex));
    SET_LEN_VAR(pstTLV);

    /* error status */
    pstTLV = &pstPDU->stErrStatus;
    pstTLV->ucType = ASN_INTEGER;
    pstTLV->ulLen = 1;
    DATA_DUP(pstTLV, &ucErrorStatus, sizeof(ucErrorStatus));
    SET_LEN_VAR(pstTLV);

    /* req id */
    pstTLV = &pstPDU->stReqID;
    pstTLV->ucType = ASN_INTEGER;
    pstTLV->ulLen = snmp_set_req_id_ex(ulReqID, aucReqID);
    DATA_DUP(pstTLV, aucReqID, pstTLV->ulLen);
    SET_LEN_VAR(pstTLV);

    /* snmp pdu */
    pstTLV = &pstPDU->stSnmpPDU;
    pstTLV->ucType = SNMP_MSG_RESPONSE;
    pstTLV->ulLen = TLV_LEN(&pstPDU->stReqID) +
                    TLV_LEN(&pstPDU->stErrStatus) +
                    TLV_LEN(&pstPDU->stErrIndex) +
                    TLV_LEN(&pstPDU->stParas);
    SET_LEN_VAR(pstTLV);

    //community
    pstTLV = &pstPDU->stCommunity;
    pstTLV->ucType = ASN_OCTET_STR;
    pstTLV->ulLen = strlen(szCommunity);
    if (pstTLV->ulLen > SNMP_MAX_COMMUNITY_LEN)
    {
        return -1;
    }
    pstTLV->pucValue = (unsigned char*)szCommunity;
    pstTLV->ucNeedFree = 0;
    SET_LEN_VAR(pstTLV);

    //version
    pstTLV = &pstPDU->stVer;
    pstTLV->ucType = ASN_INTEGER;
    pstTLV->ulLen  = 1;
    DATA_DUP(pstTLV, &ucVersion, sizeof(ucVersion));
    SET_LEN_VAR(pstTLV);

    //PDU
    pstTLV = &pstPDU->stPDU;
    pstTLV->ucType = SNMP_ID_SEQUENCE;
    pstTLV->ulLen  = TLV_LEN(&pstPDU->stVer) +
                     TLV_LEN(&pstPDU->stCommunity) +
                     TLV_LEN(&pstPDU->stSnmpPDU);
    SET_LEN_VAR(pstTLV);

    return 0;
}

/*************************************************************************
* 函数名称: snmp_create_batch_get_rsp_pdu
* 功    能: 生成get rsp操作报文
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_create_batch_get_rsp_pdu(SNMP_GET_RSP_PDU_T *pstPDU, unsigned char *pucPDU,
                                  unsigned int ulMaxReqPDULen, unsigned int *pulPDULen)
{
    unsigned char       *pucValue = pucPDU;
    unsigned int        ulCurrPDULen = 0;
    SNMP_VAR_T          *pstVar;

    /* 根据结构生成PDU */
    INIT_VAR(&pstPDU->stPDU);
    INIT_VAR(&pstPDU->stVer);
    INIT_VAR(&pstPDU->stCommunity);
    INIT_VAR(&pstPDU->stSnmpPDU);
    INIT_VAR(&pstPDU->stReqID);
    INIT_VAR(&pstPDU->stErrStatus);
    INIT_VAR(&pstPDU->stErrIndex);
    INIT_VAR(&pstPDU->stParas);

    for(pstVar=pstPDU->pstVarList; pstVar; pstVar=pstVar->pstNext)
    {
        INIT_VAR(&pstVar->stPara);
        INIT_VAR(&pstVar->stOID);
        INIT_VAR(&pstVar->stValue);
    }

    *pulPDULen = TLV_LEN(&pstPDU->stPDU);

    return 0;
}


/*************************************************************************
* 函数名称: snmp_send_get_rsp
* 功    能: 实现snmp inform操作
* 参    数:
* 参数名称          输入/输出               描述
* 函数返回:
* 说    明:
*************************************************************************/
int snmp_send_get_rsp(SNMP_SESSION_T *pstSession, unsigned int ulReqID, SNMP_VAR_T *pstVarList)
{
    int                 iRet;
    UINT32              ulRecvMsgLen = 0;
    SNMP_GET_RSP_PDU_T  stPDU = {0};
    unsigned char       *pBuf = NULL;
    unsigned int        ulBufLen = 64*1024;
    unsigned int        ulReqPDULen;
    unsigned char       *pucReqPDU;

    pBuf = (unsigned char *)snmp_malloc(ulBufLen);
    if (NULL == pBuf)
    {
        snmp_log(LOG_ERR, "snmp_send_get_rsp: snmp_malloc failed");
        return -1;
    }

    memset(&stPDU, 0, sizeof(stPDU));
    stPDU.pstVarList = pstVarList;
    iRet = snmp_init_batch_get_rsp_pdu(pstSession->ulGetVer, pstSession->acReadCommunity, ulReqID, &stPDU);
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_get_rsp: snmp_init_set_pdu failed");
        goto fail;
    }

    pucReqPDU = pBuf;
    iRet = snmp_create_batch_get_rsp_pdu(&stPDU, pucReqPDU, ulBufLen, &ulReqPDULen);
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_get_rsp: snmp_create_trap2_pdu failed");
        goto fail;
    }

    iRet = snmp_udp_send(pstSession,
                         pucReqPDU, ulReqPDULen,
                         NULL, ulBufLen, &ulRecvMsgLen,
                         &(stPDU.stReqID));
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_get_rsp: snmp_udp_send() failed");
        goto fail;
    }

    snmp_free(pBuf);
    snmp_free_get_rsp_pdu(&stPDU);

    return 0;

fail:
    if (pBuf)
    {
        snmp_free(pBuf);
    }

    snmp_free_get_rsp_pdu(&stPDU);

    return -1;
}

int snmp_send_get_rsp_ex(SNMP_SESSION_T *pstSnmpSession, unsigned int stSock, UINT32 ulReqID, SNMP_VAR_T *pstVarList)
{
    int                 iRet;
    SNMP_GET_RSP_PDU_T  stPDU = {0};
    unsigned char       *pBuf = NULL;
    unsigned int        ulBufLen = 64*1024;
    unsigned int        ulReqPDULen;
    unsigned char       *pucReqPDU;

    pBuf = (unsigned char *)snmp_malloc(ulBufLen);
    if (NULL == pBuf)
    {
        snmp_log(LOG_ERR, "snmp_send_get_rsp: snmp_malloc failed");
        return -1;
    }

    memset(&stPDU, 0, sizeof(stPDU));
    stPDU.pstVarList = pstVarList;
    iRet = snmp_init_batch_get_rsp_pdu(pstSnmpSession->ulGetVer, pstSnmpSession->acReadCommunity, ulReqID, &stPDU);
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_get_rsp: snmp_init_set_pdu failed");
        goto fail;
    }

    pucReqPDU = pBuf;
    iRet = snmp_create_batch_get_rsp_pdu(&stPDU, pucReqPDU, ulBufLen, &ulReqPDULen);
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_get_rsp: snmp_create_trap2_pdu failed");
        goto fail;
    }

    iRet = snmp_udp_send_ex(pstSnmpSession, stSock, pucReqPDU, ulReqPDULen);
    if (iRet < 0)
    {
        snmp_log(LOG_ERR, "snmp_send_get_rsp: snmp_udp_send() failed");
        goto fail;
    }

fail:
    if (pBuf)
    {
        snmp_free(pBuf);
    }

    snmp_free_get_rsp_pdu(&stPDU);

    return iRet;
}
