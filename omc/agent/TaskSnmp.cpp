#include "omc_public.h"
#include "TaskSnmp.h"
#include "snmp_nms.h"
#include "oid.h"
#include "eomc_oid.h"
#include <string>
#include "OmcDao.h"

SOCKET                  g_stSnmpSocket = INVALID_SOCKET;
RawIntMap<NEID_T>       g_mSnmpSeqID;
GMutex                  g_MutexSeqID;

extern OMC_LOCAL_CFG_T  g_stLocalCfg;
extern VOID SendSetGetLastOperLogIndex(OMA_DEV_INFO_T &stDevInfo);
extern VOID SendSetGetLastFileListIndex(OMA_DEV_INFO_T &stDevInfo);
CHAR                    *g_szAlarmRaiseFile = (CHAR*)"alarm_raise.txt";
CHAR                    *g_szAlarmRaiseTmpFile = (CHAR*)"alarm_raise_tmp.txt";

CHAR                    *g_szAlarmClearTmpFile = (CHAR*)"alarm_clear_tmp.txt";
CHAR                    *g_szAlarmClearFile = (CHAR*)"alarm_clear.txt";

/******************************************************/
/*函数名称：CreateOfflineAlarm
/*函数功能：设备离线时产生离线告警
/*返回值：成功返回 TRUE / 失败返回 FALSE
/*参数：OMA_DEV_INFO_T &stDevInfo // 离线设备信息
/*
/*说明：
/*时间：2021/4/12  ZWJ 　
/******************************************************/
BOOL TaskSnmp::CreateOfflineAlarm(OMA_DEV_INFO_T &stDevInfo)
{
    BOOL                bIsNewAlarm;
    ALARM_INFO_T        stAlarmInfo;
/*
    if (stDevInfo.bOffLine)
    {
        return FALSE;
    }
*/

    memcpy(&stAlarmInfo.acNeID ,&stDevInfo.stNeBasicInfo .acNeID ,sizeof(stAlarmInfo.acNeID));
    memcpy(&stAlarmInfo.acAlarmTitle ,DevOffLineAlarmTitle ,sizeof(stAlarmInfo.acAlarmTitle ));
    memcpy(&stAlarmInfo.acAlarmInfo ,DevOffLineAlarmInfo ,sizeof(stAlarmInfo.acAlarmInfo ));
    memcpy(&stAlarmInfo.acAlarmReason ,DevOffLineAlarmRason ,sizeof(stAlarmInfo.acAlarmReason ));

    stAlarmInfo.ulAlarmID = SNMP_ALARM_CODE_DevOffLine;
    stAlarmInfo.ulAlarmType = ALARM_TYPE_DEV;
    stAlarmInfo.ulAlarmLevel = ALARM_LEVEL_MAIN;
    stAlarmInfo.ulAlarmRaiseTime = gos_get_current_time(); //stDevInfo.ulTime;
    stAlarmInfo.ulAlarmStatus = ALARM_STATUS_RAISE;
    //stAlarmInfo.ulPrevAlarmStatus = ALARM_STATUS_RAISE; //上一个时间段的告警状态

    AddActiveAlarm(stAlarmInfo, bIsNewAlarm);  //将告警写入全局变量：g_vActiveAlarm
    if (bIsNewAlarm)
    {
        if (!m_pDao->AddActiveAlarm(stAlarmInfo)) //将告警写入数据库
        {
            TaskLog(LOG_ERROR, "add active alarm(%s, %u) to db failed", stAlarmInfo.acNeID, stAlarmInfo.ulAlarmID);
        }
    }

    stDevInfo.bIsOnLine = FALSE;
    SetDevInfo(stDevInfo);  //更新DevInfoMap

    return TRUE;
}

BOOL GetNeIDBySnmpSeqID(UINT32 ulSeqID, CHAR *szNeID)
{
    if (!g_MutexSeqID.Mutex())
    {
        return FALSE;
    }

    NEID_T  *pstNeID = g_mSnmpSeqID.GetValue(ulSeqID);

    if (!pstNeID)
    {
        g_MutexSeqID.Unmutex();
        return FALSE;
    }

    strcpy(szNeID, pstNeID->acNeID);

    g_MutexSeqID.Unmutex();

    return TRUE;
}

BOOL AddSnmpSeqID(UINT32 ulSeqID, CHAR *szNeID)
{
    if (!g_MutexSeqID.Mutex())
    {
        return FALSE;
    }

    NEID_T  stNeID;

    strcpy(stNeID.acNeID, szNeID);
    if (!g_mSnmpSeqID.Add(ulSeqID, (NEID_T*)szNeID))
    {
        g_MutexSeqID.Unmutex();
        return FALSE;
    }

    g_MutexSeqID.Unmutex();

    return TRUE;
}

BOOL DelSnmpSeqID(UINT32 ulSeqID)
{
    if (!g_MutexSeqID.Mutex())
    {
        return FALSE;
    }

    g_mSnmpSeqID.Del(ulSeqID);

    g_MutexSeqID.Unmutex();

    return TRUE;
}

VOID SnmpFreeGetRsp(SNMP_GET_RSP_T &stGetInfo)
{
    for (UINT32 i=0; i<stGetInfo.vData.size(); i++)
    {
        if (stGetInfo.vData[i].szValue)
        {
            snmp_free(stGetInfo.vData[i].szValue);
        }
    }

    stGetInfo.vData.clear();
}

BOOL SnmpParseGetRsp(UINT8 *pucRspData, UINT32 ulRspDataLen, SNMP_GET_RSP_T &stGetInfo)
{
    UINT32          ulPDULen;
    UINT32          ulSnmpPDULen;
    UINT32          ulParasLen;
    UINT32          ulPara1Len;
    UINT8           ucOIDSize;
    UINT8           ucTmp;
    UINT8           *pucRsp = pucRspData;
    UINT8           ucDataType;
    UINT32          ulDataLen;
    SNMP_DATA_T     stData;

    stGetInfo.ucGetVer = 0xff;
    stGetInfo.ulGetType = 0xffffffff;
    stGetInfo.ulSeqID = 0xffffffff;
    stGetInfo.vData.clear();

    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return FALSE;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        ulPDULen = snmp_get_int_value(pucRspData, ucTmp);
        pucRspData += ucTmp;

        if (ulRspDataLen != (1+1+ucTmp+ulPDULen))
        {
            return FALSE;
        }
    }
    else
    {
        ulPDULen = pucRspData[0];
        pucRspData ++;

        if (ulRspDataLen != (1+1+ulPDULen))
        {
            return FALSE;
        }
    }

    //ver check
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1 )
    {
        return FALSE;
    }

    stGetInfo.ucGetVer = pucRspData[2];
    pucRspData += (1+1+1);

    //community
    if (pucRspData[0] != ASN_OCTET_STR ||
        pucRspData[1] == 0)
    {
        return FALSE;
    }
    pucRspData += (1+1+pucRspData[1]);

    //snmp pdu type : get response
    stGetInfo.ulGetType = *pucRspData++;
    if (stGetInfo.ulGetType != SNMP_MSG_RESPONSE)
    /*if (stGetInfo.ulGetType != SNMP_MSG_GET &&
        stGetInfo.ulGetType != SNMP_MSG_GETNEXT &&
        stGetInfo.ulGetType != SNMP_MSG_GETBULK )*/
    {
        return FALSE;
    }

    //snmp pdu len
    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        ulSnmpPDULen = snmp_get_int_value(pucRspData, ucTmp);
        pucRspData += ucTmp;
    }
    else
    {
        ulSnmpPDULen = pucRspData[0];
        pucRspData ++;
    }

    //req id
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] == 0)
    {
        GosLog(LOG_ERROR, "snmp_parse_get_req: pucRspData[0]:%u,pucRspData[1]:%u.",
            pucRspData[0], pucRspData[1]);
        return FALSE;
    }

    ucTmp = pucRspData[1];
    pucRspData += 2;

    stGetInfo.ulSeqID = snmp_get_int_value(pucRspData, ucTmp);
    pucRspData += ucTmp;

    //error status
    if (pucRspData[0] != ASN_INTEGER ||
        pucRspData[1] != 1)
    {
        return FALSE;
    }

    pucRspData += 2;

    if ((*pucRspData++) != SNMP_ERR_NOERROR)
    {
        return FALSE;
    }

    //error index
    pucRspData += (1+1+1);

    //paras
    if ((*pucRspData++) != SNMP_ID_SEQUENCE)
    {
        return FALSE;
    }

    if (pucRspData[0] > 0x80)
    {
        ucTmp = pucRspData[0] - 0x80;
        pucRspData ++;

        ulParasLen = snmp_get_int_value(pucRspData, ucTmp);
        pucRspData += ucTmp;
    }
    else
    {
        ulParasLen = pucRspData[0];
        pucRspData ++;
    }

    while(1)
    {
        memset(&stData, 0, sizeof(stData));

        // para
        if ((*pucRspData++) != SNMP_ID_SEQUENCE)
        {
            return FALSE;
        }

        if (pucRspData[0] > 0x80)
        {
            ucTmp = pucRspData[0] - 0x80;
            pucRspData ++;

            ulPara1Len = snmp_get_int_value(pucRspData, ucTmp);
            pucRspData += ucTmp;
        }
        else
        {
            ulPara1Len = pucRspData[0];
            pucRspData ++;
        }

        //oid type
        if (pucRspData[0] != ASN_OBJECT_ID ||
            pucRspData[1] == 0)
        {
            return FALSE;
        }

        ucOIDSize = pucRspData[1];
        pucRspData += 2;

        //check oid
        if (snmp_parse_oid_var(pucRspData, ucOIDSize, stData.astOID, &stData.ulOIDSize) < 0)
        {
            return FALSE;
        }

        pucRspData += ucOIDSize;

        // value
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

        stData.ulDataType = ucDataType;
        stData.ulDataLen  = ulDataLen;

        /*
        if (ucDataType == ASN_OCTET_STR)
        {
            pstValue->ulLen = strlen((char*)pucRspData)+1;
            if (pstValue->ulLen > ulDataLen)
            {
                pstValue->ulLen = ulDataLen;
            }
        }*/

        if (ulDataLen == 0)
        {
            stData.szValue = NULL;
        }
        else
        {
            if (ucDataType == ASN_GAUGE     ||
                ucDataType == ASN_TIMETICKS ||
                ucDataType == ASN_COUNTER   ||
                ucDataType == ASN_INTEGER   )
            {
                stData.szValue = (CHAR*)snmp_calloc(sizeof(UINT32), ulDataLen);
                if (stData.szValue == NULL)
                {
                    return FALSE;
                }

                if (ulDataLen == sizeof(UINT32)+1)
                {
                    stData.ulDataLen--;
                }

                nms_get_data(ucDataType, stData.ulDataLen, pucRspData, stData.szValue);
            }
            else if (ucDataType == ASN_COUNTER64)
            {
                stData.szValue = (CHAR*)snmp_calloc(sizeof(UINT64), ulDataLen);
                if (stData.szValue == NULL)
                {
                    return FALSE;
                }

                if (ulDataLen == sizeof(UINT64)+1)
                {
                    stData.ulDataLen--;
                }

                nms_get_data(ucDataType, stData.ulDataLen, pucRspData, stData.szValue);
            }
            else
            {
                stData.szValue = (CHAR*)snmp_calloc(1, ulDataLen+1);
                if (stData.szValue == NULL)
                {
                    return FALSE;
                }

                memcpy(stData.szValue, pucRspData, ulDataLen);
            }
        }

        pucRspData += ulDataLen;

        stGetInfo.vData.push_back(stData);

        if (pucRspData >= pucRsp+ulRspDataLen)
        {
            break;
        }
    }

    return TRUE;
}

TaskSnmp::TaskSnmp(UINT16 usDispatchID):GBaseTask(1001, "Snmp", usDispatchID, FALSE)
{
    m_pDao = NULL;

    m_fpRaise =NULL;
    m_fpClear =NULL;
    m_szRiseFile = NULL;
    m_szRiseTmpFile = NULL;
    m_szClearFile = NULL;
    m_szClearTmpFile = NULL;
    m_vRaiseAlarm.clear();
    m_vClearAlarm.clear();

    m_bInitDevInfo = FALSE;
    m_stTrapSocket = INVALID_SOCKET;
    m_ulLastSaveTime = 0;
    m_ulLastAlarmRaiseUpdateTime = 0;
    m_ulLastAlarmClearUpdateTime = 0;
}   

BOOL TaskSnmp::Init()
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

    if (!m_bInitDevInfo)
    {
        VECTOR<NE_BASIC_INFO_T>     vInfo;

        if (!m_pDao->GetNEBasicInfo(vInfo))
        {
            TaskLog(LOG_ERROR, "load ne basic info failed");

            return FALSE;
        }

        InitDevInfo(vInfo);

        TaskLog(LOG_INFO, "init dev info succ");
        m_bInitDevInfo = TRUE;
    }


    CHAR acRaiseTmpFile[128] = {0};
    CHAR acRaiseFile[128] = {0};
    CHAR acClearTmpFile[128] = {0};
    CHAR acClearFile[128] = {0};

    sprintf(acRaiseTmpFile, "%s/%s", gos_get_root_path(), g_szAlarmRaiseTmpFile);
    sprintf(acRaiseFile, "%s/%s", gos_get_root_path(), g_szAlarmRaiseFile);
    sprintf(acClearTmpFile, "%s/%s", gos_get_root_path(), g_szAlarmClearTmpFile);
    sprintf(acClearFile, "%s/%s", gos_get_root_path(), g_szAlarmClearFile);

    gos_format_path(acRaiseTmpFile);
    gos_format_path(acRaiseFile);
    gos_format_path(acClearTmpFile);
    gos_format_path(acClearFile);

    m_szRiseTmpFile = acRaiseTmpFile;
    m_szRiseFile = acRaiseFile;
    m_szClearTmpFile = acClearTmpFile;
    m_szClearFile = acClearFile;

    m_fpRaise = fopen(acRaiseTmpFile, "a+b");
    m_fpClear = fopen(acClearTmpFile, "a+b");

    TaskLog(LOG_INFO, "Init successful");

    // 设置任务的状态为 work
    SetTaskStatus(TASK_STATUS_WORK);

    while (1)
    {
        ListenSnmp(g_stLocalCfg.usSnmpPort, g_stLocalCfg.usTrapPort);
        //gos_sleep_1ms(1000);
    }


    return TRUE;
}

VOID TaskSnmp::OnClientDisconnectServer(UINT16 usServerID)
{
}

INT32 RecvSnmpMsg(SOCKET stSnmpSock, SOCKET stTrapSock, VOID *pMsgBuf, UINT32 ulMsgBufLen, UINT32 ulTimeout, SOCKADDR_IN *pstAddr, BOOL &bIsTrapMsg)
{
    FD_SET          fds;
    TIMEVAL         stTimeout = {0};
    INT32           iRet;
    INT32           iRcvLen;
    socklen_t       iLen = sizeof(SOCKADDR);
    SOCKET          stMaxSocket = MAX(stSnmpSock, stTrapSock);

    while(1)
    {
        stTimeout.tv_sec = ulTimeout/1000;
        stTimeout.tv_usec = 1000*(ulTimeout%1000);
        FD_ZERO(&fds);
        FD_SET(stSnmpSock, &fds);
        FD_SET(stTrapSock, &fds);

        iRet = select(stMaxSocket + 1, &fds, NULL, NULL, &stTimeout);
        if (iRet == 0)
        {
            return 0;
        }

        if (iRet < 0)
        {
            if (errno == EINTR)
            {                
                continue;            
            }
            else
            {
                return -1;
            }        
        }
        break;
    }

    if (FD_ISSET(stSnmpSock, &fds))
    {
        iRcvLen = recvfrom(stSnmpSock, (char*)pMsgBuf, ulMsgBufLen, 0, (SOCKADDR *)pstAddr, &iLen);

        FD_CLR(stSnmpSock, &fds);

        bIsTrapMsg = FALSE;

        return iRcvLen;
    }
    else if (FD_ISSET(stTrapSock, &fds))
    {
        iRcvLen = recvfrom(stTrapSock, (char*)pMsgBuf, ulMsgBufLen, 0, (SOCKADDR *)pstAddr, &iLen);

        FD_CLR(stTrapSock, &fds);

        bIsTrapMsg = TRUE;

        //GosLog("SNMP", LOG_INFO, "recv trap msg %d", iRcvLen);

        return iRcvLen;
    }

    return -1;
}

VOID TaskSnmp::SaveTXInfo(UINT32 ulCurrTime)
{
    VECTOR<OMA_DEV_INFO_T>          vDevInfo;
    VECTOR<OMA_TX_STATUS_REC_T>     vStatus;
    OMA_TX_STATUS_REC_T             stRec;
    BOOL                            bNeedSave = FALSE;
    BOOL                            bOnline;
    std::string                     strNeID;

    GetDevInfo(OMC_DEV_TYPE_TX, vDevInfo);
    stRec.ulTime = gos_get_current_time();

    for (UINT32 i=0; i<vDevInfo.size(); i++)
    {
        strcpy(stRec.acNeID, vDevInfo[i].stNeBasicInfo.acNeID);
        memcpy(&stRec.stStatus, &vDevInfo[i].unDevStatus.stTxStatus, sizeof(stRec.stStatus));

        if (vDevInfo[i].stNeBasicInfo.ulLastOnlineTime == 0 ||
            (ulCurrTime - vDevInfo[i].stNeBasicInfo.ulLastOnlineTime) > g_stLocalCfg.ulOnlineCheckPeriod)
        {
            stRec.stStatus.bOnline = FALSE;
        }
        else
        {
            stRec.stStatus.bOnline = TRUE;
        }

        strNeID = stRec.acNeID;
        bNeedSave = TRUE;
        if (m_mOnline.find(strNeID) == m_mOnline.end())
        {
            m_mOnline[strNeID] = stRec.stStatus.bOnline;
            if (!stRec.stStatus.bOnline) // 把设备在线状态添加到 map 中，当前设备不在线则不存储
            {
                bNeedSave = FALSE;
            }
        }
        else
        {
            bOnline = m_mOnline[strNeID];
            if ((bOnline == stRec.stStatus.bOnline) && !bOnline)    // 当前不在线并且以前也不在线时不存储
            {
                bNeedSave = FALSE;
            }
            m_mOnline[strNeID] = stRec.stStatus.bOnline;
        }

        if (bNeedSave)
        {
            vStatus.push_back(stRec);
        }
    }

    if (vStatus.size() > 0)
    {
        if (!m_pDao->AddTxStatus(vStatus))
        {
            TaskLog(LOG_ERROR, "save TX ne_basic_status failed");
        }       
    }
}

VOID TaskSnmp::SaveFXInfo(UINT32 ulCurrTime)
{
    VECTOR<OMA_DEV_INFO_T>          vDevInfo;
    VECTOR<OMA_FX_STATUS_REC_T>     vStatus;
    OMA_FX_STATUS_REC_T             stRec;
    BOOL                            bNeedSave = FALSE;
    BOOL                            bOnline;
    std::string                     strNeID;

    GetDevInfo(OMC_DEV_TYPE_FX, vDevInfo);
    stRec.ulTime = gos_get_current_time();

    for (UINT32 i=0; i<vDevInfo.size(); i++)
    {
        strcpy(stRec.acNeID, vDevInfo[i].stNeBasicInfo.acNeID);
        memcpy(&stRec.stStatus, &vDevInfo[i].unDevStatus.stFxStatus, sizeof(stRec.stStatus));

        if (vDevInfo[i].stNeBasicInfo.ulLastOnlineTime == 0 ||
            (ulCurrTime - vDevInfo[i].stNeBasicInfo.ulLastOnlineTime) > g_stLocalCfg.ulOnlineCheckPeriod)
        {
            stRec.stStatus.bOnline = FALSE;
        }
        else
        {
            stRec.stStatus.bOnline = TRUE;
        }

        strNeID = stRec.acNeID;
        bNeedSave = TRUE;
        if (m_mOnline.find(strNeID) == m_mOnline.end())
        {
            m_mOnline[strNeID] = stRec.stStatus.bOnline;
            if (!stRec.stStatus.bOnline) // 把设备在线状态添加到 map 中，当前设备不在线则不存储
            {
                bNeedSave = FALSE;
            }
        }
        else
        {
            bOnline = m_mOnline[strNeID];
            if ((bOnline == stRec.stStatus.bOnline) && !bOnline)    // 当前不在线并且以前也不在线时不存储
            {
                bNeedSave = FALSE;
            }
            m_mOnline[strNeID] = stRec.stStatus.bOnline;
        }

        if (bNeedSave)
        {
            vStatus.push_back(stRec);
        }
    }

    if (vStatus.size() > 0)
    {
        if (!m_pDao->AddFxStatus(vStatus))
        {
            TaskLog(LOG_ERROR, "save FX ne_basic_status failed");
        }       
    }
}

VOID TaskSnmp::SaveTAUInfo(UINT32 ulCurrTime)
{
    VECTOR<OMA_DEV_INFO_T>          vDevInfo;
    VECTOR<OMA_TAU_STATUS_REC_T>    vStatus;
    OMA_TAU_STATUS_REC_T            stRec;
    BOOL                            bNeedSave = FALSE;
    BOOL                            bOnline;
    std::string                     strNeID;

    GetDevInfo(OMC_DEV_TYPE_TAU, vDevInfo);
    stRec.ulTime = gos_get_current_time();

    for (UINT32 i=0; i<vDevInfo.size(); i++)
    {
        strcpy(stRec.acNeID, vDevInfo[i].stNeBasicInfo.acNeID);
        memcpy(&stRec.stStatus, &vDevInfo[i].unDevStatus.stTauStatus, sizeof(stRec.stStatus));

        if (vDevInfo[i].stNeBasicInfo.ulLastOnlineTime == 0 ||
            (ulCurrTime - vDevInfo[i].stNeBasicInfo.ulLastOnlineTime) > g_stLocalCfg.ulOnlineCheckPeriod)
        {
            stRec.stStatus.bOnline = FALSE;
        }
        else
        {
            stRec.stStatus.bOnline = TRUE;
        }

        strNeID = stRec.acNeID;
        bNeedSave = TRUE;
        if (m_mOnline.find(strNeID) == m_mOnline.end())
        {
            m_mOnline[strNeID] = stRec.stStatus.bOnline;
            if (!stRec.stStatus.bOnline) // 把设备在线状态添加到 map 中，当前设备不在线则不存储
            {
                bNeedSave = FALSE;
            }
        }
        else
        {
            bOnline = m_mOnline[strNeID];
            if ((bOnline == stRec.stStatus.bOnline) && !bOnline)    // 当前不在线并且以前也不在线时不存储
            {
                bNeedSave = FALSE;
            }
        }

        if (bNeedSave)
        {
            vStatus.push_back(stRec);
        }
    }

    if (vStatus.size() > 0)
    {
        if (!m_pDao->AddTauStatus(vStatus))
        {
            TaskLog(LOG_ERROR, "save TAU ne_basic_status failed");
        }

        if (!m_pDao->AddTrafficStatus(vStatus))
        {
            TaskLog(LOG_ERROR, "save TAU traffic_status failed");
        }
    }
}

VOID TaskSnmp::SaveDCInfo(UINT32 ulCurrTime)
{
    VECTOR<OMA_DEV_INFO_T>          vDevInfo;
    VECTOR<OMA_DC_STATUS_REC_T>     vStatus;
    OMA_DC_STATUS_REC_T             stRec;
    BOOL                            bNeedSave = FALSE;
    BOOL                            bOnline;
    std::string                     strNeID;

    GetDevInfo(OMC_DEV_TYPE_DC, vDevInfo);
    stRec.ulTime = gos_get_current_time();

    for (UINT32 i=0; i<vDevInfo.size(); i++)
    {
        strcpy(stRec.acNeID, vDevInfo[i].stNeBasicInfo.acNeID);
        memcpy(&stRec.stStatus, &vDevInfo[i].unDevStatus.stDcStatus, sizeof(stRec.stStatus));

        if (vDevInfo[i].stNeBasicInfo.ulLastOnlineTime == 0 ||
            (ulCurrTime - vDevInfo[i].stNeBasicInfo.ulLastOnlineTime) > g_stLocalCfg.ulOnlineCheckPeriod)
        {
            stRec.stStatus.bOnline = FALSE;
        }
        else
        {
            stRec.stStatus.bOnline = TRUE;
        }

        strNeID = stRec.acNeID;
        bNeedSave = TRUE;
        if (m_mOnline.find(strNeID) == m_mOnline.end())
        {
            m_mOnline[strNeID] = stRec.stStatus.bOnline;
            if (!stRec.stStatus.bOnline) // 把设备在线状态添加到 map 中，当前设备不在线则不存储
            {
                bNeedSave = FALSE;
            }
        }
        else
        {
            bOnline = m_mOnline[strNeID];
            if ((bOnline == stRec.stStatus.bOnline) && !bOnline)    // 当前不在线并且以前也不在线时不存储
            {
                bNeedSave = FALSE;
            }
        }

        if (bNeedSave)
        {
            vStatus.push_back(stRec);
        }
    }

    if (vStatus.size() > 0)
    {
        if (!m_pDao->AddDCStatus(vStatus))
        {
            TaskLog(LOG_ERROR, "save DC ne_basic_status failed");
        }       
    }
}

VOID TaskSnmp::SaveDISInfo(UINT32 ulCurrTime)
{
    VECTOR<OMA_DEV_INFO_T>          vDevInfo;
    VECTOR<OMA_DIS_STATUS_REC_T>    vStatus;
    OMA_DIS_STATUS_REC_T            stRec;
    BOOL                            bNeedSave = FALSE;
    BOOL                            bOnline;
    std::string                     strNeID;

    GetDevInfo(OMC_DEV_TYPE_DIS, vDevInfo);
    stRec.ulTime = gos_get_current_time();

    for (UINT32 i=0; i<vDevInfo.size(); i++)
    {
        strcpy(stRec.acNeID, vDevInfo[i].stNeBasicInfo.acNeID);
        memcpy(&stRec.stStatus, &vDevInfo[i].unDevStatus.stDisStatus, sizeof(stRec.stStatus));

        if (vDevInfo[i].stNeBasicInfo.ulLastOnlineTime == 0 ||
            (ulCurrTime - vDevInfo[i].stNeBasicInfo.ulLastOnlineTime) > g_stLocalCfg.ulOnlineCheckPeriod)
        {
            stRec.stStatus.bOnline = FALSE;
        }
        else
        {
            stRec.stStatus.bOnline = TRUE;
        }

        strNeID = stRec.acNeID;
        bNeedSave = TRUE;
        if (m_mOnline.find(strNeID) == m_mOnline.end())
        {
            m_mOnline[strNeID] = stRec.stStatus.bOnline;
            if (!stRec.stStatus.bOnline) // 把设备在线状态添加到 map 中，当前设备不在线则不存储
            {
                bNeedSave = FALSE;
            }
        }
        else
        {
            bOnline = m_mOnline[strNeID];
            if ((bOnline == stRec.stStatus.bOnline) && !bOnline)    // 当前不在线并且以前也不在线时不存储
            {
                bNeedSave = FALSE;
            }
        }

        if (bNeedSave)
        {
            vStatus.push_back(stRec);
        }
    }

    if (vStatus.size() > 0)
    {
        if (!m_pDao->AddDISStatus(vStatus))
        {
            TaskLog(LOG_ERROR, "save DIS ne_basic_status failed");
        }       
    }
}

VOID TaskSnmp::SaveRECInfo(UINT32 ulCurrTime)
{
    VECTOR<OMA_DEV_INFO_T>          vDevInfo;
    VECTOR<OMA_REC_STATUS_REC_T>    vStatus;
    OMA_REC_STATUS_REC_T            stRec;
    BOOL                            bNeedSave = FALSE;
    BOOL                            bOnline;
    std::string                     strNeID;

    GetDevInfo(OMC_DEV_TYPE_REC, vDevInfo);
    stRec.ulTime = gos_get_current_time();

    for (UINT32 i=0; i<vDevInfo.size(); i++)
    {
        strcpy(stRec.acNeID, vDevInfo[i].stNeBasicInfo.acNeID);
        memcpy(&stRec.stStatus, &vDevInfo[i].unDevStatus.stRecStatus, sizeof(stRec.stStatus));

        if (vDevInfo[i].stNeBasicInfo.ulLastOnlineTime == 0 ||
            (ulCurrTime - vDevInfo[i].stNeBasicInfo.ulLastOnlineTime) > g_stLocalCfg.ulOnlineCheckPeriod)
        {
            stRec.stStatus.bOnline = FALSE;
        }
        else
        {
            stRec.stStatus.bOnline = TRUE;
        }

        strNeID = stRec.acNeID;
        bNeedSave = TRUE;
        if (m_mOnline.find(strNeID) == m_mOnline.end())
        {
            m_mOnline[strNeID] = stRec.stStatus.bOnline;
            if (!stRec.stStatus.bOnline) // 把设备在线状态添加到 map 中，当前设备不在线则不存储
            {
                bNeedSave = FALSE;
            }
        }
        else
        {
            bOnline = m_mOnline[strNeID];
            if ((bOnline == stRec.stStatus.bOnline) && !bOnline)    // 当前不在线并且以前也不在线时不存储
            {
                bNeedSave = FALSE;
            }
        }

        if (bNeedSave)
        {
            vStatus.push_back(stRec);
        }
    }


    if (vStatus.size() > 0)
    {
        if (!m_pDao->AddRECStatus(vStatus))
        {
            TaskLog(LOG_ERROR, "save REC ne_basic_status failed");
        }       
    }
}

VOID TaskSnmp::SaveDevInfo()
{
    UINT32  ulCurrTime = gos_get_uptime();

    if ((m_ulLastSaveTime+g_stLocalCfg.ulSaveDevInfoPeriod) > ulCurrTime)
    {
        return;
    }

	SaveTAUInfo(ulCurrTime);
    SaveTXInfo(ulCurrTime);
    SaveFXInfo(ulCurrTime);
	SaveDCInfo(ulCurrTime);
	SaveDISInfo(ulCurrTime);
	SaveRECInfo(ulCurrTime);
    m_ulLastSaveTime = ulCurrTime;

#if 0
    static UINT32   ulLastSaveTime = 0;
    UINT32          ulCurrTime = gos_get_uptime();

    if ((ulLastSaveTime+g_stLocalCfg.ulSaveDevInfoPeriod) > ulCurrTime)
    {
        return;
    }

    ulLastSaveTime = ulCurrTime;
    // 存储 oma 设备信息的容器
    VECTOR<OMA_DEV_INFO_T>          vDevInfo;
    VECTOR<OMA_DEV_INFO_T>          vTauInfo;
    VECTOR<OMA_TX_STATUS_REC_T>     vTxStatus;
    VECTOR<OMA_TAU_STATUS_REC_T>    vTauStatus;
    OMA_TX_STATUS_REC_T             stRec;
    OMA_TAU_STATUS_REC_T            stTauRec;
    static std::map<std::string, BOOL>  mOnline;
    BOOL                            bNeedSave = FALSE;
    BOOL                            bOnline;
    std::string                     strNeID;

    stRec.ulTime = gos_get_current_time();
    stTauRec.ulTime = gos_get_current_time();

    // 获取 oma 的信息
    GetDevInfo(OMC_DEV_TYPE_TX, vDevInfo);
    GetDevInfo(OMC_DEV_TYPE_TAU, vTauInfo);

    for (UINT32 i=0; i<vDevInfo.size(); i++)
    {
        strcpy(stRec.acNeID, vDevInfo[i].stNeBasicInfo.acNeID);
        memcpy(&stRec.stStatus, &vDevInfo[i].unDevStatus.stTxStatus, sizeof(stRec.stStatus));

        if (vDevInfo[i].stNeBasicInfo.ulLastOnlineTime == 0 ||
            (ulCurrTime - vDevInfo[i].stNeBasicInfo.ulLastOnlineTime) > g_stLocalCfg.ulOnlineCheckPeriod)
        {
            stRec.stStatus.bOnline = FALSE;
        }
        else
        {
            stRec.stStatus.bOnline = TRUE;
        }

        strNeID = stRec.acNeID;
        bNeedSave = TRUE;
        if (mOnline.find(strNeID) == mOnline.end())
        {
            mOnline[strNeID] = stRec.stStatus.bOnline;
            if (!stRec.stStatus.bOnline) // 把设备在线状态添加到 map 中，当前设备不在线则不存储
            {
                bNeedSave = FALSE;
            }
        }
        else
        {
            bOnline = mOnline[strNeID]; // 获取之前在线状态
            /*
                1.之前不在线，当前在线，则存储
                2.之前不在线，当前也不在线，则不存储 !bOnline && !stRec.stStatus.bOnline
                3.之前在线，当前不在线，则不存储    bOnline && !stRec.stStatus.bOnline
                4.之前在线，当前也在线，则存储
            */
            if ((bOnline == stRec.stStatus.bOnline) && !bOnline)    // 当前不在线并且以前也不在线时不存储
            {
                bNeedSave = FALSE;
            }
            mOnline[strNeID] = stRec.stStatus.bOnline; // 更新设备当前在线状态
        }

        if (bNeedSave)
        {
            vTxStatus.push_back(stRec);
        }
    }

    if (!m_pDao->AddTxStatus(vTxStatus))
    {
        TaskLog(LOG_ERROR, "save TX dev info failed");
    }

    for (UINT32 i=0; i<vTauInfo.size(); i++)
    {
        strcpy(stTauRec.acNeID, vTauInfo[i].stNeBasicInfo.acNeID);
        memcpy(&stTauRec.stStatus, &vTauInfo[i].unDevStatus.stTauStatus, sizeof(stTauRec.stStatus));

        if (vTauInfo[i].stNeBasicInfo.ulLastOnlineTime == 0 ||
            (ulCurrTime - vTauInfo[i].stNeBasicInfo.ulLastOnlineTime) > g_stLocalCfg.ulOnlineCheckPeriod)
        {
            stTauRec.stStatus.bOnline = FALSE;
        }
        else
        {
            stTauRec.stStatus.bOnline = TRUE;
        }

        strNeID = stTauRec.acNeID;
        bNeedSave = TRUE;
        if (mOnline.find(strNeID) == mOnline.end())
        {
            mOnline[strNeID] = stTauRec.stStatus.bOnline;
            if (!stTauRec.stStatus.bOnline) // 把设备在线状态添加到 map 中，当前设备不在线则不存储
            {
                bNeedSave = FALSE;
            }
        }
        else
        {
            bOnline = mOnline[strNeID];
            if ((bOnline == stTauRec.stStatus.bOnline) && !bOnline)    // 当前不在线并且以前也不在线时不存储
            {
                bNeedSave = FALSE;
            }
        }

        if (bNeedSave)
        {
            vTauStatus.push_back(stTauRec);
        }
    }

    if (!m_pDao->AddTauStatus(vTauStatus))
    {
        TaskLog(LOG_ERROR, "save TAU dev info failed");
    }

    if (!m_pDao->AddTrafficStatus(vTauStatus))
    {
        TaskLog(LOG_ERROR, "save TAU traffic status failed");
    }
#endif
}

// 监听 snmp 报文
BOOL TaskSnmp::ListenSnmp(UINT16 usSnmpPort, UINT16 usTrapPort)
{
    INT32               iRecvSize;
    SOCKET              stSocket = INVALID_SOCKET;
    SOCKADDR_IN         stSockAddr = {0};
    SOCKADDR_IN         stClientAddr;
    UINT32              ulTimeout = 2000;
    UINT32              ulFlag = 1;
    static UINT8        aucMsgBuf[64*1024];
    SNMP_GET_RSP_T      stSnmpGetRsp;
    BOOL                bIsTrapMsg;  //标志位  true:trapMsg  flase:snmpMsg

    snmp_init_socket();
    // snmp socket
    if (g_stSnmpSocket == INVALID_SOCKET)
    {
        stSockAddr.sin_family = AF_INET;
        stSockAddr.sin_port   = htons(usSnmpPort);
        memset(&stSockAddr.sin_addr, 0, 4);

        stSocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (stSocket == INVALID_SOCKET)
        {
            TaskLog(LOG_ERROR, "Create snmp socket failed!");
            return FALSE;
        }

        if (setsockopt(stSocket, SOL_SOCKET, SO_REUSEADDR, (CHAR*)&ulFlag, sizeof(ulFlag)) == SOCKET_ERROR)
        {
            TaskLog(LOG_ERROR, "set snmp socket option failed!");
            closesocket (stSocket);

            return FALSE;
        }

        if (bind (stSocket, (SOCKADDR *) &stSockAddr, sizeof (SOCKADDR)) == SOCKET_ERROR)
        {
            TaskLog(LOG_ERROR, "bind snmp socket failed!");
            closesocket(stSocket);
            return FALSE;
        }

        g_stSnmpSocket = stSocket;
    }

    // trap socket
    if (m_stTrapSocket == INVALID_SOCKET)
    {
        stSockAddr.sin_family = AF_INET;
        stSockAddr.sin_port   = htons(usTrapPort);
        memset(&stSockAddr.sin_addr, 0, 4);

        stSocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (stSocket == INVALID_SOCKET)
        {
            TaskLog(LOG_ERROR, "Create trap socket failed!");
            CLOSE_SOCKET(g_stSnmpSocket);

            return FALSE;
        }

        if (setsockopt(stSocket, SOL_SOCKET, SO_REUSEADDR, (CHAR*)&ulFlag, sizeof(ulFlag)) == SOCKET_ERROR)
        {
            TaskLog(LOG_ERROR, "set trap socket option failed!");
            closesocket (stSocket);
            CLOSE_SOCKET(g_stSnmpSocket);

            return FALSE;
        }

        if (bind (stSocket, (SOCKADDR *) &stSockAddr, sizeof (SOCKADDR)) == SOCKET_ERROR)
        {
            TaskLog(LOG_ERROR, "bind trap socket failed!");
            closesocket(stSocket);
            CLOSE_SOCKET(g_stSnmpSocket);

            return FALSE;
        }

        m_stTrapSocket = stSocket;
    }

    while(1)
    {
        memset(aucMsgBuf, 0, sizeof(aucMsgBuf));

        // 将收到的 snmp 报文放入 aucMsgBuf
        iRecvSize = RecvSnmpMsg(g_stSnmpSocket, m_stTrapSocket, aucMsgBuf, sizeof(aucMsgBuf), ulTimeout, &stClientAddr, bIsTrapMsg);
 
        /* 失败 */
        if (iRecvSize < 0)
        {
            break;
        }


        //保存获取到的设备信息
        //SaveDevInfo();

        /* 超时 */
        if (iRecvSize == 0)
        {
            // 检查告警产生和告警消除临时文件
            StoreAlarmRaiseData();
            StoreAlarmClearData();
            continue;
        }

        /* 消息解析和处理 */
        if (bIsTrapMsg)
        {
            IP_ADDR_T           stDevAddr;
            SNMP_TRAP2_PDU_T    stPDU = {0};

            memcpy(stDevAddr.aucIP, &stClientAddr.sin_addr, sizeof(stDevAddr.aucIP));
            stDevAddr.usPort = ntohs(stClientAddr.sin_port);

            memcpy(&stSnmpGetRsp.stClientAddr, &stDevAddr, sizeof(stDevAddr));

            /* 消息解析和处理 */
            //memset(&stPDU, 0, sizeof(stPDU));
            if (snmp_parse_trap2_pdu(aucMsgBuf, iRecvSize, &stPDU) == 0)
            {
                HandleTrapMsg(&stDevAddr, (VOID*)&stPDU);
            }

            snmp_free_trap2_pdu(&stPDU);
        }
        else if (SnmpParseGetRsp(aucMsgBuf, iRecvSize, stSnmpGetRsp))
        {
            memcpy(stSnmpGetRsp.stClientAddr.aucIP, &stClientAddr.sin_addr, sizeof(stSnmpGetRsp.stClientAddr.aucIP));
            stSnmpGetRsp.stClientAddr.usPort = ntohs(stClientAddr.sin_port);
            // 处理snmp报文
            HandleSnmpMsg(stSnmpGetRsp);
            SnmpFreeGetRsp(stSnmpGetRsp);
        }
        else
        {  
            SnmpFreeGetRsp(stSnmpGetRsp);
            TaskLog(LOG_ERROR, "unknown snmp msg");
        }
    }

    return TRUE;
}

BOOL GetSnmpValue(VECTOR<SNMP_DATA_T> &vData, SNMP_OID *pstOID, UINT32 ulOIDLen, CHAR *szValue, UINT32 ulMaxLen, BOOL *pbNoSuchInstance=NULL)
{
    if (pbNoSuchInstance)
    {
        *pbNoSuchInstance = FALSE;
    }

    INT32      iLen;

    for (UINT32 i=0; i<vData.size(); i++)
    {
        if (memcmp(pstOID, vData[i].astOID, ulOIDLen) == 0)
        {
            if (pbNoSuchInstance &&
                vData[i].ulDataType == SNMP_ERR_NOSUCHINSTANCE)
            {
                *pbNoSuchInstance = TRUE;
                return FALSE;
            }

            if (vData[i].ulDataType != ASN_OCTET_STR)
            {
                return FALSE;
            }

            if (!vData[i].szValue)
            {
                return FALSE;
            }

            iLen = MIN(vData[i].ulDataLen, (INT32)(ulMaxLen-1));
            if (iLen > 0)
            {
                strncpy(szValue, vData[i].szValue, iLen);
                szValue[iLen] = '\0';
            }
            else
            {
                *szValue = '\0';
            }

            return TRUE;
        }
    }

    return FALSE;
}

BOOL GetSnmpValue(VECTOR<SNMP_DATA_T> &vData, SNMP_OID *pstOID, UINT32 ulOIDLen, UINT8 *pucValue, UINT32 ulMaxLen, BOOL *pbNoSuchInstance=NULL)
{
    if (pbNoSuchInstance)
    {
        *pbNoSuchInstance = FALSE;
    }

    for (UINT32 i=0; i<vData.size(); i++)
    {
        if (memcmp(pstOID, vData[i].astOID, ulOIDLen) == 0)
        {
            if (pbNoSuchInstance &&
                vData[i].ulDataType == SNMP_ERR_NOSUCHINSTANCE)
            {
                *pbNoSuchInstance = TRUE;
                return FALSE;
            }

            if (vData[i].ulDataLen > ulMaxLen)
            {
                return FALSE;
            }

            if (!vData[i].szValue)
            {
                return FALSE;
            }

            memcpy(pucValue, vData[i].szValue, vData[i].ulDataLen);
            return TRUE;
        }
    }

    return FALSE;
}

BOOL GetSnmpValue(VECTOR<SNMP_DATA_T> &vData, SNMP_OID *pstOID, UINT32 ulOIDLen, UINT32 *pulValue, BOOL *pbNoSuchInstance=NULL)
{
    if (pbNoSuchInstance)
    {
        *pbNoSuchInstance = FALSE;
    }

    for (UINT32 i=0; i<vData.size(); i++)
    {
        if (memcmp(pstOID, vData[i].astOID, ulOIDLen) == 0)
        {
            if (pbNoSuchInstance &&
                vData[i].ulDataType == SNMP_ERR_NOSUCHINSTANCE)
            {
                *pbNoSuchInstance = TRUE;
                return FALSE;
            }

            if (vData[i].ulDataType != ASN_GAUGE     &&
                vData[i].ulDataType != ASN_TIMETICKS &&
                vData[i].ulDataType != ASN_INTEGER   &&
                vData[i].ulDataType != ASN_COUNTER   &&
                vData[i].ulDataType != ASN_OCTET_STR &&
                vData[i].ulDataType != ASN_OBJECT_ID &&
                vData[i].ulDataType != ASN_CONTEXT   &&
                vData[i].ulDataType != ASN_COUNTER64)
            {
                GosLog(LOG_ERROR, "GetSnmpValue: %x", vData[i].ulDataType);
                return FALSE;
            }

            if (!vData[i].szValue)
            {
                return FALSE;
            }
            //TaskLog(LOG_ERROR,"GetSnmpValue32: %u",GET_INT(vData[i].szValue));
            *pulValue = GET_INT(vData[i].szValue);
            return TRUE;
        }
    }

    return FALSE;
}


BOOL GetSnmpValue(VECTOR<SNMP_DATA_T> &vData, SNMP_OID *pstOID, UINT32 ulOIDLen, UINT64 *pu64Value, BOOL *pbNoSuchInstance=NULL)
{
    if (pbNoSuchInstance)
    {
        *pbNoSuchInstance = FALSE;
    }

    for (UINT32 i=0; i<vData.size(); i++)
    {
        if (memcmp(pstOID, vData[i].astOID, ulOIDLen) == 0)
        {
            if (pbNoSuchInstance &&
                vData[i].ulDataType == SNMP_ERR_NOSUCHINSTANCE)
            {
                *pbNoSuchInstance = TRUE;
                return FALSE;
            }

            if (vData[i].ulDataType != ASN_COUNTER64)
            {
                GosLog(LOG_ERROR, "GetSnmpValue: %x", vData[i].ulDataType);
                return FALSE;
            }

            if (!vData[i].szValue)
            {
                return FALSE;
            }

            *pu64Value = GET_UINT64(vData[i].szValue);
            return TRUE;
        }
    }

    return FALSE;
}

VOID TaskSnmp::OnGetSysInfoRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData)
{
    UINT32              ulUptime;
    UINT32              ulClusterStatus = CLUSTER_STATUS_MASTER;
    UINT32              ulDevType = stDevInfo.stNeBasicInfo.ulDevType;
    CHAR                acSoftwareVer[64] = {0};
    CHAR                acHardwareVer[64] = {0};
    CHAR                acRadioVer[64] = {0};
    CHAR                acAndroidVer[64] = {0};
    CHAR                acSIMIMSI[32] = {0};
    OMA_RES_STATUS_T    stResStatus = {0}; //保存收到的设备状态

    // 所有网元共同的基础信息
    if (!GetSnmpValue(vData, oid_sysUptime,         sizeof(oid_sysUptime),          &ulUptime)  ||
        !GetSnmpValue(vData, oid_CpuUsage,          sizeof(oid_CpuUsage),           &stResStatus.ulCpuUsage)  ||
        !GetSnmpValue(vData, oid_MemUsage,          sizeof(oid_MemUsage),           &stResStatus.ulMemUsage)  ||
        !GetSnmpValue(vData, oid_DiskUsage,         sizeof(oid_DiskUsage),          &stResStatus.ulDiskUsage))
    {
        TaskLog(LOG_ERROR, "get dev %s(%s) info failed", stDevInfo.stNeBasicInfo.acDevName,stDevInfo.stNeBasicInfo.acDevMac);
        return;
    }

    if (ulDevType == OMC_DEV_TYPE_TX)
    {
        if (!GetSnmpValue(vData, oid_sysHardwareVer,    sizeof(oid_sysHardwareVer),     acHardwareVer,  sizeof(acHardwareVer)) ||
            !GetSnmpValue(vData, oid_sysSoftwareVer,    sizeof(oid_sysSoftwareVer),     acSoftwareVer,  sizeof(acSoftwareVer)) ||
            !GetSnmpValue(vData, oid_CpuTemp,           sizeof(oid_CpuTemp),            (UINT32 *)&stResStatus.iDevTemp)       ||
            !GetSnmpValue(vData, oid_sysRadioVer,       sizeof(oid_sysRadioVer),        acRadioVer,     sizeof(acRadioVer))    ||
            !GetSnmpValue(vData, oid_sysAndroidVer,     sizeof(oid_sysAndroidVer),      acAndroidVer,   sizeof(acAndroidVer))  ||
            !GetSnmpValue(vData, oid_TCID,              sizeof(oid_TCID),               &stDevInfo.stNeBasicInfo.ulDevTC))
        {
            TaskLog(LOG_ERROR, "get TX %s (%s) basic info failed", stDevInfo.stNeBasicInfo.acDevName, stDevInfo.stNeBasicInfo.acDevMac);
            return;
        }
        
        // 若上报的基础信息和数据库保存的信息不一致，则更新数据库保存的设备基础信息
        if (strcmp(acHardwareVer, stDevInfo.stNeBasicInfo.acHardwareVer) != 0 ||
            strcmp(acSoftwareVer, stDevInfo.stNeBasicInfo.acSoftwareVer) != 0 ||
            strcmp(acRadioVer,    stDevInfo.stNeBasicInfo.acRadioVer)    != 0 ||
            strcmp(acAndroidVer,  stDevInfo.stNeBasicInfo.acAndroidVer)  != 0 )
        {
            strcpy(stDevInfo.stNeBasicInfo.acHardwareVer, acHardwareVer);
            strcpy(stDevInfo.stNeBasicInfo.acSoftwareVer, acSoftwareVer);
            strcpy(stDevInfo.stNeBasicInfo.acRadioVer,    acRadioVer);
            strcpy(stDevInfo.stNeBasicInfo.acAndroidVer,  acAndroidVer);

            if (!m_pDao->AddNeBasicInfo(stDevInfo.stNeBasicInfo))
            {
                TaskLog(LOG_ERROR, "save TX %s (%s) basic info failed", stDevInfo.stNeBasicInfo.acDevName, stDevInfo.stNeBasicInfo.acDevMac);
            }
        }
    }
    else if (ulDevType == OMC_DEV_TYPE_FX)
    {
        if (!GetSnmpValue(vData, oid_sysHardwareVer,    sizeof(oid_sysHardwareVer),     acHardwareVer,  sizeof(acHardwareVer)) ||
            !GetSnmpValue(vData, oid_sysSoftwareVer,    sizeof(oid_sysSoftwareVer),     acSoftwareVer,  sizeof(acSoftwareVer)) ||
            !GetSnmpValue(vData, oid_CpuTemp,           sizeof(oid_CpuTemp),            (UINT32 *)&stResStatus.iDevTemp)       ||
            !GetSnmpValue(vData, oid_sysRadioVer,       sizeof(oid_sysRadioVer),        acRadioVer,     sizeof(acRadioVer))    ||
            !GetSnmpValue(vData, oid_sysAndroidVer,     sizeof(oid_sysAndroidVer),      acAndroidVer,   sizeof(acAndroidVer)))
        {
            TaskLog(LOG_ERROR, "get FX %s (%s) basic info failed", stDevInfo.stNeBasicInfo.acDevName, stDevInfo.stNeBasicInfo.acDevMac);
            return;
        }
        
        // 若上报的基础信息和数据库保存的信息不一致，则更新数据库保存的设备基础信息
        if (strcmp(acHardwareVer, stDevInfo.stNeBasicInfo.acHardwareVer) != 0 ||
            strcmp(acSoftwareVer, stDevInfo.stNeBasicInfo.acSoftwareVer) != 0 ||
            strcmp(acRadioVer,    stDevInfo.stNeBasicInfo.acRadioVer)    != 0 ||
            strcmp(acAndroidVer,  stDevInfo.stNeBasicInfo.acAndroidVer)  != 0 )
        {
            strcpy(stDevInfo.stNeBasicInfo.acHardwareVer, acHardwareVer);
            strcpy(stDevInfo.stNeBasicInfo.acSoftwareVer, acSoftwareVer);
            strcpy(stDevInfo.stNeBasicInfo.acRadioVer,    acRadioVer);
            strcpy(stDevInfo.stNeBasicInfo.acAndroidVer,  acAndroidVer);

            if (!m_pDao->AddNeBasicInfo(stDevInfo.stNeBasicInfo))
            {
                TaskLog(LOG_ERROR, "save FX %s (%s) basic info failed", stDevInfo.stNeBasicInfo.acDevName, stDevInfo.stNeBasicInfo.acDevMac);
            }
        }
    }
    else if (ulDevType == OMC_DEV_TYPE_TAU)
    {
        // TAU上报软件版本和硬件版本使用的OID与二开使用的不一样
        if (!GetSnmpValue(vData, oid_hardwareVersion,    sizeof(oid_hardwareVersion),     acHardwareVer,  sizeof(acHardwareVer)) ||
            !GetSnmpValue(vData, oid_softwareVersion,    sizeof(oid_softwareVersion),     acSoftwareVer,  sizeof(acSoftwareVer)) ||
            !GetSnmpValue(vData, oid_CpuTemp,            sizeof(oid_CpuTemp),             (UINT32 *)&stResStatus.iDevTemp)       ||
            !GetSnmpValue(vData, oid_DiskLogsaveUsage,   sizeof(oid_DiskLogsaveUsage),    &stResStatus.ulDiskLogsaveUsage)       ||
            !GetSnmpValue(vData, oid_DiskPackageUsage,   sizeof(oid_DiskPackageUsage),    &stResStatus.ulDiskPackageUsage)       ||
            !GetSnmpValue(vData, oid_DiskModuleUsage,    sizeof(oid_DiskModuleUsage),     &stResStatus.ulDiskModuleUsage)        ||
            !GetSnmpValue(vData, oid_SIMIMSI,            sizeof(oid_SIMIMSI),             acSIMIMSI,      sizeof(acSIMIMSI)))
        {
            TaskLog(LOG_ERROR, "get TAU %s(%s) SysInfo failed",stDevInfo.stNeBasicInfo.acDevName, stDevInfo.stNeBasicInfo.acDevMac);
            return;
        }

        // SIMIMSI若发送改变，则更新至数据库
        if (strcmp(acHardwareVer, stDevInfo.stNeBasicInfo.acHardwareVer) != 0 ||
            strcmp(acSoftwareVer, stDevInfo.stNeBasicInfo.acSoftwareVer) != 0 ||
            strcmp(acSIMIMSI, stDevInfo.stNeBasicInfo.acSIMIMSI) != 0)
        {
            strcpy(stDevInfo.stNeBasicInfo.acHardwareVer, acHardwareVer);
            strcpy(stDevInfo.stNeBasicInfo.acSoftwareVer, acSoftwareVer);
            strcpy(stDevInfo.stNeBasicInfo.acSIMIMSI,  acSIMIMSI);

            if (!m_pDao->AddNeBasicInfo(stDevInfo.stNeBasicInfo))
            {
                TaskLog(LOG_ERROR, "save dev %s(%s) basic info failed", stDevInfo.stNeBasicInfo.acDevName,stDevInfo.stNeBasicInfo.acDevMac);
            }
        }
    
    }
    else if (ulDevType == OMC_DEV_TYPE_DIS) 
    {
        if (!GetSnmpValue(vData, oid_sysHardwareVer,    sizeof(oid_sysHardwareVer),     acHardwareVer,  sizeof(acHardwareVer)) ||
            !GetSnmpValue(vData, oid_sysSoftwareVer,    sizeof(oid_sysSoftwareVer),     acSoftwareVer,  sizeof(acSoftwareVer)) ||
            !GetSnmpValue(vData, oid_ClusterStatus, sizeof(oid_ClusterStatus), &ulClusterStatus))
        {
            TaskLog(LOG_ERROR, "get %s (%s) cluster info failed", stDevInfo.stNeBasicInfo.acDevName, stDevInfo.stNeBasicInfo.acDevMac);
            return;
        }

        // 若硬软件版本号改变，则更新至数据库
        if (strcmp(acHardwareVer, stDevInfo.stNeBasicInfo.acHardwareVer) != 0 ||
            strcmp(acSoftwareVer, stDevInfo.stNeBasicInfo.acSoftwareVer) != 0)
        {
            strcpy(stDevInfo.stNeBasicInfo.acHardwareVer, acHardwareVer);
            strcpy(stDevInfo.stNeBasicInfo.acSoftwareVer, acSoftwareVer);

            if (!m_pDao->AddNeBasicInfo(stDevInfo.stNeBasicInfo))
            {
                TaskLog(LOG_ERROR, "save dev %s(%s) basic info failed", stDevInfo.stNeBasicInfo.acDevName,stDevInfo.stNeBasicInfo.acDevMac);
            }
        }
    }
	else if (ulDevType == OMC_DEV_TYPE_REC)
	{
        if (!GetSnmpValue(vData, oid_sysHardwareVer,    sizeof(oid_sysHardwareVer),     acHardwareVer,  sizeof(acHardwareVer)) ||
            !GetSnmpValue(vData, oid_sysSoftwareVer,    sizeof(oid_sysSoftwareVer),     acSoftwareVer,  sizeof(acSoftwareVer)))
        {
            TaskLog(LOG_ERROR, "get %s (%s) version info failed", stDevInfo.stNeBasicInfo.acDevName, stDevInfo.stNeBasicInfo.acDevMac);
            return;
        }

        // 若硬软件版本号改变，则更新至数据库
        if (strcmp(acHardwareVer, stDevInfo.stNeBasicInfo.acHardwareVer) != 0 ||
            strcmp(acSoftwareVer, stDevInfo.stNeBasicInfo.acSoftwareVer) != 0)
        {
            strcpy(stDevInfo.stNeBasicInfo.acHardwareVer, acHardwareVer);
            strcpy(stDevInfo.stNeBasicInfo.acSoftwareVer, acSoftwareVer);

            if (!m_pDao->AddNeBasicInfo(stDevInfo.stNeBasicInfo))
            {
                TaskLog(LOG_ERROR, "save dev %s(%s) basic info failed", stDevInfo.stNeBasicInfo.acDevName,stDevInfo.stNeBasicInfo.acDevMac);
            }
        }	
	}

    stDevInfo.stNeInfo.ulUptime = ulUptime/100;     // uptime单位10毫秒
    if (ulDevType == OMC_DEV_TYPE_TX)
    {
        memcpy(&stDevInfo.unDevStatus.stTxStatus.stResStatus, &stResStatus, sizeof(stResStatus));
    }
    else if (ulDevType == OMC_DEV_TYPE_FX)
    {
        memcpy(&stDevInfo.unDevStatus.stFxStatus.stResStatus, &stResStatus, sizeof(stResStatus));
    }
    else if (ulDevType == OMC_DEV_TYPE_TAU)
    {
        memcpy(&stDevInfo.unDevStatus.stTauStatus, &stResStatus, sizeof(stResStatus));
    }
    else if (ulDevType == OMC_DEV_TYPE_DC)
    {
        memcpy(&stDevInfo.unDevStatus.stDcStatus.stResStatus, &stResStatus, sizeof(stResStatus));
    }
    else if (ulDevType == OMC_DEV_TYPE_DIS)
    {
        memcpy(&stDevInfo.unDevStatus.stDisStatus.stResStatus, &stResStatus, sizeof(stResStatus));
        stDevInfo.unDevStatus.stDisStatus.ulClusterStatus = ulClusterStatus;
    }
    else if (ulDevType == OMC_DEV_TYPE_REC)
    {
        memcpy(&stDevInfo.unDevStatus.stRecStatus.stResStatus, &stResStatus, sizeof(stResStatus));
    }

    SetDevInfo(stDevInfo);
}

// 获取 Dis 状态信息的 response 包
VOID TaskSnmp::OnGetDISStatusRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData)
{
    OMA_DIS_STATUS_T    stStatus = {0};

    memcpy(&stStatus, &stDevInfo.unDevStatus.stDisStatus, sizeof(stStatus));

    if (!GetSnmpValue(vData, oid_ATSStatus, sizeof(oid_ATSStatus), (UINT32*)&stStatus.bATSStatus)||
        !GetSnmpValue(vData, oid_NTPStatus, sizeof(oid_NTPStatus), (UINT32*)&stStatus.bNTPStatus)||
        !GetSnmpValue(vData, oid_LTEStatus, sizeof(oid_LTEStatus), (UINT32*)&stStatus.bLTEStatus) )
    {
        TaskLog(LOG_ERROR, "get dev(%s) status failed", stDevInfo.stNeBasicInfo.acDevName);
        return;
    }

    if (memcmp(&stStatus, &stDevInfo.unDevStatus.stDisStatus, sizeof(stStatus)) != 0)
    {
        memcpy(&stDevInfo.unDevStatus.stDisStatus, &stStatus, sizeof(stStatus));
        SetDevInfo(stDevInfo);
    }
}

VOID TaskSnmp::OnGetDCStatusRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData)
{
    OMA_DC_STATUS_T stStatus = {0};

    memcpy(&stStatus, &stDevInfo.unDevStatus.stDcStatus, sizeof(stStatus));

    if (!GetSnmpValue(vData, oid_ATSStatus, sizeof(oid_ATSStatus), (UINT32*)&stStatus.bATSStatus)||
        !GetSnmpValue(vData, oid_NTPStatus, sizeof(oid_NTPStatus), (UINT32*)&stStatus.bNTPStatus)||
        !GetSnmpValue(vData, oid_LTEStatus, sizeof(oid_LTEStatus), (UINT32*)&stStatus.bLTEStatus) )
    {
        TaskLog(LOG_ERROR, "get dev(%s) status failed", stDevInfo.stNeBasicInfo.acDevName);
        return;
    }

    if (memcmp(&stStatus, &stDevInfo.unDevStatus.stDcStatus, sizeof(stStatus)) != 0)
    {
        memcpy(&stDevInfo.unDevStatus.stDcStatus, &stStatus, sizeof(stStatus));
        SetDevInfo(stDevInfo);
    }
}

VOID TaskSnmp::OnGetTXStatusRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData)
{
    OMA_TX_RUN_STATUS_T     stStatus = {0};

    if (!GetSnmpValue(vData, oid_GatewayReachable,      sizeof(oid_GatewayReachable),       (UINT32*)&stStatus.ulGatewayReachable)||
        !GetSnmpValue(vData, oid_MDCRegStatus,          sizeof(oid_MDCRegStatus),           (UINT32*)&stStatus.ulMDCRegStatus)||
        !GetSnmpValue(vData, oid_OMCLinkStatus,         sizeof(oid_OMCLinkStatus),          (UINT32*)&stStatus.ulOMCLinkStatus)||
        !GetSnmpValue(vData, oid_DISLinkStatus,         sizeof(oid_DISLinkStatus),          (UINT32*)&stStatus.ulDISLinkStatus)||
        !GetSnmpValue(vData, oid_SelfCheckStatus,       sizeof(oid_SelfCheckStatus),        (UINT32*)&stStatus.ulSelfCheckStatus)||
        !GetSnmpValue(vData, oid_DevCoverStatus,        sizeof(oid_DevCoverStatus),         (UINT32*)&stStatus.ulDevCoverStatus)||
        !GetSnmpValue(vData, oid_DriveMode,             sizeof(oid_DriveMode),              (UINT32*)&stStatus.ulDriveMode)||
        !GetSnmpValue(vData, oid_EmergencyCallStatus,   sizeof(oid_EmergencyCallStatus),    (UINT32*)&stStatus.ulEmergencyCallStatus)||
        !GetSnmpValue(vData, oid_EmergencyBraceStatus,  sizeof(oid_EmergencyBraceStatus),   (UINT32*)&stStatus.ulEmergencyBraceStatus)||
        !GetSnmpValue(vData, oid_PISLinkStatus,         sizeof(oid_PISLinkStatus),          (UINT32*)&stStatus.ulIPHLinkStatus)||
        !GetSnmpValue(vData, oid_PALinkStatus,          sizeof(oid_PALinkStatus),           (UINT32*)&stStatus.ulPALinkStatus) )
    {
        TaskLog(LOG_ERROR, "get dev(%s) status failed", stDevInfo.stNeBasicInfo.acDevName);
        return;
    }

/*  strcpy(stStatus.acNeID, stNEBasicInfo.acNeID);
    sprintf(stStatus.acNTPAddr, IP_FMT, IP_ARG(aucNTPAddr)); */

    if (stStatus.ulGatewayReachable     > 1 ||
        stStatus.ulMDCRegStatus         > 1 ||
        stStatus.ulOMCLinkStatus        > 1 ||
        stStatus.ulDISLinkStatus        > 1 ||
        stStatus.ulSelfCheckStatus      > 1 ||
        stStatus.ulDevCoverStatus       > 1 ||
        stStatus.ulDriveMode            > 1 ||
        stStatus.ulEmergencyCallStatus  > 1 ||
        stStatus.ulEmergencyBraceStatus > 1 ||
        stStatus.ulIPHLinkStatus        > 1 ||
        stStatus.ulPALinkStatus         > 1)
    {
        TaskLog(LOG_ERROR, "invalid dev status of %s", stDevInfo.stNeBasicInfo.acNeID);
        return;
    }

    stDevInfo.unDevStatus.stTxStatus.ulRunStatus = ConvertTxRunStatus(stStatus);

    SetDevInfo(stDevInfo);
}

VOID TaskSnmp::OnGetFXStatusRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData)
{
    OMA_FX_RUN_STATUS_T     stStatus = {0};

    if (!GetSnmpValue(vData, oid_GatewayReachable,      sizeof(oid_GatewayReachable),       (UINT32*)&stStatus.ulGatewayReachable)    ||
        !GetSnmpValue(vData, oid_MDCRegStatus,          sizeof(oid_MDCRegStatus),           (UINT32*)&stStatus.ulMDCRegStatus)        ||
        !GetSnmpValue(vData, oid_OMCLinkStatus,         sizeof(oid_OMCLinkStatus),          (UINT32*)&stStatus.ulOMCLinkStatus)       ||
        !GetSnmpValue(vData, oid_DISLinkStatus,         sizeof(oid_DISLinkStatus),          (UINT32*)&stStatus.ulDISLinkStatus)       ||
        !GetSnmpValue(vData, oid_SelfCheckStatus,       sizeof(oid_SelfCheckStatus),        (UINT32*)&stStatus.ulSelfCheckStatus)     ||
        !GetSnmpValue(vData, oid_EmergencyCallStatus,   sizeof(oid_EmergencyCallStatus),    (UINT32*)&stStatus.ulEmergencyCallStatus) ||
        !GetSnmpValue(vData, oid_PALinkStatus,          sizeof(oid_PALinkStatus),           (UINT32*)&stStatus.ulPALinkStatus) )
    {
        TaskLog(LOG_ERROR, "get dev(%s) status failed", stDevInfo.stNeBasicInfo.acDevName);
        return;
    }


    if (stStatus.ulGatewayReachable     > 1 ||
        stStatus.ulMDCRegStatus         > 1 ||
        stStatus.ulOMCLinkStatus        > 1 ||
        stStatus.ulDISLinkStatus        > 1 ||
        stStatus.ulSelfCheckStatus      > 1 ||
        stStatus.ulEmergencyCallStatus  > 1 ||
        stStatus.ulPALinkStatus         > 1)
    {
        TaskLog(LOG_ERROR, "invalid dev status of %s", stDevInfo.stNeBasicInfo.acNeID);
        return;
    }

    stDevInfo.unDevStatus.stFxStatus.ulRunStatus = ConvertFxRunStatus(stStatus);

    SetDevInfo(stDevInfo);
}

VOID TaskSnmp::OnGetLteStatusRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData)
{
    OMA_LTE_STATUS_T    stStatus = {0};
    CHAR                acPCI[8] = {0};

    if (vData.size() < 5)
    {
        return;
    }

    if (!GetSnmpValue(vData, oid_LtePCI,    sizeof(oid_LtePCI),   acPCI, sizeof(acPCI))     ||
        !GetSnmpValue(vData, oid_StationID, sizeof(oid_StationID), &stStatus.ulStationID)   ||
        !GetSnmpValue(vData, oid_LteRSSI,   sizeof(oid_LteRSSI), (UINT32*)&stStatus.iRSSI)  ||
        !GetSnmpValue(vData, oid_LteRSRP,   sizeof(oid_LteRSRP), (UINT32*)&stStatus.iRSRP)  ||
        !GetSnmpValue(vData, oid_LteRSRQ,   sizeof(oid_LteRSRQ), (UINT32*)&stStatus.iRSRQ)  ||
        !GetSnmpValue(vData, oid_LteSINR,   sizeof(oid_LteSINR), (UINT32*)&stStatus.iSINR))
    {
        TaskLog(LOG_ERROR, "OnGetLteStatusRsp: get dev(%s) LTE status failed", stDevInfo.stNeBasicInfo.acNeID);
        return;
    }

    if (!gos_atou(acPCI, &stStatus.ulPCI))
    {
        TaskLog(LOG_ERROR, "invalid PCI(%s)", acPCI);
        return;
    }

    if (stDevInfo.stNeBasicInfo.ulDevType == OMC_DEV_TYPE_TX)
    {
        memcpy(&stDevInfo.unDevStatus.stTxStatus.stLteStatus, &stStatus, sizeof(stStatus));
    }
    else if (stDevInfo.stNeBasicInfo.ulDevType == OMC_DEV_TYPE_FX)
    {
        memcpy(&stDevInfo.unDevStatus.stFxStatus.stLteStatus, &stStatus, sizeof(stStatus));
    }
    else
    {
       TaskLog(LOG_ERROR, "OnGetLteStatusRsp: invalid DevType(%u)", stDevInfo.stNeBasicInfo.ulDevType);
    }

    SetDevInfo(stDevInfo);
}

VOID TaskSnmp::OnGetTAULteStatusRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData)
{
    OMA_LTE_STATUS_T    stStatus = {0};
    CHAR                acPCI[8] = {0};
    CHAR                acRSSI[8] = {0};
    CHAR                acRSRQ[8] = {0};

    if (vData.size() < 5)
    {
        return;
    }
    /*
    关于TAU4.0 LTE 信息字段类型差异：PCI、RSRI、RSRQ上报字段类型为字符串，RSRP、SINR上报字段类型为INT型。
    */
    if (!GetSnmpValue(vData, oid_LtePCI,    sizeof(oid_LtePCI),   acPCI,    sizeof(acPCI))  ||
        !GetSnmpValue(vData, oid_LteRSSI,   sizeof(oid_LteRSSI),  acRSSI,   sizeof(acRSSI)) ||
        !GetSnmpValue(vData, oid_LteRSRQ,   sizeof(oid_LteRSRQ),  acRSRQ,   sizeof(acRSRQ)) ||
        !GetSnmpValue(vData, oid_LteRSRP,   sizeof(oid_LteRSRP), (UINT32*)&stStatus.iRSRP)  ||
        !GetSnmpValue(vData, oid_LteSINR,   sizeof(oid_LteSINR), (UINT32*)&stStatus.iSINR))
    {
        TaskLog(LOG_ERROR, "OnGetLteStatusRsp: get dev(%s) LTE status failed", stDevInfo.stNeBasicInfo.acNeID);
        return;
    }

    if (!gos_atou(acPCI, &stStatus.ulPCI))
    {
        TaskLog(LOG_ERROR, "invalid PCI(%s)", acPCI);
        return;
    }

    if (!gos_atoi(acRSSI, &stStatus.iRSSI))
    {
        TaskLog(LOG_ERROR, "invalid RSSI(%s)", acRSSI);
        return;
    }

    if (!gos_atoi(acRSRQ, &stStatus.iRSRQ))
    {
        TaskLog(LOG_ERROR, "invalid RSSQ(%s)", acRSRQ);
        return;
    }

    memcpy(&stDevInfo.unDevStatus.stTauStatus.stLteStatus, &stStatus, sizeof(stStatus));

    SetDevInfo(stDevInfo);
}

VOID TaskSnmp::OnGetTrafficRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData)
{
    OMA_TRAFFIC_STATUS_T stTraffic={0};
    
    if (!GetSnmpValue(vData, oid_WANTxBytes,  sizeof(oid_WANTxBytes),  &stTraffic.u64WanTxBytes)  ||
        !GetSnmpValue(vData, oid_WANRxBytes,  sizeof(oid_WANRxBytes),  &stTraffic.u64WanRxBytes)  ||
        !GetSnmpValue(vData, oid_LAN1TxBytes, sizeof(oid_LAN1TxBytes), &stTraffic.u64Lan1TxBytes) ||
        !GetSnmpValue(vData, oid_LAN1RxBytes, sizeof(oid_LAN1RxBytes), &stTraffic.u64Lan1RxBytes) ||
        !GetSnmpValue(vData, oid_LAN2TxBytes, sizeof(oid_LAN2TxBytes), &stTraffic.u64Lan2TxBytes) ||
        !GetSnmpValue(vData, oid_LAN2RxBytes, sizeof(oid_LAN2RxBytes), &stTraffic.u64Lan2RxBytes) ||
        !GetSnmpValue(vData, oid_LAN3TxBytes, sizeof(oid_LAN3TxBytes), &stTraffic.u64Lan3TxBytes) ||
        !GetSnmpValue(vData, oid_LAN3RxBytes, sizeof(oid_LAN3RxBytes), &stTraffic.u64Lan3RxBytes) ||
        !GetSnmpValue(vData, oid_LAN4TxBytes, sizeof(oid_LAN4TxBytes), &stTraffic.u64Lan4TxBytes) ||
        !GetSnmpValue(vData, oid_LAN4RxBytes, sizeof(oid_LAN4RxBytes), &stTraffic.u64Lan4RxBytes) )
    {
        TaskLog(LOG_ERROR, "get dev(%s) Traffic Statistics failed", stDevInfo.stNeBasicInfo.acDevName);
        return;
    }

    memcpy(&stDevInfo.unDevStatus.stTauStatus.stTrafficStatus, &stTraffic, sizeof(stTraffic));
    SetDevInfo(stDevInfo);
}

VOID TaskSnmp::OnGetOperLogIndexRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData)
{
    UINT32              ulIndex = 0;

    if (!GetSnmpValue(vData, oid_OperLogIndex,  sizeof(oid_OperLogIndex), &ulIndex) )
    {
        TaskLog(LOG_ERROR, "get dev(%s) oper log index failed", stDevInfo.stNeBasicInfo.acDevName);
        return;
    }

    if (stDevInfo.stNeBasicInfo.ulDevType == OMC_DEV_TYPE_TX)
    {
        if (ulIndex >= stDevInfo.unDevStatus.stTxStatus.ulOperLogIndex)
        {
            stDevInfo.unDevStatus.stTxStatus.ulOperLogIndex = ulIndex;
            stDevInfo.unDevStatus.stTxStatus.bGetOperLogIndex = TRUE;
        }
    }
    else if (stDevInfo.stNeBasicInfo.ulDevType == OMC_DEV_TYPE_FX)
    {
        if (ulIndex >= stDevInfo.unDevStatus.stFxStatus.ulOperLogIndex)
        {
            stDevInfo.unDevStatus.stFxStatus.ulOperLogIndex = ulIndex;
            stDevInfo.unDevStatus.stFxStatus.bGetOperLogIndex = TRUE;
        }
    }

    TaskLog(LOG_DETAIL, "get dev(%s) oper log index %u", stDevInfo.stNeBasicInfo.acDevName, ulIndex);

    SetDevInfo(stDevInfo);
}

VOID TaskSnmp::OnGetOperLogRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData)
{
    OMA_OPERLOG_T   stLog = {0};
    OID_T           stOID;
    UINT32          ulOperLogIndex = 0;
	UINT32          ulLastOperLogIndex = 0;

    if (stDevInfo.stNeBasicInfo.ulDevType == OMC_DEV_TYPE_TX)
    {
        ulOperLogIndex = stDevInfo.unDevStatus.stTxStatus.ulOperLogIndex;
    }
    else if (stDevInfo.stNeBasicInfo.ulDevType == OMC_DEV_TYPE_FX)
    {
        ulOperLogIndex = stDevInfo.unDevStatus.stFxStatus.ulOperLogIndex;
    }
    else
    {
        return;
    }

    memcpy(stOID.astOID, oid_DevOperation,  sizeof(oid_DevOperation));
    stOID.ulOIDNum = ARRAY_SIZE(oid_DevOperation);
    stOID.astOID[stOID.ulOIDNum++] = ulOperLogIndex;

    BOOL    bNoSuchInstance = FALSE;

    if (!GetSnmpValue(vData, stOID.astOID, stOID.ulOIDNum*sizeof(SNMP_OID), stLog.acLogInfo, sizeof(stLog.acLogInfo), &bNoSuchInstance) )
    {
        if (!bNoSuchInstance)
        {
            TaskLog(LOG_ERROR, "get dev(%s) oper log failed", stDevInfo.stNeBasicInfo.acDevName);
        }

        return;
    }

    memcpy(stOID.astOID, oid_DevOperTime,   sizeof(oid_DevOperTime));
    stOID.ulOIDNum = ARRAY_SIZE(oid_DevOperTime);
    stOID.astOID[stOID.ulOIDNum++] = ulOperLogIndex;

    if (!GetSnmpValue(vData, stOID.astOID, stOID.ulOIDNum*sizeof(SNMP_OID), (UINT32*)&stLog.ulTime) )
    {
        TaskLog(LOG_ERROR, "get dev(%s) oper log failed", stDevInfo.stNeBasicInfo.acDevName);
        return;
    }

    memcpy(stOID.astOID, oid_LastOperLogIndex,   sizeof(oid_LastOperLogIndex));
    stOID.ulOIDNum = ARRAY_SIZE(oid_LastOperLogIndex);
    stOID.astOID[stOID.ulOIDNum++] = ulOperLogIndex;

    if (!GetSnmpValue(vData, stOID.astOID, stOID.ulOIDNum*sizeof(SNMP_OID), (UINT32*)&ulLastOperLogIndex))
    {
        TaskLog(LOG_ERROR, "get dev(%s) oper log failed", stDevInfo.stNeBasicInfo.acDevName);
        return;
    }

    if (stDevInfo.stNeBasicInfo.ulDevType == OMC_DEV_TYPE_TX)
    {
        stDevInfo.unDevStatus.stTxStatus.bGetOperLogIndex = FALSE;
		if (ulLastOperLogIndex == 1)
		{
			// 若获取到矢量表的最后一条记录,发送set报文通知网元,并且重置获取矢量表的index
			SendSetGetLastOperLogIndex(stDevInfo);
			stDevInfo.unDevStatus.stTxStatus.ulOperLogIndex = 0;
		}
    }
    else if (stDevInfo.stNeBasicInfo.ulDevType == OMC_DEV_TYPE_FX)
    {
        stDevInfo.unDevStatus.stFxStatus.bGetOperLogIndex = FALSE;
		if (ulLastOperLogIndex == 1)
		{
			// 若获取到矢量表的最后一条记录,发送set报文通知网元,并且重置获取矢量表的index
			SendSetGetLastOperLogIndex(stDevInfo);
			stDevInfo.unDevStatus.stFxStatus.ulOperLogIndex = 0;
		}
    }

    strcpy(stLog.acNeID, stDevInfo.stNeBasicInfo.acNeID);

    TaskLog(LOG_DETAIL, "get dev(%s) oper log: index=%u, time=%s, %s", stDevInfo.stNeBasicInfo.acDevName,
        ulOperLogIndex, gos_get_text_time(&stLog.ulTime), stLog.acLogInfo);

    if (!m_pDao->AddDevOperLog(stLog))
    {
        TaskLog(LOG_ERROR, "save dev(%s) oper log failed", stDevInfo.stNeBasicInfo.acDevName);
        return;
    }

//  AddDevOperLog(stLog);

    SetDevInfo(stDevInfo);
}

VOID TaskSnmp::OnGetDevFileIndexRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData)
{
    UINT32              ulIndex = 0;

    if (!GetSnmpValue(vData, oid_DevFileIndex,  sizeof(oid_DevFileIndex), &ulIndex) )
    {
        TaskLog(LOG_ERROR, "get dev(%s) file index failed", stDevInfo.stNeBasicInfo.acDevName);
        return;
    }

    if (stDevInfo.stNeBasicInfo.ulDevType == OMC_DEV_TYPE_TX)
    {
        if (ulIndex >= stDevInfo.unDevStatus.stTxStatus.ulFileIndex)
        {
            stDevInfo.unDevStatus.stTxStatus.ulFileIndex = ulIndex;
            stDevInfo.unDevStatus.stTxStatus.bGetFileIndex = TRUE;
        }
    }
    else if (stDevInfo.stNeBasicInfo.ulDevType == OMC_DEV_TYPE_FX)
    {
        if (ulIndex >= stDevInfo.unDevStatus.stFxStatus.ulFileIndex)
        {
            stDevInfo.unDevStatus.stFxStatus.ulFileIndex = ulIndex;
            stDevInfo.unDevStatus.stFxStatus.bGetFileIndex = TRUE;
        }
    }

    TaskLog(LOG_DETAIL, "get dev(%s) file index %u", stDevInfo.stNeBasicInfo.acDevName, ulIndex);

    SetDevInfo(stDevInfo);
}

VOID TaskSnmp::OnGetDevFileRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData)
{
    OMA_FILE_INFO_T stFile = {0};
    OID_T           stOID;
    UINT32          ulFileIndex = 0;
	UINT32			ulLastFileListIndex = 0;
    BOOL            bNoSuchInstance = FALSE;

    if (stDevInfo.stNeBasicInfo.ulDevType == OMC_DEV_TYPE_TX)
    {
        ulFileIndex = stDevInfo.unDevStatus.stTxStatus.ulFileIndex;
    }
    else if (stDevInfo.stNeBasicInfo.ulDevType == OMC_DEV_TYPE_FX)
    {
        ulFileIndex = stDevInfo.unDevStatus.stFxStatus.ulFileIndex;
    }
    else
    {
        return;
    }

    memcpy(stOID.astOID, oid_DevFileType, sizeof(oid_DevFileType));
    stOID.ulOIDNum = ARRAY_SIZE(oid_DevFileType);
    stOID.astOID[stOID.ulOIDNum++] = ulFileIndex;
    if (!GetSnmpValue(vData, stOID.astOID, stOID.ulOIDNum*sizeof(SNMP_OID), &stFile.ulFileType, &bNoSuchInstance) )
    {
        if (!bNoSuchInstance)
        {
            TaskLog(LOG_ERROR, "get dev(%s) file failed", stDevInfo.stNeBasicInfo.acDevName);
        }

        return;
    }

    memcpy(stOID.astOID, oid_DevFileTime, sizeof(oid_DevFileTime));
    stOID.ulOIDNum = ARRAY_SIZE(oid_DevFileTime);
    stOID.astOID[stOID.ulOIDNum++] = ulFileIndex;
    if (!GetSnmpValue(vData, stOID.astOID, stOID.ulOIDNum*sizeof(SNMP_OID), &stFile.ulFileTime) )
    {
        TaskLog(LOG_ERROR, "get dev(%s) file failed", stDevInfo.stNeBasicInfo.acDevName);
        return;
    }

    memcpy(stOID.astOID, oid_DevFileName, sizeof(oid_DevFileName));
    stOID.ulOIDNum = ARRAY_SIZE(oid_DevFileName);
    stOID.astOID[stOID.ulOIDNum++] = ulFileIndex;
    if (!GetSnmpValue(vData, stOID.astOID, stOID.ulOIDNum*sizeof(SNMP_OID), stFile.acFileName, sizeof(stFile.acFileName)) )
    {
        TaskLog(LOG_ERROR, "get dev(%s) file failed", stDevInfo.stNeBasicInfo.acDevName);
        return;
    }

    memcpy(stOID.astOID, oid_DevFileSize, sizeof(oid_DevFileSize));
    stOID.ulOIDNum = ARRAY_SIZE(oid_DevFileSize);
    stOID.astOID[stOID.ulOIDNum++] = ulFileIndex;
    if (!GetSnmpValue(vData, stOID.astOID, stOID.ulOIDNum*sizeof(SNMP_OID), stFile.acFileSize, sizeof(stFile.acFileSize)) )
    {
        TaskLog(LOG_ERROR, "get dev(%s) file failed", stDevInfo.stNeBasicInfo.acDevName);
        return;
    }

    memcpy(stOID.astOID, oid_LastDevFileIndex, sizeof(oid_LastDevFileIndex));
    stOID.ulOIDNum = ARRAY_SIZE(oid_LastDevFileIndex);
    stOID.astOID[stOID.ulOIDNum++] = ulFileIndex;
    if (!GetSnmpValue(vData, stOID.astOID, stOID.ulOIDNum*sizeof(SNMP_OID), &ulLastFileListIndex) )
    {
        TaskLog(LOG_ERROR, "get dev(%s) file failed", stDevInfo.stNeBasicInfo.acDevName);
        return;
    }

    if (stDevInfo.stNeBasicInfo.ulDevType == OMC_DEV_TYPE_TX)
    {
        stDevInfo.unDevStatus.stTxStatus.bGetFileIndex = FALSE;
		if (ulLastFileListIndex == 1)
		{
			// 若获取到矢量表的最后一条记录,发送set报文通知网元,并且重置获取矢量表的index
			SendSetGetLastFileListIndex(stDevInfo);
			stDevInfo.unDevStatus.stTxStatus.ulFileIndex = 0;
		}
    }
    else if (stDevInfo.stNeBasicInfo.ulDevType == OMC_DEV_TYPE_FX)
    {
        stDevInfo.unDevStatus.stFxStatus.bGetFileIndex = FALSE;
        if (ulLastFileListIndex == 1)
		{
			// 若获取到矢量表的最后一条记录,发送set报文通知网元,并且重置获取矢量表的index
			SendSetGetLastFileListIndex(stDevInfo);
			stDevInfo.unDevStatus.stFxStatus.ulFileIndex = 0;
		}
    }

    strcpy(stFile.acNeID, stDevInfo.stNeBasicInfo.acNeID);

    TaskLog(LOG_DETAIL, "get dev(%s) file: index=%u, time=%s, %s", stDevInfo.stNeBasicInfo.acDevName,
        ulFileIndex, gos_get_text_time(&stFile.ulFileTime), stFile.acFileName);

    if (!m_pDao->AddDevFile(stFile))
    {
        TaskLog(LOG_ERROR, "save dev(%s) file failed", stDevInfo.stNeBasicInfo.acDevName);
        return;
    }

    //  AddDevOperLog(stLog);

    SetDevInfo(stDevInfo);
}

VOID TaskSnmp::OnGetDevNetInfoRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData)
{
     
    OID_T           stOID;
    UINT8           aucGateway[4];

    memcpy(stOID.astOID, oid_ipGatewayAddress, sizeof(oid_ipGatewayAddress));
    stOID.ulOIDNum = ARRAY_SIZE(oid_ipGatewayAddress);
    if (!GetSnmpValue(vData, stOID.astOID, stOID.ulOIDNum*sizeof(SNMP_OID), aucGateway, sizeof(aucGateway)) )
    {
        TaskLog(LOG_ERROR, "get dev(%s) gateway failed", stDevInfo.stNeBasicInfo.acDevName);
        return;
    }

    if (memcmp(stDevInfo.stNeInfo.aucGateway, aucGateway, sizeof(aucGateway)) != 0)
    {
        memcpy(stDevInfo.stNeInfo.aucGateway, aucGateway, sizeof(aucGateway));

        TaskLog(LOG_DETAIL, "get dev(%s) gateway " IP_FMT, stDevInfo.stNeBasicInfo.acDevName, IP_ARG(aucGateway));

        SetDevInfo(stDevInfo);
    }
}

VOID TaskSnmp::OnGetDevSwUpdateRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData)
{
    BOOL    bSetFlag = FALSE;
    TaskLog(LOG_INFO, "get sw update rsp of %s", stDevInfo.stNeBasicInfo.acDevName);

    if (!m_pDao->UpdateSwSetFlag(stDevInfo.stNeBasicInfo.acNeID, bSetFlag))
    {
        TaskLog(LOG_ERROR, "save sw update set flag of %s failed", stDevInfo.stNeBasicInfo.acDevName);
        return;
    }

    if ( SetSwUpdateSetFlag(stDevInfo.stNeBasicInfo.acNeID, bSetFlag))
    {
        TaskLog(LOG_INFO, "sw update %s success.", stDevInfo.stNeBasicInfo.acDevName);
    }
}

// 处理收到的 snmp 报文
VOID TaskSnmp::HandleSnmpMsg(SNMP_GET_RSP_T &stSnmpGetRsp)
{
    VECTOR<SNMP_DATA_T> &vData = stSnmpGetRsp.vData;
    UINT32              ulDevType = 0;
    OMA_DEV_INFO_T      stDevInfo = {0};

    if (vData.size() == 0)
    {
        return;
    }

    if (!GetDevInfo(&stSnmpGetRsp.stClientAddr, stDevInfo)) // 获取设备的信息
    {
        TaskLog(LOG_ERROR, "unknown dev ip " IP_FMT, IP_ARG(stSnmpGetRsp.stClientAddr.aucIP));
        return;
    }

    //设置在线状态
    SetOnline(stDevInfo, TRUE);
    // 设置设备的类型
    ulDevType = stDevInfo.stNeBasicInfo.ulDevType;

    // 普通信息
    if (IS_OID(vData[0].astOID, oid_sysUptime))
    {
        OnGetSysInfoRsp(stDevInfo, vData); 
    }
    else if (IS_OID(vData[0].astOID, oid_ATSStatus))
    {
        if (ulDevType == OMC_DEV_TYPE_DIS)
        {
            OnGetDISStatusRsp(stDevInfo, vData);
        }
        else if (ulDevType == OMC_DEV_TYPE_DC)
        {
            OnGetDCStatusRsp(stDevInfo, vData);
        }
        else
        {
            TaskLog(LOG_ERROR, "oid_ATSStatus: invalid dev type %u", ulDevType);
        }
    }
    else if (IS_OID(vData[0].astOID, oid_GatewayReachable))
    {
        if (ulDevType == OMC_DEV_TYPE_TX)
        {
            OnGetTXStatusRsp(stDevInfo, vData);
        }
        else if (ulDevType == OMC_DEV_TYPE_FX)
        {
            OnGetFXStatusRsp(stDevInfo, vData);
        }
        else
        {
            TaskLog(LOG_ERROR, "oid_GatewayReachable: invalid dev type %u", ulDevType);
        }
    }
    else if (IS_OID(vData[0].astOID, oid_LteRSSI))
    {
        if ( ulDevType == OMC_DEV_TYPE_TX ||
             ulDevType == OMC_DEV_TYPE_FX )
        {
            OnGetLteStatusRsp(stDevInfo, vData);
        }
        else if (ulDevType == OMC_DEV_TYPE_TAU)
        {
            OnGetTAULteStatusRsp(stDevInfo, vData);
        }
        else
        {
            TaskLog(LOG_ERROR, "oid_LteRSSI: invalid dev type %u", ulDevType);
        }
    }
    else if (IS_TABLE_OID(vData[0].astOID, oid_OperLogIndex))
    {
        if (ulDevType == OMC_DEV_TYPE_TX ||
            ulDevType == OMC_DEV_TYPE_FX)
        {
            OnGetOperLogIndexRsp(stDevInfo, vData);
        }
        else
        {
            TaskLog(LOG_ERROR, "oid_OperLogIndex: invalid dev type %u", ulDevType);
        }
    }
    else if (IS_TABLE_OID(vData[0].astOID, oid_DevOperation))
    {
        if (ulDevType == OMC_DEV_TYPE_TX ||
            ulDevType == OMC_DEV_TYPE_FX)
        {
            OnGetOperLogRsp(stDevInfo, vData);
        }
        else
        {
            TaskLog(LOG_ERROR, "oid_DevOperation: invalid dev type %u", ulDevType);
        }
    }
    else if (IS_TABLE_OID(vData[0].astOID, oid_DevFileIndex))
    {
        if (ulDevType == OMC_DEV_TYPE_TX ||
            ulDevType == OMC_DEV_TYPE_FX)
        {
            OnGetDevFileIndexRsp(stDevInfo, vData);
        }
        else
        {
            TaskLog(LOG_ERROR, "oid_DevFileIndex: invalid dev type %u", ulDevType);
        }
    }
    else if (IS_TABLE_OID(vData[0].astOID, oid_DevFileType))
    {
        if (ulDevType == OMC_DEV_TYPE_TX ||
            ulDevType == OMC_DEV_TYPE_FX)
        {
            OnGetDevFileRsp(stDevInfo, vData);
        }
        else
        {
            TaskLog(LOG_ERROR, "oid_DevFileType: invalid dev type %u", ulDevType);
        }
    }
    else if (IS_OID(vData[0].astOID, oid_ipGatewayAddress))
    {
        OnGetDevNetInfoRsp(stDevInfo, vData);
    }
    else if (IS_OID(vData[0].astOID, oid_SoftwareUpdate))
    {
        if (ulDevType == OMC_DEV_TYPE_TX ||
            ulDevType == OMC_DEV_TYPE_FX)
        {
            OnGetDevSwUpdateRsp(stDevInfo, vData);
        }
        else
        {
            TaskLog(LOG_ERROR, "oid_SoftwareUpdate: invalid dev type %u", ulDevType);
        }
    }
    else if(IS_OID(vData[0].astOID, oid_WANTxBytes))
    {
        OnGetTrafficRsp(stDevInfo, vData);
    }

    SaveDevInfo();
}

VOID TaskSnmp::TaskEntry(UINT16 usMsgID, VOID *pvMsg, UINT32 ulMsgLen)
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

        default:
            break;
    }
}