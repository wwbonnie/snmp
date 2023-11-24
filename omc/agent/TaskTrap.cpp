include "omc_public.h"
#include "TaskTrap.h"
#include "snmp_nms.h"
#include "oid.h"

extern OMC_LOCAL_CFG_T g_stLocalCfg;


BOOL ParseAlarmTrap(SNMP_VAR_T  *pstVarList, ALARM_INFO_T &stInfo)
{
    SNMP_OID    astOID[SNMP_MAX_OID_SIZE];
    UINT32      ulOIDSize;
    CHAR        *szValue;
    UINT32      ulLen;
    BOOL        abTrapCheck[10];

    memset(&stInfo, 0, sizeof(stInfo));
    memset(abTrapCheck, 0, sizeof(abTrapCheck));

    while(pstVarList)
    {
        szValue = (CHAR*)pstVarList->stValue.pucValue;
        ulLen = pstVarList->stValue.ulLen;
        if (snmp_parse_oid_var(pstVarList->stOID.pucValue, pstVarList->stOID.ulLen, astOID, &ulOIDSize) < 0)
        {
            return FALSE;
        }

        if (IS_OID(astOID, oid_AlarmSn))
        {
            stInfo.ulAlarmID = snmp_get_int_value((UINT8*)szValue, ulLen);
            abTrapCheck[0] = TRUE;
        }
        else if (IS_OID(astOID, oid_AlarmNeName))
        {
            ToNeID((UINT8*)szValue, stInfo.acNeID);
            abTrapCheck[1] = TRUE;
        }
        else if (IS_OID(astOID, oid_AlarmLevel))
        {
            stInfo.ulAlarmLevel = snmp_get_int_value((UINT8*)szValue, ulLen);
            abTrapCheck[2] = TRUE;
        }
        else if (IS_OID(astOID, oid_AlarmType))
        {
            stInfo.ulAlarmType = snmp_get_int_value((UINT8*)szValue, ulLen);
            abTrapCheck[3] = TRUE;
        }
        else if (IS_OID(astOID, oid_AlarmReasonID))
        {
            stInfo.ulAlarmReasonID = snmp_get_int_value((UINT8*)szValue, ulLen);
            abTrapCheck[4] = TRUE;
        }
        else if (IS_OID(astOID, oid_AlarmReason))
        {
            CopyText(stInfo.acAlarmReason, sizeof(stInfo.acAlarmReason), szValue, ulLen);
            abTrapCheck[5] = TRUE;
        }
        else if (IS_OID(astOID, oid_AlarmRaiseTime))
        {
            stInfo.ulAlarmRaiseTime = snmp_get_int_value((UINT8*)szValue, ulLen);
            abTrapCheck[6] = TRUE;
        }
        else if (IS_OID(astOID, oid_AlarmStatus))
        {
            stInfo.ulAlarmStatus = snmp_get_int_value((UINT8*)szValue, ulLen);
            abTrapCheck[7] = TRUE;
        }
        else if (IS_OID(astOID, oid_AlarmTitle))
        {
            CopyText(stInfo.acAlarmTitle, sizeof(stInfo.acAlarmTitle), szValue, ulLen);
            abTrapCheck[8] = TRUE;
        }
        else if (IS_OID(astOID, oid_AlarmInfo))
        {
            CopyText(stInfo.acAlarmInfo, sizeof(stInfo.acAlarmInfo), szValue, ulLen);
            abTrapCheck[9] = TRUE;
        }

        pstVarList = pstVarList->pstNext;
    }

    for (UINT32 i=0; i<ARRAY_SIZE(abTrapCheck); i++)
    {
        if (!abTrapCheck[i])
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL ParseOnlineTrap(SNMP_VAR_T *pstVarList, NE_BASIC_INFO_T &stInfo)
{
    SNMP_OID    astOID[SNMP_MAX_OID_SIZE];
    UINT32      ulOIDSize;
    CHAR        *szValue;
    UINT32      ulLen;
    BOOL        abTrapCheck[6];

    memset(&stInfo, 0, sizeof(stInfo));
    memset(abTrapCheck, 0, sizeof(abTrapCheck));

    while(pstVarList)
    {
        szValue = (CHAR*)pstVarList->stValue.pucValue;
        ulLen = pstVarList->stValue.ulLen;
        if (snmp_parse_oid_var(pstVarList->stOID.pucValue, pstVarList->stOID.ulLen, astOID, &ulOIDSize) < 0)
        {
            return FALSE;
        }

        if (IS_OID(astOID, oid_sysMacAddress))
        {
            if (ulLen != 6)
            {
                return FALSE;
            }

            sprintf(stInfo.acDevMac, GOS_MAC_FMT, GOS_MAC_ARG(szValue));
            ToNeID((UINT8*)szValue, stInfo.acNeID);
            abTrapCheck[0] = TRUE;
        }
        else if (IS_OID(astOID, oid_sysIpaddress))
        {
            if (ulLen != 4)
            {
                return FALSE;
            }

            sprintf(stInfo.acDevIp, IP_FMT, IP_ARG(szValue));
            abTrapCheck[1] = TRUE;
        }
        else if (IS_OID(astOID, oid_softwareVersion))
        {
            CopyText(stInfo.acSoftwareVer, sizeof(stInfo.acSoftwareVer), szValue, ulLen);
            abTrapCheck[2] = TRUE;
        }
        else if (IS_OID(astOID, oid_hardwareVersion))
        {
            CopyText(stInfo.acHardwareVer, sizeof(stInfo.acHardwareVer), szValue, ulLen);
            abTrapCheck[3] = TRUE;
        }
        else if (IS_OID(astOID, oid_modelName))
        {
            CopyText(stInfo.acModelName, sizeof(stInfo.acModelName), szValue, ulLen);
            abTrapCheck[4] = TRUE;
        }
        else if (IS_OID(astOID, oid_devType))
        {
            stInfo.ulDevType = snmp_get_int_value((UINT8*)szValue, ulLen);
            abTrapCheck[5] = TRUE;
        }

        pstVarList = pstVarList->pstNext;
    }

    for (UINT32 i=0; i<ARRAY_SIZE(abTrapCheck); i++)
    {
        if (!abTrapCheck[i])
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL ParseHeartBeatTrap(SNMP_VAR_T *pstVarList, CHAR *szNeID)
{
    SNMP_OID    astOID[SNMP_MAX_OID_SIZE];
    UINT32      ulOIDSize;
    CHAR        *szValue;
    UINT32      ulLen;

    while(pstVarList)
    {
        szValue = (CHAR*)pstVarList->stValue.pucValue;
        ulLen = pstVarList->stValue.ulLen;
        if (snmp_parse_oid_var(pstVarList->stOID.pucValue, pstVarList->stOID.ulLen, astOID, &ulOIDSize) < 0)
        {
            return FALSE;
        }

        if (IS_OID(astOID, oid_sysMacAddress))
        {
            if (ulLen != 6)
            {
                return FALSE;
            }

            ToNeID((UINT8*)szValue, szNeID);
            return TRUE;
        }

        pstVarList = pstVarList->pstNext;
    }

    return FALSE;
}

VOID TaskTrap::OnAlarmTrap(ALARM_INFO_T &stInfo)
{
    BOOL    bIsNewAlarm = FALSE;

    AddActiveAlarm(stInfo, bIsNewAlarm);

    if (bIsNewAlarm)
    {
        m_pDao->AddActiveAlarm(&stInfo);
    }
}

VOID TaskTrap::OnOnlineTrap(IP_ADDR_T *pstClientAddr, NE_BASIC_INFO_T &stInfo)
{
    OMA_DEV_INFO_T      stDevInfo;
    BOOL                bSave = TRUE;

    memcpy(&stInfo.stClientAddr, pstClientAddr, sizeof(stInfo.stClientAddr));

    if (!GetDevInfo(stInfo.acNeID, stDevInfo))
    {
        stInfo.ulOnlineTime = gos_get_current_time();
        stInfo.ulFirstOnlineTime = gos_get_uptime();
        stInfo.ulLastOnlineTime = stInfo.ulFirstOnlineTime;

        SetNEBasicInfo(stInfo);
        Log(LOG_INFO, "dev %s(%s) is online", stInfo.acDevName, stInfo.acDevMac);
    }
    else
    {
        stInfo.ulOnlineTime = stOldInfo.ulOnlineTime;
        stInfo.ulFirstOnlineTime = stOldInfo.ulFirstOnlineTime;
        stInfo.ulLastOnlineTime = stOldInfo.ulLastOnlineTime;

        if (memcmp(&stOldInfo, &stInfo, sizeof(stInfo)) == 0)
        {
            bSave = FALSE;
        }

        stInfo.ulLastOnlineTime = gos_get_uptime();
        if (stInfo.ulFirstOnlineTime == 0)
        {
            stInfo.ulFirstOnlineTime = stInfo.ulLastOnlineTime;
        }

        SetNEBasicInfo(stInfo);
    }

    if (bSave)
    {
        if (!m_pDao->AddNeBasicInfo(&stInfo))
        {
            Log(LOG_ERROR, "set dev(%s) basic info failed", stInfo.acDevName);
        }
    }
}

VOID TaskTrap::OnHeartBeatTrap(CHAR *szNeID)
{
    NE_BASIC_INFO_T     stInfo;

    if (!GetNEBasicInfo(szNeID, stInfo))
    {
        return;
    }

    stInfo.ulLastOnlineTime = gos_get_uptime();
    if (stInfo.ulFirstOnlineTime == 0)
    {
        stInfo.ulFirstOnlineTime = stInfo.ulLastOnlineTime;
    }

    SetNEBasicInfo(stInfo);
}

VOID TaskTrap::HandleTrapMsg(IP_ADDR_T *pstClientAddr, VOID *pPDU)
{
    SNMP_TRAP2_PDU_T    *pstPDU = (SNMP_TRAP2_PDU_T*)pPDU;
    SNMP_VAR_T          *pstVarList;
    SNMP_OID            astOID[SNMP_MAX_OID_SIZE];
    UINT32              ulOIDSize;
    CHAR                *szValue;
    UINT32              ulLen;
    UINT32              ulTimeStamp = 0;
    SNMP_OID            astTimeStampOID[] = {SNMP_TIMESTAMP_OID};
    SNMP_OID            astTrapOID[] = {SNMP_TRAP_OID};
    ALARM_INFO_T        stAlarmInfo;
    NE_BASIC_INFO_T     stBasicInfo;

    pstVarList = pstPDU->pstVarList;
    if (!pstVarList)
    {
        return;
    }

    // time stamp oid
    if (snmp_parse_oid_var(pstVarList->stOID.pucValue, pstVarList->stOID.ulLen, astOID, &ulOIDSize) < 0)
    {
        return;
    }

    if (!IS_OID(astOID, astTimeStampOID))
    {
        Log(LOG_ERROR, "parse alarm trap failed, expect time stamp OID");
        return;
    }

    ulTimeStamp = snmp_get_int_value(pstVarList->stValue.pucValue, pstVarList->stValue.ulLen);

    // trap oid
    pstVarList = pstVarList->pstNext;
    if (!pstVarList)
    {
        return;
    }

    if (snmp_parse_oid_var(pstVarList->stOID.pucValue, pstVarList->stOID.ulLen, astOID, &ulOIDSize) < 0)
    {
        return;
    }

    if (!IS_OID(astOID, astTrapOID))
    {
        Log(LOG_ERROR, "parse alarm trap failed, expect trap OID");
        return;
    }

    // 获取Trap类型，心跳、告警等
    szValue = (CHAR*)pstVarList->stValue.pucValue;
    ulLen = pstVarList->stValue.ulLen;
    if (snmp_parse_oid_var((UINT8*)szValue, ulLen, astOID, &ulOIDSize) < 0)
    {
        Log(LOG_ERROR, "parse alarm trap failed");
        return;
    }

    // other
    pstVarList = pstVarList->pstNext;
    if (!pstVarList)
    {
        Log(LOG_ERROR, "parse alarm trap failed");
        return;
    }

    if (IS_OID(astOID, oid_DiscoveryTrap))
    {
        if (!ParseOnlineTrap(pstVarList, stBasicInfo))
        {
            Log(LOG_ERROR, "parse dev online trap failed");
            return;
        }

        OnOnlineTrap(pstClientAddr, stBasicInfo);
    }
    else if (IS_OID(astOID, oid_HeartBeatTrap))
    {
        CHAR    acNeID[32];

        if (!ParseHeartBeatTrap(pstVarList, acNeID))
        {
            Log(LOG_ERROR, "parse heart beat trap failed");
            return;
        }

        OnHeartBeatTrap(acNeID);
    }
    else if (IS_OID(astOID, oid_CpuUsageTooHighTrap) ||
            IS_OID(astOID, oid_MemUsageTooHighTrap) ||
            IS_OID(astOID, oid_RebootTrap) ||
            IS_OID(astOID, oid_DiskUsageTooHighTrap) ||
            IS_OID(astOID, oid_SwitchToMasterTrap) ||
            IS_OID(astOID, oid_SwitchToSlaveTrap)  ||
            IS_OID(astOID, oid_GetATSDataFailTrap) ||
            IS_OID(astOID, oid_SyncTimeFailTrap)   ||
            IS_OID(astOID, oid_LinkToLteFailTrap)  )
    {
        if (!ParseAlarmTrap(pstVarList, stAlarmInfo))
        {
            Log(LOG_ERROR, "parse alarm trap failed");
            return;
        }

        OnAlarmTrap(stAlarmInfo);
    }

  //  SendLocal(MODULE_APP, EV_TRAP_REPORT_IND, &stTrapInfo, sizeof(stTrapInfo));


}
