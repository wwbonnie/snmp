#include "omc_public.h"
#include "TaskQuery.h"
#include "snmp_nms.h"
#include "oid.h"

extern SOCKET               g_stSnmpSocket;

#define CHECK_TIMER         TIMER10
#define EV_CHECK_TIMER      GET_TIMER_EV(CHECK_TIMER)

#define INIT_GET_OID(stReq, stOID, OID)\
{\
    memcpy(stOID.astOID, OID, sizeof(OID));\
    stOID.ulOIDNum = ARRAY_SIZE(OID);\
    stReq.vOID.push_back(stOID);\
}

UINT32      g_ulDevCheckPeriod = 15*1000;
UINT32      g_ulCurrNeIndex = 0;

extern OMC_LOCAL_CFG_T  g_stLocalCfg;

#define SET_LEN_VAR(x)          snmp_set_len_var(x)

#define TLV_LEN(x)  (1 + (x)->ucLenNum + (x)->ulLen)

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

#define INIT_VAR(x)                                     \
{                                                       \
    unsigned int    __i;                                \
    if ((++ulCurrPDULen) > ulMaxReqPDULen) return -1;   \
    *pucValue++ = (x)->ucType;                          \
    for (__i=0; __i<(x)->ucLenNum; __i++)               \
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

int SnmpSetReqID(unsigned int ulReqIDValue, unsigned char *pucReqID)
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

UINT32 SnmpGetSeqID()
{
    static UINT32   ulSeqID = 0;

    if (ulSeqID == 0)
    {
        ulSeqID = gos_get_current_time();
    }

    return ulSeqID++;
}

BOOL SnmpInitGetReqPDU(SNMP_GET_REQ_T &stReq, SNMP_BUF_T &stReqBuf)
{
    SNMP_GET_REQ_PDU_T  stPDU;
    SNMP_TLV_T          *pstTLV;
    UINT8               aucReqID[4];
    UINT8               ucErrorStatus = 0;
    UINT8               ucErrorIndex  = 0;
    UINT8               ucNoRepeaters  = 0;
    UINT32              ulPDULen = 0;
    UINT32              ulParaLen = 0;
    UINT32              ulCurrPDULen = 0;
    UINT32              ulMaxReqPDULen = stReqBuf.ulMaxLen;
    UINT8               *pucValue = stReqBuf.pucBuf;
    VECTOR<SNMP_VAR_T>  vVar;
    UINT32              i;
    BOOL                bRet = FALSE;

    memset(&stPDU, 0, sizeof(stPDU));

    if (stReq.vOID.size() == 0)
    {
        return FALSE;
    }

    for (i=0; i<stReq.vOID.size(); i++)
    {
        SNMP_VAR_T      stVar = {0};

        if (snmp_init_var(&stVar, stReq.vOID[i].astOID, stReq.vOID[i].ulOIDNum, ASN_NULL, NULL, 0) < 0)
        {
            goto end;
        }

        ulParaLen += TLV_LEN(&stVar.stPara);
        vVar.push_back(stVar);
    }

    /* paras */
    pstTLV = &stPDU.stParas;
    pstTLV->ucType = SNMP_ID_SEQUENCE;
    pstTLV->ulLen = ulParaLen;
    SET_LEN_VAR(pstTLV);

    if (stReq.ucMaxRepetitions <= 1)
    {
        /* error index */
        pstTLV = &stPDU.stErrIndex;
        pstTLV->ucType = ASN_INTEGER;
        pstTLV->ulLen = 1;
        DATA_DUP(pstTLV, &ucErrorIndex, sizeof(ucErrorIndex));
        SET_LEN_VAR(pstTLV);

        /* error status */
        pstTLV = &stPDU.stErrStatus;
        pstTLV->ucType = ASN_INTEGER;
        pstTLV->ulLen = 1;
        DATA_DUP(pstTLV, &ucErrorStatus, sizeof(ucErrorStatus));
        SET_LEN_VAR(pstTLV);
    }
    else
    {
        /* max repetitions */
        pstTLV = &stPDU.stErrIndex;
        pstTLV->ucType = ASN_INTEGER;
        pstTLV->ulLen = 1;
        DATA_DUP(pstTLV, &stReq.ucMaxRepetitions, sizeof(ucErrorIndex));
        SET_LEN_VAR(pstTLV);

        /* No  Repeaters */
        pstTLV = &stPDU.stErrStatus;
        pstTLV->ucType = ASN_INTEGER;
        pstTLV->ulLen = 1;
        DATA_DUP(pstTLV, &ucNoRepeaters, sizeof(ucNoRepeaters));
        SET_LEN_VAR(pstTLV);
    }

    /* req id */
    pstTLV = &stPDU.stReqID;
    pstTLV->ucType = ASN_INTEGER;
    pstTLV->ulLen = SnmpSetReqID(stReq.ulSeqID, aucReqID);
    DATA_DUP(pstTLV, aucReqID, pstTLV->ulLen);
    SET_LEN_VAR(pstTLV);

    /* snmp pdu */
    pstTLV = &stPDU.stSnmpPDU;
    pstTLV->ucType = stReq.ulGetType;
    pstTLV->ulLen = TLV_LEN(&stPDU.stReqID) +
                    TLV_LEN(&stPDU.stErrStatus) +
                    TLV_LEN(&stPDU.stErrIndex) +
                    TLV_LEN(&stPDU.stParas);
    SET_LEN_VAR(pstTLV);

    //community
    pstTLV = &stPDU.stCommunity;
    pstTLV->ucType = ASN_OCTET_STR;
    pstTLV->ulLen = strlen(stReq.acGetCommunity);
    pstTLV->pucValue = (UINT8*)snmp_strdup(stReq.acGetCommunity);
    pstTLV->ucNeedFree = TRUE;
    SET_LEN_VAR(pstTLV);

    //version
    pstTLV = &stPDU.stVer;
    pstTLV->ucType = ASN_INTEGER;
    pstTLV->ulLen  = 1;
    DATA_DUP(pstTLV, &stReq.ucGetVer, sizeof(stReq.ucGetVer));
    SET_LEN_VAR(pstTLV);

    //PDU
    pstTLV = &stPDU.stPDU;
    pstTLV->ucType = SNMP_ID_SEQUENCE;
    pstTLV->ulLen  = TLV_LEN(&stPDU.stVer) + TLV_LEN(&stPDU.stCommunity) + TLV_LEN(&stPDU.stSnmpPDU);
    SET_LEN_VAR(pstTLV);

    // ���ɷ��ͱ���
    /* ���ݽṹ����PDU */
    INIT_VAR(&stPDU.stPDU);
    INIT_VAR(&stPDU.stVer);
    INIT_VAR(&stPDU.stCommunity);
    INIT_VAR(&stPDU.stSnmpPDU);
    INIT_VAR(&stPDU.stReqID);
    INIT_VAR(&stPDU.stErrStatus);
    INIT_VAR(&stPDU.stErrIndex);
    INIT_VAR(&stPDU.stParas);

    for(i=0; i<vVar.size(); i++)
    {
        INIT_VAR(&vVar[i].stPara);
        INIT_VAR(&vVar[i].stOID);
        INIT_VAR(&vVar[i].stValue);
        
        snmp_free_tlv(&vVar[i].stPara);
        snmp_free_tlv(&vVar[i].stOID);
        snmp_free_tlv(&vVar[i].stValue); 
    }

    ulPDULen = TLV_LEN(&stPDU.stPDU);

    stReqBuf.ulLen = ulPDULen;

    bRet = TRUE;

end:
    snmp_free_tlv(&stPDU.stPDU);
    snmp_free_tlv(&stPDU.stVer);
    snmp_free_tlv(&stPDU.stCommunity);
    snmp_free_tlv(&stPDU.stSnmpPDU);
    snmp_free_tlv(&stPDU.stReqID);
    snmp_free_tlv(&stPDU.stErrStatus);
    snmp_free_tlv(&stPDU.stErrIndex);
    snmp_free_tlv(&stPDU.stParas);

    return bRet;
}

BOOL SnmpInitSetReqPDU(SNMP_SET_REQ_T &stReq, SNMP_BUF_T &stReqBuf)
{
    SNMP_SET_REQ_PDU_T  stPDU;
    SNMP_TLV_T          *pstTLV;
    UINT8               aucReqID[4];
    UINT8               ucErrorStatus = 0;
    UINT8               ucErrorIndex  = 0;
    UINT32              ulPDULen = 0;
    UINT32              ulParaLen = 0;

    UINT32              ulAsnType;
    UINT32              ulCurrPDULen = 0;
    UINT32              ulMaxReqPDULen = stReqBuf.ulMaxLen;
    UINT8               *pucValue = stReqBuf.pucBuf;
    VECTOR<SNMP_VAR_T>  vVar;
    UINT32              i;
    BOOL                bRet = FALSE;

    memset(&stPDU, 0, sizeof(stPDU));

    if (stReq.vData.size() == 0)
    {
        return FALSE;
    }
    
    for (i=0; i<stReq.vData.size(); i++)
    {
        SNMP_VAR_T      stVar = {0};
        SNMP_DATA_T     &stData = stReq.vData[i];

        ulAsnType = snmp_get_asn1_type(stData.ulDataType);
        if (snmp_init_var(&stVar, stData.astOID, stData.ulOIDSize, ulAsnType, (UINT8*)stData.szValue, stData.ulDataLen) < 0)
        {
            goto end;
        }

        ulParaLen += TLV_LEN(&stVar.stPara);
        vVar.push_back(stVar);
    }

    /* paras */
    pstTLV = &stPDU.stParas;
    pstTLV->ucType = SNMP_ID_SEQUENCE;
    pstTLV->ulLen = ulParaLen;
    SET_LEN_VAR(pstTLV);

    /* error index */
    pstTLV = &stPDU.stErrIndex;
    pstTLV->ucType = ASN_INTEGER;
    pstTLV->ulLen = 1;
    DATA_DUP(pstTLV, &ucErrorIndex, sizeof(ucErrorIndex));
    SET_LEN_VAR(pstTLV);

    /* error status */
    pstTLV = &stPDU.stErrStatus;
    pstTLV->ucType = ASN_INTEGER;
    pstTLV->ulLen = 1;
    DATA_DUP(pstTLV, &ucErrorStatus, sizeof(ucErrorStatus));
    SET_LEN_VAR(pstTLV);

    /* req id */
    pstTLV = &stPDU.stReqID;
    pstTLV->ucType = ASN_INTEGER;
    pstTLV->ulLen = SnmpSetReqID(stReq.ulSeqID, aucReqID);
    DATA_DUP(pstTLV, aucReqID, pstTLV->ulLen);
    SET_LEN_VAR(pstTLV);

    /* snmp pdu */
    pstTLV = &stPDU.stSnmpPDU;
    pstTLV->ucType = stReq.ulSetType;
    pstTLV->ulLen = TLV_LEN(&stPDU.stReqID) +
        TLV_LEN(&stPDU.stErrStatus) +
        TLV_LEN(&stPDU.stErrIndex) +
        TLV_LEN(&stPDU.stParas);
    SET_LEN_VAR(pstTLV);

    //community
    pstTLV = &stPDU.stCommunity;
    pstTLV->ucType = ASN_OCTET_STR;
    pstTLV->ulLen = strlen(stReq.acSetCommunity);
    pstTLV->pucValue = (UINT8*)snmp_strdup(stReq.acSetCommunity);
    pstTLV->ucNeedFree = TRUE;
    SET_LEN_VAR(pstTLV);

    //version
    pstTLV = &stPDU.stVer;
    pstTLV->ucType = ASN_INTEGER;
    pstTLV->ulLen  = 1;
    DATA_DUP(pstTLV, &stReq.ucSetVer, sizeof(stReq.ucSetVer));
    SET_LEN_VAR(pstTLV);

    //PDU
    pstTLV = &stPDU.stPDU;
    pstTLV->ucType = SNMP_ID_SEQUENCE;
    pstTLV->ulLen  = TLV_LEN(&stPDU.stVer) + TLV_LEN(&stPDU.stCommunity) + TLV_LEN(&stPDU.stSnmpPDU);
    SET_LEN_VAR(pstTLV);

    // ���ɷ��ͱ���
    /* ���ݽṹ����PDU */
    INIT_VAR(&stPDU.stPDU);
    INIT_VAR(&stPDU.stVer);
    INIT_VAR(&stPDU.stCommunity);
    INIT_VAR(&stPDU.stSnmpPDU);
    INIT_VAR(&stPDU.stReqID);
    INIT_VAR(&stPDU.stErrStatus);
    INIT_VAR(&stPDU.stErrIndex);
    INIT_VAR(&stPDU.stParas);

    for(i=0; i<vVar.size(); i++)
    {
        INIT_VAR(&vVar[i].stPara);
        INIT_VAR(&vVar[i].stOID);
        INIT_VAR(&vVar[i].stValue);

        snmp_free_tlv(&vVar[i].stPara);
        snmp_free_tlv(&vVar[i].stOID);
        snmp_free_tlv(&vVar[i].stValue);
    }

    ulPDULen = TLV_LEN(&stPDU.stPDU);

    stReqBuf.ulLen = ulPDULen;

    bRet = TRUE;

end:
    snmp_free_tlv(&stPDU.stPDU);
    snmp_free_tlv(&stPDU.stVer);
    snmp_free_tlv(&stPDU.stCommunity);
    snmp_free_tlv(&stPDU.stSnmpPDU);
    snmp_free_tlv(&stPDU.stReqID);
    snmp_free_tlv(&stPDU.stErrStatus);
    snmp_free_tlv(&stPDU.stErrIndex);
    snmp_free_tlv(&stPDU.stParas);

    for (i=0; i<stReq.vData.size(); i++)
    {
        snmp_free(stReq.vData[i].szValue);
    }

    return bRet;
}

// ���� snmp get_request ����
BOOL SendSnmpGetReq(SNMP_GET_REQ_T &stReq, CHAR *szNeID, IP_ADDR_T &stClientAddr)
{
    SNMP_BUF_T          stReqBuf;
    BOOL                bRet = FALSE;
    SOCKADDR_IN         stClientSockAddr = {0};

    memset(&stReqBuf, 0, sizeof(stReqBuf));

    /*
    if (!GetNEAddr(szNeID, stClientAddr))
    {
        GosLog(LOG_ERROR, "unknown ne %s", szNeID);
        return FALSE;
    } */

    stClientSockAddr.sin_family = PF_INET;
    stClientSockAddr.sin_port   = htons(stClientAddr.usPort);
    memcpy(&stClientSockAddr.sin_addr.s_addr, stClientAddr.aucIP, 4);

    stReqBuf.ulLen = 0;
    stReqBuf.ulMaxLen = 64*1024;
    stReqBuf.pucBuf = (UINT8 *)snmp_malloc(stReqBuf.ulMaxLen);
    if (NULL == stReqBuf.pucBuf)
    {
        GosLog(LOG_ERROR, "SendSnmpGetReq: malloc failed");
        return FALSE;
    }

    if (!SnmpInitGetReqPDU(stReq, stReqBuf))
    {
        GosLog(LOG_ERROR, "SendSnmpGetReq failed, init PDU failed");
        goto end;
    }

    if (!gos_udp_send(g_stSnmpSocket, stReqBuf.pucBuf, stReqBuf.ulLen, &stClientSockAddr))
    {
        GosLog(LOG_ERROR, "SendSnmpGetReq %s failed: %s", szNeID, gos_get_socket_err());
        goto end;
    }

    bRet = TRUE;

end:
    snmp_free(stReqBuf.pucBuf);

    //PrintSnmpMemPool();

    return bRet;
}

// ���� snmp set_request ��
BOOL SendSnmpSetReq(SNMP_SET_REQ_T &stReq, CHAR *szNeID, IP_ADDR_T &stClientAddr)
{
    SNMP_BUF_T          stReqBuf;
    BOOL                bRet = FALSE;
    SOCKADDR_IN         stClientSockAddr = {0};

    memset(&stReqBuf, 0, sizeof(stReqBuf));

    stClientSockAddr.sin_family = PF_INET;
    stClientSockAddr.sin_port   = htons(stClientAddr.usPort);
    memcpy(&stClientSockAddr.sin_addr.s_addr, stClientAddr.aucIP, 4);

    stReqBuf.ulLen = 0;
    stReqBuf.ulMaxLen = 64*1024;
    stReqBuf.pucBuf = (UINT8 *)snmp_malloc(stReqBuf.ulMaxLen);
    if (NULL == stReqBuf.pucBuf)
    {
        GosLog(LOG_ERROR, "SendSnmpSetReq: malloc failed");
        return FALSE;
    }

    if (!SnmpInitSetReqPDU(stReq, stReqBuf))
    {
        GosLog(LOG_ERROR, "SendSnmpSetReq failed, init PDU failed");
        goto end;
    }

    if (!gos_udp_send(g_stSnmpSocket, stReqBuf.pucBuf, stReqBuf.ulLen, &stClientSockAddr))
    {
        GosLog(LOG_ERROR, "SendSnmpSetReq failed: %s", gos_get_socket_err());
        goto end;
    }

    bRet = TRUE;

end:
    snmp_free(stReqBuf.pucBuf);

    //PrintSnmpMemPool();

    return bRet;
}

TaskQuery::TaskQuery(UINT16 usDispatchID):GBaseTask(1002, "Query", usDispatchID, FALSE)
{
//  m_bGetCfg = FALSE;
    m_pDao = NULL;
}

BOOL TaskQuery::Init()
{
    if (!m_pDao)
    {
        m_pDao = GetOmcDao();
        if (!m_pDao)
        {
            TaskLog(LOG_ERROR, "get db dao failed");

            return FALSE;
        }
    }
    SetLoopTimer(CHECK_TIMER, g_ulDevCheckPeriod);
    SetTaskStatus(TASK_STATUS_WORK);

    TaskLog(LOG_INFO, "Task init successful!");

    return TRUE;
}

// ���ͻ�ȡ��Ԫ�Ļ�����Ϣ
VOID SendGetNeBasicInfoMsg(NE_BASIC_INFO_T &stInfo)
{
    SNMP_GET_REQ_T  stReq;
    OID_T           stOID;

    stReq.ucGetVer = g_stLocalCfg.ulSnmpVer;
    stReq.ulGetType = SNMP_MSG_GET;
    stReq.ulSeqID = SnmpGetSeqID();
    strcpy(stReq.acGetCommunity, g_stLocalCfg.acTrapCommunity);

    // ��Ԫ��ͬ�Ļ�����Ϣ
    INIT_GET_OID(stReq, stOID, oid_sysUptime);
    INIT_GET_OID(stReq, stOID, oid_CpuUsage);
    INIT_GET_OID(stReq, stOID, oid_MemUsage);
    INIT_GET_OID(stReq, stOID, oid_DiskUsage);
    INIT_GET_OID(stReq, stOID, oid_sysHardwareVer);
    INIT_GET_OID(stReq, stOID, oid_sysSoftwareVer);

    if (stInfo.ulDevType == OMC_DEV_TYPE_TX)
    {
        INIT_GET_OID(stReq, stOID, oid_CpuTemp);
        INIT_GET_OID(stReq, stOID, oid_sysRadioVer);
        INIT_GET_OID(stReq, stOID, oid_sysAndroidVer);
        INIT_GET_OID(stReq, stOID, oid_TCID);
    }
    else if(stInfo.ulDevType == OMC_DEV_TYPE_FX)
    {
        INIT_GET_OID(stReq, stOID, oid_CpuTemp);
        INIT_GET_OID(stReq, stOID, oid_sysRadioVer);
        INIT_GET_OID(stReq, stOID, oid_sysAndroidVer);
    }
    else if (stInfo.ulDevType == OMC_DEV_TYPE_TAU)
    {
        INIT_GET_OID(stReq, stOID, oid_softwareVersion);    // TAU �ϱ��汾��Ϣoid�ڶ�����ͬ
        INIT_GET_OID(stReq, stOID, oid_hardwareVersion);
        INIT_GET_OID(stReq, stOID, oid_CpuTemp);
        INIT_GET_OID(stReq, stOID, oid_DiskLogsaveUsage);
        INIT_GET_OID(stReq, stOID, oid_DiskPackageUsage);
        INIT_GET_OID(stReq, stOID, oid_DiskModuleUsage);
        INIT_GET_OID(stReq, stOID, oid_SIMIMSI);
    }
    else if (stInfo.ulDevType == OMC_DEV_TYPE_DIS ||
             stInfo.ulDevType == OMC_DEV_TYPE_REC)
    {
        INIT_GET_OID(stReq, stOID, oid_ClusterStatus);
    }

    SendSnmpGetReq(stReq, stInfo.acNeID, stInfo.stClientAddr);
}

//  ���� GET �� ��ȡ��Ԫ��״̬��Ϣ
VOID SendGetNeStatusMsg(NE_BASIC_INFO_T &stInfo)
{
    SNMP_GET_REQ_T  stReq;
    OID_T           stOID;

    stReq.ucGetVer = g_stLocalCfg.ulSnmpVer;
    stReq.ulGetType = SNMP_MSG_GET;
    stReq.ulSeqID = SnmpGetSeqID();
    strcpy(stReq.acGetCommunity, g_stLocalCfg.acTrapCommunity);

    if (stInfo.ulDevType == OMC_DEV_TYPE_DIS ||
        stInfo.ulDevType == OMC_DEV_TYPE_DC )
    {
        INIT_GET_OID(stReq, stOID, oid_ATSStatus);
        INIT_GET_OID(stReq, stOID, oid_NTPStatus);
        INIT_GET_OID(stReq, stOID, oid_LTEStatus);
    }
    else if (stInfo.ulDevType == OMC_DEV_TYPE_TX )
    {
        INIT_GET_OID(stReq, stOID, oid_GatewayReachable);
        INIT_GET_OID(stReq, stOID, oid_MDCRegStatus);
        INIT_GET_OID(stReq, stOID, oid_OMCLinkStatus);
        INIT_GET_OID(stReq, stOID, oid_DISLinkStatus);
        INIT_GET_OID(stReq, stOID, oid_SelfCheckStatus);
        INIT_GET_OID(stReq, stOID, oid_DevCoverStatus);
        INIT_GET_OID(stReq, stOID, oid_DriveMode);
        INIT_GET_OID(stReq, stOID, oid_EmergencyCallStatus);
        INIT_GET_OID(stReq, stOID, oid_EmergencyBraceStatus);
        INIT_GET_OID(stReq, stOID, oid_PISLinkStatus);
        INIT_GET_OID(stReq, stOID, oid_PALinkStatus);
        INIT_GET_OID(stReq, stOID, oid_NTPAddr);
        INIT_GET_OID(stReq, stOID, oid_NTPPort);
        INIT_GET_OID(stReq, stOID, oid_NTPSyncTime);
    }
    else if (stInfo.ulDevType == OMC_DEV_TYPE_FX )
    {
        INIT_GET_OID(stReq, stOID, oid_GatewayReachable);
        INIT_GET_OID(stReq, stOID, oid_MDCRegStatus);
        INIT_GET_OID(stReq, stOID, oid_OMCLinkStatus);
        INIT_GET_OID(stReq, stOID, oid_DISLinkStatus);
        INIT_GET_OID(stReq, stOID, oid_SelfCheckStatus);
        INIT_GET_OID(stReq, stOID, oid_EmergencyCallStatus);
        INIT_GET_OID(stReq, stOID, oid_NTPAddr);
        INIT_GET_OID(stReq, stOID, oid_NTPPort);
        INIT_GET_OID(stReq, stOID, oid_NTPSyncTime);
        INIT_GET_OID(stReq, stOID, oid_PALinkStatus);
    }
    else
    {
        return;
    }

    SendSnmpGetReq(stReq, stInfo.acNeID, stInfo.stClientAddr);
}

VOID SendGetLteStatusMsg(NE_BASIC_INFO_T &stInfo)
{
    SNMP_GET_REQ_T  stReq;
    OID_T           stOID;

    stReq.ucGetVer = g_stLocalCfg.ulSnmpVer;
    stReq.ulGetType = SNMP_MSG_GET;
    stReq.ulSeqID = SnmpGetSeqID();
    strcpy(stReq.acGetCommunity, g_stLocalCfg.acTrapCommunity);

    INIT_GET_OID(stReq, stOID, oid_LteRSSI);
    INIT_GET_OID(stReq, stOID, oid_LteRSRP);
    INIT_GET_OID(stReq, stOID, oid_LteRSRQ);
    INIT_GET_OID(stReq, stOID, oid_LteSINR);
    INIT_GET_OID(stReq, stOID, oid_LtePCI);

    // TX��FX�ϱ���վ��Ϣ
    if (stInfo.ulDevType == OMC_DEV_TYPE_TX ||
        stInfo.ulDevType == OMC_DEV_TYPE_FX)
    {
        INIT_GET_OID(stReq, stOID, oid_StationID);
    }

    SendSnmpGetReq(stReq, stInfo.acNeID, stInfo.stClientAddr);
}

//��ȡ������Ϣ
VOID SendGetTrafficMsg(NE_BASIC_INFO_T &stInfo)
{
    SNMP_GET_REQ_T  stReq;
    OID_T           stOID;

    stReq.ucGetVer = g_stLocalCfg.ulSnmpVer;
    stReq.ulGetType = SNMP_MSG_GET;
    stReq.ulSeqID = SnmpGetSeqID();
    strcpy(stReq.acGetCommunity, g_stLocalCfg.acTrapCommunity);

    INIT_GET_OID(stReq, stOID, oid_WANTxBytes);
    INIT_GET_OID(stReq, stOID, oid_WANRxBytes);
    INIT_GET_OID(stReq, stOID, oid_LAN1TxBytes);
    INIT_GET_OID(stReq, stOID, oid_LAN1RxBytes);
    INIT_GET_OID(stReq, stOID, oid_LAN2TxBytes);
    INIT_GET_OID(stReq, stOID, oid_LAN2RxBytes);
    INIT_GET_OID(stReq, stOID, oid_LAN3TxBytes);
    INIT_GET_OID(stReq, stOID, oid_LAN3RxBytes);
    INIT_GET_OID(stReq, stOID, oid_LAN4TxBytes);
    INIT_GET_OID(stReq, stOID, oid_LAN4RxBytes);

    SendSnmpGetReq(stReq, stInfo.acNeID, stInfo.stClientAddr);
}

// ʸ����
VOID SendGetOperLogMsg(NE_BASIC_INFO_T &stInfo, UINT32 ulOperLogIndex, BOOL bGetOperLogIndexDone)
{
    SNMP_GET_REQ_T  stReq;
    OID_T           stOID;

    if (!bGetOperLogIndexDone) // ��ȡIndex
    {
        stReq.ucGetVer = g_stLocalCfg.ulSnmpVer;
        stReq.ulGetType = SNMP_MSG_GETNEXT;
        stReq.ulSeqID = SnmpGetSeqID();
        strcpy(stReq.acGetCommunity, g_stLocalCfg.acTrapCommunity);

        memcpy(stOID.astOID, oid_OperLogIndex, sizeof(oid_OperLogIndex));
        stOID.ulOIDNum = ARRAY_SIZE(oid_OperLogIndex);
        stOID.astOID[stOID.ulOIDNum++] = ulOperLogIndex;
        stReq.vOID.push_back(stOID);

        SendSnmpGetReq(stReq, stInfo.acNeID, stInfo.stClientAddr);
    }
    else // ��ȡOperLog
    {
        stReq.ucGetVer = g_stLocalCfg.ulSnmpVer;
        stReq.ulGetType = SNMP_MSG_GET;
        stReq.ulSeqID = SnmpGetSeqID();
        strcpy(stReq.acGetCommunity, g_stLocalCfg.acTrapCommunity);

        memcpy(stOID.astOID, oid_DevOperation, sizeof(oid_DevOperation));
        stOID.ulOIDNum = ARRAY_SIZE(oid_DevOperation);
        stOID.astOID[stOID.ulOIDNum++] = ulOperLogIndex;
        stReq.vOID.push_back(stOID);

        memcpy(stOID.astOID, oid_DevOperTime, sizeof(oid_DevOperTime));
        stOID.ulOIDNum = ARRAY_SIZE(oid_DevOperTime);
        stOID.astOID[stOID.ulOIDNum++] = ulOperLogIndex;
        stReq.vOID.push_back(stOID);

        memcpy(stOID.astOID, oid_LastOperLogIndex, sizeof(oid_LastOperLogIndex));
        stOID.ulOIDNum = ARRAY_SIZE(oid_LastOperLogIndex);
        stOID.astOID[stOID.ulOIDNum++] = ulOperLogIndex;
        stReq.vOID.push_back(stOID);

        SendSnmpGetReq(stReq, stInfo.acNeID, stInfo.stClientAddr);
    }
}

// ʸ����
VOID SendGetFileListMsg(NE_BASIC_INFO_T &stInfo, UINT32 ulFileIndex, BOOL bGetFileIndexDone)
{
    SNMP_GET_REQ_T  stReq;
    OID_T           stOID;

    if (!bGetFileIndexDone) // ��ȡIndex
    {
        stReq.ucGetVer = g_stLocalCfg.ulSnmpVer;
        stReq.ulGetType = SNMP_MSG_GETNEXT;
        stReq.ulSeqID = SnmpGetSeqID();
        strcpy(stReq.acGetCommunity, g_stLocalCfg.acTrapCommunity);

        memcpy(stOID.astOID, oid_DevFileIndex, sizeof(oid_DevFileIndex));
        stOID.ulOIDNum = ARRAY_SIZE(oid_DevFileIndex);
        stOID.astOID[stOID.ulOIDNum++] = ulFileIndex;
        stReq.vOID.push_back(stOID);

        SendSnmpGetReq(stReq, stInfo.acNeID, stInfo.stClientAddr);
    }
    else // ��ȡOperLog
    {
        stReq.ucGetVer = g_stLocalCfg.ulSnmpVer;
        stReq.ulGetType = SNMP_MSG_GET;
        stReq.ulSeqID = SnmpGetSeqID();
        strcpy(stReq.acGetCommunity, g_stLocalCfg.acTrapCommunity);

        memcpy(stOID.astOID, oid_DevFileType, sizeof(oid_DevFileType));
        stOID.ulOIDNum = ARRAY_SIZE(oid_DevFileType);
        stOID.astOID[stOID.ulOIDNum++] = ulFileIndex;
        stReq.vOID.push_back(stOID);

        memcpy(stOID.astOID, oid_DevFileName, sizeof(oid_DevFileName));
        stOID.ulOIDNum = ARRAY_SIZE(oid_DevFileName);
        stOID.astOID[stOID.ulOIDNum++] = ulFileIndex;
        stReq.vOID.push_back(stOID);

        memcpy(stOID.astOID, oid_DevFileSize, sizeof(oid_DevFileSize));
        stOID.ulOIDNum = ARRAY_SIZE(oid_DevFileSize);
        stOID.astOID[stOID.ulOIDNum++] = ulFileIndex;
        stReq.vOID.push_back(stOID);

        memcpy(stOID.astOID, oid_DevFileTime, sizeof(oid_DevFileTime));
        stOID.ulOIDNum = ARRAY_SIZE(oid_DevFileTime);
        stOID.astOID[stOID.ulOIDNum++] = ulFileIndex;
        stReq.vOID.push_back(stOID);

        memcpy(stOID.astOID, oid_LastDevFileIndex, sizeof(oid_LastDevFileIndex));
        stOID.ulOIDNum = ARRAY_SIZE(oid_LastDevFileIndex);
        stOID.astOID[stOID.ulOIDNum++] = ulFileIndex;
        stReq.vOID.push_back(stOID);

        SendSnmpGetReq(stReq, stInfo.acNeID, stInfo.stClientAddr);
    }
}

// ʸ����
VOID SendGetNetInfoMsg(NE_BASIC_INFO_T &stInfo)
{
    SNMP_GET_REQ_T  stReq;
    OID_T           stOID;
//  UINT32          ulIndex = 1;

    stReq.ucGetVer = g_stLocalCfg.ulSnmpVer;
    stReq.ulGetType = SNMP_MSG_GETNEXT;
    stReq.ulSeqID = SnmpGetSeqID();
    strcpy(stReq.acGetCommunity, g_stLocalCfg.acTrapCommunity);
    /*
    memcpy(stOID.astOID, oid_ipAddress, sizeof(oid_ipAddress));
    stOID.ulOIDNum = ARRAY_SIZE(oid_ipAddress);
    stOID.astOID[stOID.ulOIDNum++] = ulIndex;
    stReq.vOID.push_back(stOID);

    memcpy(stOID.astOID, oid_ipNetMask, sizeof(oid_ipNetMask));
    stOID.ulOIDNum = ARRAY_SIZE(oid_ipNetMask);
    stOID.astOID[stOID.ulOIDNum++] = ulIndex;
    stReq.vOID.push_back(stOID);
    */
    memcpy(stOID.astOID, oid_ipGatewayAddress, sizeof(oid_ipGatewayAddress));
    stOID.ulOIDNum = ARRAY_SIZE(oid_ipGatewayAddress);
//  stOID.astOID[stOID.ulOIDNum++] = ulIndex;
    stReq.vOID.push_back(stOID);

    SendSnmpGetReq(stReq, stInfo.acNeID, stInfo.stClientAddr);
}

INT32* ToIntPtr(INT32 iValue)
{
    INT32   *piValue = (INT32*)snmp_malloc(sizeof(INT32));

    if (!piValue)
    {
        return NULL;
    }

    *piValue = iValue;
    return piValue;
}

VOID InitDevSetCommunity(NE_BASIC_INFO_T &stNeBasicInfo, CHAR *szSetCommunity)
{
    sprintf(szSetCommunity, stNeBasicInfo.acDevMac);
}

// omc����ʱ������onlineΪfalse���豸����ʱ������Ϊtrue
VOID SendSetManageStateMsg(CHAR *szNeID, BOOL bOnline)
{
    OMA_DEV_INFO_T  stDevInfo;
    SNMP_SET_REQ_T  stReq;
    SNMP_DATA_T     stData;
    INT32           iState = bOnline;

    if (!GetDevInfo(szNeID, stDevInfo))
    {
        GosLog(LOG_ERROR, "send set manage state failed, unknown ne %s", szNeID);
        return;
    }

    stReq.ucSetVer = g_stLocalCfg.ulSnmpVer;
    stReq.ulSetType = SNMP_MSG_SET;
    stReq.ulSeqID = SnmpGetSeqID();
    InitDevSetCommunity(stDevInfo.stNeBasicInfo, stReq.acSetCommunity);

    memcpy(stData.astOID, oid_sysManageState, sizeof(oid_sysManageState));
    stData.ulOIDSize = ARRAY_SIZE(oid_sysManageState);
    //stData.astOID[stData.ulOIDSize++] = 0; // set scalar������Ҫoid��0����
    stData.szValue = (CHAR*)ToIntPtr(iState);
    stData.ulDataType = TYPE_INT;
    stData.ulDataLen = sizeof(INT32);
    stReq.vData.push_back(stData);

    SendSnmpSetReq(stReq, szNeID, stDevInfo.stNeBasicInfo.stClientAddr);
}

VOID SendSetSwUpdateMsg(NE_BASIC_INFO_T &stNeBasicInfo, CHAR *szNewSwVer)
{
    SNMP_SET_REQ_T  stReq;
    SNMP_DATA_T     stData;
    INT32           iLoadFlag = 1; // download
    INT32           iFileType = 1; // software
    INT32           iTransProtocol = 1; // ftp
    INT32           iServerPort = g_stLocalCfg.usFtpPort;
    OMC_SW_INFO_T   stSwInfo;
    CHAR            acVerFile[256];
    CHAR            *szVerFile;

    if (!GetSwInfo(szNewSwVer, stSwInfo))
    {
        GosLog(LOG_ERROR, "unknown sw ver %s", szNewSwVer);
        return;
    }

    sprintf(acVerFile, "%s/%s", g_stLocalCfg.acFtpVerDir, stSwInfo.acSwFile);
    szVerFile = acVerFile + strlen(g_stLocalCfg.acFtpDir);

    stReq.ucSetVer = g_stLocalCfg.ulSnmpVer;
    stReq.ulSetType = SNMP_MSG_SET;
    stReq.ulSeqID = SnmpGetSeqID();
    InitDevSetCommunity(stNeBasicInfo, stReq.acSetCommunity);

    memcpy(stData.astOID, oid_FileTrans_LoadFlag, sizeof(oid_FileTrans_LoadFlag));
    stData.ulOIDSize = ARRAY_SIZE(oid_FileTrans_LoadFlag);
    stData.szValue = (CHAR*)ToIntPtr(iLoadFlag);
    stData.ulDataType = TYPE_INT;
    stData.ulDataLen = sizeof(INT32);
    stReq.vData.push_back(stData);

    memcpy(stData.astOID, oid_FileTrans_FileName, sizeof(oid_FileTrans_FileName));
    stData.ulOIDSize = ARRAY_SIZE(oid_FileTrans_FileName);
    stData.szValue = (CHAR*)snmp_strdup(szVerFile);
    stData.ulDataType = TYPE_CHAR;
    stData.ulDataLen = strlen(stData.szValue);
    stReq.vData.push_back(stData);

    memcpy(stData.astOID, oid_FileTrans_FileType, sizeof(oid_FileTrans_FileType));
    stData.ulOIDSize = ARRAY_SIZE(oid_FileTrans_FileType);
    stData.szValue = (CHAR*)ToIntPtr(iFileType);
    stData.ulDataType = TYPE_INT;
    stData.ulDataLen = sizeof(INT32);
    stReq.vData.push_back(stData);

    memcpy(stData.astOID, oid_FileTrans_TransProtocol, sizeof(oid_FileTrans_TransProtocol));
    stData.ulOIDSize = ARRAY_SIZE(oid_FileTrans_TransProtocol);
    stData.szValue = (CHAR*)ToIntPtr(iTransProtocol);
    stData.ulDataType = TYPE_INT;
    stData.ulDataLen = sizeof(INT32);
    stReq.vData.push_back(stData);

    memcpy(stData.astOID, oid_FileTrans_ServerAddr, sizeof(oid_FileTrans_ServerAddr));
    stData.ulOIDSize = ARRAY_SIZE(oid_FileTrans_ServerAddr);
    stData.szValue = (CHAR*)snmp_malloc(sizeof(g_stLocalCfg.aucFtpAddr));
    memcpy(stData.szValue, g_stLocalCfg.aucFtpAddr, sizeof(g_stLocalCfg.aucFtpAddr));
    stData.ulDataType = TYPE_IP;
    stData.ulDataLen = sizeof(g_stLocalCfg.aucFtpAddr);
    stReq.vData.push_back(stData);

    memcpy(stData.astOID, oid_FileTrans_ServerPort, sizeof(oid_FileTrans_ServerPort));
    stData.ulOIDSize = ARRAY_SIZE(oid_FileTrans_ServerPort);
    stData.szValue = (CHAR*)ToIntPtr(iServerPort);
    stData.ulDataType = TYPE_INT;
    stData.ulDataLen = sizeof(INT32);
    stReq.vData.push_back(stData);

    memcpy(stData.astOID, oid_FileTrans_ServerUserName, sizeof(oid_FileTrans_ServerUserName));
    stData.ulOIDSize = ARRAY_SIZE(oid_FileTrans_ServerUserName);
    stData.szValue = (CHAR*)snmp_strdup(g_stLocalCfg.acFtpUser);
    stData.ulDataType = TYPE_CHAR;
    stData.ulDataLen = strlen(stData.szValue);
    stReq.vData.push_back(stData);

    memcpy(stData.astOID, oid_FileTrans_ServerPasswd, sizeof(oid_FileTrans_ServerPasswd));
    stData.ulOIDSize = ARRAY_SIZE(oid_FileTrans_ServerPasswd);
    stData.szValue = (CHAR*)snmp_strdup(g_stLocalCfg.acFtpPwd);
    stData.ulDataType = TYPE_CHAR;
    stData.ulDataLen = strlen(stData.szValue);
    stReq.vData.push_back(stData);

    memcpy(stData.astOID, oid_FileTrans_FileVersion, sizeof(oid_FileTrans_FileVersion));
    stData.ulOIDSize = ARRAY_SIZE(oid_FileTrans_FileVersion);
    stData.szValue = (CHAR*)snmp_strdup(stSwInfo.acSwVer);
    stData.ulDataType = TYPE_CHAR;
    stData.ulDataLen = strlen(stData.szValue);
    stReq.vData.push_back(stData);

    SendSnmpSetReq(stReq, stNeBasicInfo.acNeID, stNeBasicInfo.stClientAddr);

    // update_command
    INT32   iCmdOn = 1; // COMMAND_ON

    stReq.vData.clear();
    stReq.ulSeqID = SnmpGetSeqID();

    memcpy(stData.astOID, oid_SoftwareUpdate, sizeof(oid_SoftwareUpdate));
    stData.ulOIDSize = ARRAY_SIZE(oid_SoftwareUpdate);
    stData.szValue = (CHAR*)ToIntPtr(iCmdOn);
    stData.ulDataType = TYPE_INT;
    stData.ulDataLen = sizeof(INT32);
    stReq.vData.push_back(stData);

    SendSnmpSetReq(stReq, stNeBasicInfo.acNeID, stNeBasicInfo.stClientAddr);
}

VOID SendSetUploadDevFileMsg(CHAR *szNeID, CHAR *szFile)
{
    OMA_DEV_INFO_T  stDevInfo;
    SNMP_SET_REQ_T  stReq;
    SNMP_DATA_T     stData;
    INT32           iLoadFlag = 2; // 2 upload
    INT32           iFileType = 2; // 2 config
    INT32           iTransProtocol = 1; // ftp
    INT32           iExportFile = 2; // export(2)
    INT32           iServerPort = g_stLocalCfg.usFtpPort;
    CHAR            acServerFile[256];

    if (!GetDevInfo(szNeID, stDevInfo))
    {
        GosLog(LOG_ERROR, "send upload file failed, unknown ne %s", szNeID);
        return;
    }

    sprintf(acServerFile, "%s/", g_stLocalCfg.acFtpLogFileDir+strlen(g_stLocalCfg.acFtpDir)+1);
    gos_get_file_rawname(szFile, acServerFile+strlen(acServerFile));

    stReq.ucSetVer = g_stLocalCfg.ulSnmpVer;
    stReq.ulSetType = SNMP_MSG_SET;
    stReq.ulSeqID = SnmpGetSeqID();
    //strcpy(stReq.acSetCommunity, g_stLocalCfg.acSetCommunity);
    InitDevSetCommunity(stDevInfo.stNeBasicInfo, stReq.acSetCommunity);

    memcpy(stData.astOID, oid_FileTrans_LoadFlag, sizeof(oid_FileTrans_LoadFlag));
    stData.ulOIDSize = ARRAY_SIZE(oid_FileTrans_LoadFlag);
    stData.szValue = (CHAR*)ToIntPtr(iLoadFlag);
    stData.ulDataType = TYPE_INT;
    stData.ulDataLen = sizeof(INT32);
    stReq.vData.push_back(stData);

    memcpy(stData.astOID, oid_FileTrans_FileName, sizeof(oid_FileTrans_FileName));
    stData.ulOIDSize = ARRAY_SIZE(oid_FileTrans_FileName);
    stData.szValue = (CHAR*)snmp_strdup(acServerFile);
    stData.ulDataType = TYPE_CHAR;
    stData.ulDataLen = strlen(stData.szValue);
    stReq.vData.push_back(stData);

    memcpy(stData.astOID, oid_FileTrans_FileType, sizeof(oid_FileTrans_FileType));
    stData.ulOIDSize = ARRAY_SIZE(oid_FileTrans_FileType);
    stData.szValue = (CHAR*)ToIntPtr(iFileType);
    stData.ulDataType = TYPE_INT;
    stData.ulDataLen = sizeof(INT32);
    stReq.vData.push_back(stData);

    memcpy(stData.astOID, oid_FileTrans_TransProtocol, sizeof(oid_FileTrans_TransProtocol));
    stData.ulOIDSize = ARRAY_SIZE(oid_FileTrans_TransProtocol);
    stData.szValue = (CHAR*)ToIntPtr(iTransProtocol);
    stData.ulDataType = TYPE_INT;
    stData.ulDataLen = sizeof(INT32);
    stReq.vData.push_back(stData);

    memcpy(stData.astOID, oid_FileTrans_ServerAddr, sizeof(oid_FileTrans_ServerAddr));
    stData.ulOIDSize = ARRAY_SIZE(oid_FileTrans_ServerAddr);
    stData.szValue = (CHAR*)snmp_malloc(sizeof(g_stLocalCfg.aucFtpAddr));
    memcpy(stData.szValue, g_stLocalCfg.aucFtpAddr, sizeof(g_stLocalCfg.aucFtpAddr));
    stData.ulDataType = TYPE_IP;
    stData.ulDataLen = sizeof(g_stLocalCfg.aucFtpAddr);
    stReq.vData.push_back(stData);

    memcpy(stData.astOID, oid_FileTrans_ServerPort, sizeof(oid_FileTrans_ServerPort));
    stData.ulOIDSize = ARRAY_SIZE(oid_FileTrans_ServerPort);
    stData.szValue = (CHAR*)ToIntPtr(iServerPort);
    stData.ulDataType = TYPE_INT;
    stData.ulDataLen = sizeof(INT32);
    stReq.vData.push_back(stData);

    memcpy(stData.astOID, oid_FileTrans_ServerUserName, sizeof(oid_FileTrans_ServerUserName));
    stData.ulOIDSize = ARRAY_SIZE(oid_FileTrans_ServerUserName);
    stData.szValue = (CHAR*)snmp_strdup(g_stLocalCfg.acFtpUser);
    stData.ulDataType = TYPE_CHAR;
    stData.ulDataLen = strlen(stData.szValue);
    stReq.vData.push_back(stData);

    memcpy(stData.astOID, oid_FileTrans_ServerPasswd, sizeof(oid_FileTrans_ServerPasswd));
    stData.ulOIDSize = ARRAY_SIZE(oid_FileTrans_ServerPasswd);
    stData.szValue = (CHAR*)snmp_strdup(g_stLocalCfg.acFtpPwd);
    stData.ulDataType = TYPE_CHAR;
    stData.ulDataLen = strlen(stData.szValue);
    stReq.vData.push_back(stData);

    memcpy(stData.astOID, oid_FileTrans_FileVersion, sizeof(oid_FileTrans_FileVersion));
    stData.ulOIDSize = ARRAY_SIZE(oid_FileTrans_FileVersion);
    stData.szValue = (CHAR*)snmp_strdup(" ");
    stData.ulDataType = TYPE_CHAR;
    stData.ulDataLen = strlen(stData.szValue);
    stReq.vData.push_back(stData);

    SendSnmpSetReq(stReq, szNeID, stDevInfo.stNeBasicInfo.stClientAddr);

    // upload_command
    stReq.vData.clear();
    stReq.ulSeqID = SnmpGetSeqID();

    memcpy(stData.astOID, oid_CfgDataFileImpExp, sizeof(oid_CfgDataFileImpExp));
    stData.ulOIDSize = ARRAY_SIZE(oid_CfgDataFileImpExp);
    stData.szValue = (CHAR*)ToIntPtr(iExportFile);
    stData.ulDataType = TYPE_INT;
    stData.ulDataLen = sizeof(INT32);
    stReq.vData.push_back(stData);

    SendSnmpSetReq(stReq, szNeID, stDevInfo.stNeBasicInfo.stClientAddr);
}

VOID SendRebootDevMsg(CHAR *szNeID)
{
    OMA_DEV_INFO_T  stDevInfo;
    SNMP_SET_REQ_T  stReq;
    SNMP_DATA_T     stData;
    INT32           iReset = 1;

    if (!GetDevInfo(szNeID, stDevInfo))
    {
        GosLog(LOG_ERROR, "send upload file failed, unknown ne %s", szNeID);
        return;
    }

    stReq.ucSetVer = g_stLocalCfg.ulSnmpVer;
    stReq.ulSetType = SNMP_MSG_SET;
    stReq.ulSeqID = SnmpGetSeqID();
    InitDevSetCommunity(stDevInfo.stNeBasicInfo, stReq.acSetCommunity);

    memcpy(stData.astOID, oid_SysRestart, sizeof(oid_SysRestart));
    stData.ulOIDSize = ARRAY_SIZE(oid_SysRestart);
    stData.szValue = (CHAR*)ToIntPtr(iReset);
    stData.ulDataType = TYPE_INT;
    stData.ulDataLen = sizeof(INT32);
    stReq.vData.push_back(stData);

    SendSnmpSetReq(stReq, szNeID, stDevInfo.stNeBasicInfo.stClientAddr);
}

VOID SendSetGetLastOperLogIndex(OMA_DEV_INFO_T &stDevInfo)
{
    SNMP_SET_REQ_T  stReq;
    SNMP_DATA_T     stData;
    INT32           iGetLastIndex = 1;

    stReq.ucSetVer = g_stLocalCfg.ulSnmpVer;
    stReq.ulSetType = SNMP_MSG_SET;
    stReq.ulSeqID = SnmpGetSeqID();
    InitDevSetCommunity(stDevInfo.stNeBasicInfo, stReq.acSetCommunity);

    memcpy(stData.astOID, oid_GetLastOperLogIndex, sizeof(oid_GetLastOperLogIndex));
    stData.ulOIDSize = ARRAY_SIZE(oid_GetLastOperLogIndex);
    stData.szValue = (CHAR*)ToIntPtr(iGetLastIndex);
    stData.ulDataType = TYPE_INT;
    stData.ulDataLen = sizeof(INT32);
    stReq.vData.push_back(stData);

	SendSnmpSetReq(stReq, stDevInfo.stNeBasicInfo.acNeID, stDevInfo.stNeBasicInfo.stClientAddr);
}

VOID SendSetGetLastFileListIndex(OMA_DEV_INFO_T &stDevInfo)
{
    SNMP_SET_REQ_T  stReq;
    SNMP_DATA_T     stData;
    INT32           iGetLastIndex = 1;

    stReq.ucSetVer = g_stLocalCfg.ulSnmpVer;
    stReq.ulSetType = SNMP_MSG_SET;
    stReq.ulSeqID = SnmpGetSeqID();
    InitDevSetCommunity(stDevInfo.stNeBasicInfo, stReq.acSetCommunity);

    memcpy(stData.astOID, oid_GetLastFileListIndex, sizeof(oid_GetLastFileListIndex));
    stData.ulOIDSize = ARRAY_SIZE(oid_GetLastFileListIndex);
    stData.szValue = (CHAR*)ToIntPtr(iGetLastIndex);
    stData.ulDataType = TYPE_INT;
    stData.ulDataLen = sizeof(INT32);
    stReq.vData.push_back(stData);

	SendSnmpSetReq(stReq, stDevInfo.stNeBasicInfo.acNeID, stDevInfo.stNeBasicInfo.stClientAddr);
}

BOOL TaskQuery::CreateOfflineAlarm(OMA_DEV_INFO_T &stInfo)
{
    BOOL                bIsNewAlarm;
    ALARM_INFO_T        stAlarmInfo;
   
    memcpy(&stAlarmInfo.acNeID ,&stInfo.stNeBasicInfo .acNeID ,sizeof(stAlarmInfo.acNeID));
    memcpy(&stAlarmInfo.acAlarmTitle ,DevOffLineAlarmTitle ,sizeof(stAlarmInfo.acAlarmTitle ));
    memcpy(&stAlarmInfo.acAlarmInfo ,DevOffLineAlarmInfo ,sizeof(stAlarmInfo.acAlarmInfo ));
    memcpy(&stAlarmInfo.acAlarmReason ,DevOffLineAlarmRason ,sizeof(stAlarmInfo.acAlarmReason ));

    stAlarmInfo.ulAlarmID = SNMP_ALARM_CODE_DevOffLine;
    stAlarmInfo.ulAlarmType = ALARM_TYPE_DEV;
    stAlarmInfo.ulAlarmLevel = ALARM_LEVEL_MAIN;
    stAlarmInfo.ulAlarmRaiseTime = gos_get_current_time(); 
    stAlarmInfo.ulAlarmStatus = ALARM_STATUS_RAISE;

    //�жϸø澯�Ƿ��Ѵ��ڣ����澯д��ȫ�ֱ�����g_vActiveAlarm
    AddActiveAlarm(stAlarmInfo, bIsNewAlarm);  
    if (bIsNewAlarm)
    {
        if (!m_pDao->AddActiveAlarm(stAlarmInfo)) //���澯д�����ݿ�
        {
            TaskLog(LOG_ERROR, "add active alarm(%s, %u) to db failed", stAlarmInfo.acNeID, stAlarmInfo.ulAlarmID);
            return FALSE;
        }
        else
		{
            TaskLog(LOG_INFO, "dev %s create offline alarm sucess", stInfo.stNeBasicInfo.acDevName);
		}
    }

    // ����ȫ���豸״̬��Ϣ
    SetOnline(stInfo, FALSE);
    //����DevInfoMap
    SetDevInfo(stInfo); 	
    return TRUE;
}
// ��� OMA �豸����Ϣ
VOID TaskQuery::CheckDev(OMA_DEV_INFO_T &stInfo)
{
    static UINT32   ulLastCheckTime = 0;
    UINT32  ulDevType = stInfo.stNeBasicInfo.ulDevType;
    UINT32  ulCurrTime = gos_get_uptime();
    ulLastCheckTime = ulCurrTime;

    if (stInfo.stNeBasicInfo.stClientAddr.usPort == 0)
    {
        return;
    }

    if (ulDevType == OMC_DEV_TYPE_EOMC ||
        ulDevType == OMC_DEV_TYPE_ZTE)
    {
        return;
    }

    SendGetNeBasicInfoMsg(stInfo.stNeBasicInfo);    // ��ȡ������Ϣ
    SendGetNeStatusMsg(stInfo.stNeBasicInfo);       // ��ȡ״̬��Ϣ

    // ��ȡIP/����/����
    SendGetNetInfoMsg(stInfo.stNeBasicInfo );

    // ��ȡLTE״̬��Ϣ
    if (ulDevType == OMC_DEV_TYPE_TX ||
        ulDevType == OMC_DEV_TYPE_FX ||
        ulDevType == OMC_DEV_TYPE_TAU)
    {
        SendGetLteStatusMsg(stInfo.stNeBasicInfo);      
    }

    //��ȡ TAU ������Ϣ
	if (ulDevType == OMC_DEV_TYPE_TAU)
	{
		SendGetTrafficMsg(stInfo.stNeBasicInfo);
	}

    // ��ȡ TX/FX ������־��������־�ļ�����
    if (ulDevType == OMC_DEV_TYPE_TX )
    {
        SendGetOperLogMsg(stInfo.stNeBasicInfo, stInfo.unDevStatus.stTxStatus.ulOperLogIndex, stInfo.unDevStatus.stTxStatus.bGetOperLogIndex);  // ��ȡ������־
        SendGetFileListMsg(stInfo.stNeBasicInfo, stInfo.unDevStatus.stTxStatus.ulFileIndex, stInfo.unDevStatus.stTxStatus.bGetFileIndex);   // ��ȡ�ļ��б�
    }
    else if (ulDevType == OMC_DEV_TYPE_FX )
    {
        SendGetOperLogMsg(stInfo.stNeBasicInfo, stInfo.unDevStatus.stFxStatus.ulOperLogIndex, stInfo.unDevStatus.stFxStatus.bGetOperLogIndex);  // ��ȡ������־
        SendGetFileListMsg(stInfo.stNeBasicInfo, stInfo.unDevStatus.stFxStatus.ulFileIndex, stInfo.unDevStatus.stFxStatus.bGetFileIndex);   // ��ȡ�ļ��б�
    }

    // TX/FX ����������
    if (ulDevType == OMC_DEV_TYPE_TX ||
        ulDevType == OMC_DEV_TYPE_FX )
    {
        OMC_SW_UPDATE_INFO_T    stSwUpdate = {0};
		
		if (GetSwUpdate(stInfo.stNeBasicInfo.acNeID, stSwUpdate))
		{	
			// ����豸�Ƿ�������������Ϣ����������豸��ǰ�������汾������������Ŀ��汾��һ�£����·�����ָ��
			if ( stSwUpdate.bSetFlag && 
				 strcmp(stSwUpdate.acNewSwVer, stInfo.stNeBasicInfo.acHardwareVer) != 0 )
		    {
		        SendSetSwUpdateMsg(stInfo.stNeBasicInfo, stSwUpdate.acNewSwVer);
		    }
		}
		
    }

    // �豸����״̬��⣬������һ���հ�ʱ��������ߵļ�����ڣ����ж��豸����
    if (stInfo.stNeBasicInfo.ulLastOnlineTime == 0 ||
        (ulCurrTime - stInfo.stNeBasicInfo.ulLastOnlineTime) > g_stLocalCfg.ulOnlineCheckPeriod)
    {
		// ����̨���̶�̨��TAU�Ļ�Ծ�澯�����ó������Զ����
		if ( ulDevType == OMC_DEV_TYPE_TAU ||
			 ulDevType == OMC_DEV_TYPE_TX  ||
			 ulDevType == OMC_DEV_TYPE_FX )
		{
			VECTOR<ALARM_INFO_T> vInfo;
			GetActiveAlarmInfo(stInfo.stNeBasicInfo.acNeID, vInfo);
			for(UINT32 i=0; i<vInfo.size(); i++)
			{
			    DelActiveAlarm(vInfo[i].acNeID , vInfo[i].ulAlarmID);   
			    if (!m_pDao->ClearActiveAlarm(vInfo[i].acNeID , vInfo[i].ulAlarmID, vInfo[i].ulAlarmRaiseTime,vInfo[i].ulAlarmLevel,(CHAR *)ALARM_CLEAR_OFFLINE))
			    {
					TaskLog(LOG_ERROR, "clear active alarm(%s, %u) from db failed", vInfo[i] .acNeID , vInfo[i].ulAlarmID);
			    }	   
			}
		}

        // ���豸λ�ڳ������Ͷ��޶ε��豸���ߣ��򲻲������߸澯
        // ����14����StationID��11-28֮��Ϊ����
        if ( ulDevType == OMC_DEV_TYPE_TX && 
			(stInfo.unDevStatus.stTxStatus.stLteStatus.ulStationID < 11 ||
			 stInfo.unDevStatus.stTxStatus.stLteStatus.ulStationID > 28) )
        {
            SetOnline(stInfo, FALSE);
        }
		else 
		{
			// �������߸澯
			CreateOfflineAlarm(stInfo);
		}

        // ֪ͨ�豸����
        SendSetManageStateMsg(stInfo.stNeBasicInfo.acNeID, FALSE);
    }
}

VOID TaskQuery::OnCheckTimer()
{
    VECTOR<OMA_DEV_INFO_T>  vDevInfo;

    GetDevInfo(vDevInfo);
    if (vDevInfo.size() == 0)
    {
        return;
    }

    UINT32  ulSingleDevCheckPeriod = (0.9*g_ulDevCheckPeriod)/vDevInfo.size();

    for (UINT32 i=0; i<vDevInfo.size(); i++)
    {
        if (i > 0)
        {
            gos_sleep_1ms(ulSingleDevCheckPeriod);
        }
        else
        {
            TaskLog(LOG_INFO, "retrieving ne info");
        }

        // �豸���߲Ž���Get����
        if (vDevInfo[i].bIsOnLine)
        {
            CheckDev(vDevInfo[i]);
        }
    }
}

VOID TaskQuery::OnClientDisconnectServer(UINT16 usServerID)
{
}

VOID TaskQuery::TaskEntry(UINT16 usMsgID, VOID *pvMsg, UINT32 ulMsgLen)
{
    UINT32  ulTaskStatus = GetTaskStatus();

    switch(ulTaskStatus)
    {
        case TASK_STATUS_IDLE:
            switch(usMsgID)
            {
                case EV_TASK_INIT_REQ:
                    TaskLog(LOG_INFO, "start trap task...");
                    SendRsp();
                    SetTaskStatus(TASK_STATUS_INIT);
                    SetTimer(TIMER_INIT, 0);
                    break;

                default:
                    break;
            }

            break;

        case TASK_STATUS_INIT:
            switch(usMsgID)
            {
                case EV_INIT_IND:
                    if (!Init())
                    {
                        SetTimer(TIMER_INIT, 1000);
                    }

                    break;

                default:
                    break;
            }

            break;

        case TASK_STATUS_WORK:
            switch(usMsgID)
            {
                case EV_CHECK_TIMER:
                    OnCheckTimer();
                    break;

                default:
                    TaskLog(LOG_ERROR, "unknown msg %u", usMsgID);
                    break;
            }

            break;

        default:
            break;
    }
}
