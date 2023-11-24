#include "omc_public.h"
#include "TaskSnmp.h"
#include "snmp_nms.h"
#include "oid.h"
#include "eomc_oid.h"

extern OMC_LOCAL_CFG_T g_stLocalCfg;
extern VOID SendSetManageStateMsg(CHAR *szNeID, BOOL bOnline);
extern VECTOR<ALARM_LEVEL_CFG_T>      g_vAlarmCfg;
#define ALARM_STORE_TIMEOUT 5    ///< �澯�洢��ʱʱ�䣬��λΪ��

BOOL ParseAlarmTrap(SNMP_VAR_T  *pstVarList, ALARM_INFO_T &stInfo)
{
    SNMP_OID    astOID[SNMP_MAX_OID_SIZE];
    UINT32      ulOIDSize;
    CHAR        *szValue;
    UINT32      ulLen;
    BOOL        abTrapCheck[10];
    //OMA_DEV_INFO_T stDevInfo = {0};

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
            if (ulLen == 6) // ������MAC��ַ
            {
                ToNeID((UINT8*)szValue, stInfo.acNeID);
            }
            else if (ulLen == 17)   // �ַ���MAC��ַ
            {
                UINT8       aucMAC[6];

                if (!gos_get_mac(szValue, aucMAC))
                {
                    return FALSE;
                }

                ToNeID(aucMAC, stInfo.acNeID);
            }
            else
            {
                return FALSE;
            }

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

            GOS_DATETIME_T  stTime;

            gos_get_localtime(&stInfo.ulAlarmRaiseTime, &stTime);
            if (stTime.usYear < 2000)
            {
                stInfo.ulAlarmRaiseTime = gos_get_current_time();
            }

            abTrapCheck[6] = TRUE;
        }
        else if (IS_OID(astOID, oid_AlarmStatus))
        {
            stInfo.ulAlarmStatus = snmp_get_int_value((UINT8*)szValue, ulLen);
            if (stInfo.ulAlarmStatus != ALARM_STATUS_RAISE)
            {
                stInfo.ulAlarmStatus = ALARM_STATUS_CLEAR;
            }

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
    BOOL        abTrapCheck[7];

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
            if (stInfo.acHardwareVer[0] == ' ')
            {
                memmove(stInfo.acHardwareVer, stInfo.acHardwareVer+1, strlen(stInfo.acHardwareVer+1)+1);
            }
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
        else if (IS_OID(astOID, oid_sysHotId))
        {
            CopyText(stInfo.acDevName, sizeof(stInfo.acDevName), szValue, ulLen);
            abTrapCheck[6] = TRUE;   
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

INT32 GetEOmcAlarmLevel(UINT32 ulAlarmLevel)
{
    if (ulAlarmLevel == EOMC_ALARM_LEVEL_FATAL)     return ALARM_LEVEL_CRITICAL;
    if (ulAlarmLevel == EOMC_ALARM_LEVEL_MAIN)      return ALARM_LEVEL_MAIN;
    if (ulAlarmLevel == EOMC_ALARM_LEVEL_MINOR)     return ALARM_LEVEL_MINOR;
    if (ulAlarmLevel == EOMC_ALARM_LEVEL_NORMAL)    return ALARM_LEVEL_NORMAL;
    if (ulAlarmLevel == EOMC_ALARM_LEVEL_CLEAR)     return ALARM_LEVEL_CLEAR;
    if (ulAlarmLevel == EOMC_ALARM_LEVEL_EVENT)     return ALARM_LEVEL_EVENT;

    return ALARM_LEVEL_UNKNOWN;
}

UINT32 GetEOmcAlarmTime(CHAR *szValue, UINT32 ulLen)
{
    if (ulLen != 19)
    {
        return 0;
    }

    CHAR            acTime[32];
    GOS_DATETIME_T  stTime;

    memcpy(acTime, szValue, ulLen);
    acTime[ulLen] = '\0';

    if (!gos_parse_time(acTime, &stTime))
    {
        return 0;
    }

    INT32   iTimeZone = gos_get_timezone();
    UINT32  ulTime = gos_mktime(&stTime);

    ulTime += iTimeZone;

    return ulTime;
}

BOOL ParseEOmcAlarmTrap(SNMP_VAR_T  *pstVarList, ALARM_INFO_T &stInfo)
{
    SNMP_OID    astOID[SNMP_MAX_OID_SIZE];
    UINT32      ulOIDSize;
    CHAR        *szValue;
    UINT32      ulLen;
    BOOL        abTrapCheck[6];

    memset(&stInfo, 0, sizeof(stInfo));
    memset(abTrapCheck, 0, sizeof(abTrapCheck));

    strcpy(stInfo.acNeID, OMC_NE_ID_EOMC);
    stInfo.ulAlarmType = ALARM_TYPE_DEV;
    stInfo.ulAlarmReasonID = 0;

    while(pstVarList)
    {
        szValue = (CHAR*)pstVarList->stValue.pucValue;
        ulLen = pstVarList->stValue.ulLen;
        if (snmp_parse_oid_var(pstVarList->stOID.pucValue, pstVarList->stOID.ulLen, astOID, &ulOIDSize) < 0)
        {
            return FALSE;
        }

        if (IS_OID(astOID, oid_eOMC_AlarmID))
        {
            stInfo.ulAlarmID = snmp_get_int_value((UINT8*)szValue, ulLen);
            abTrapCheck[0] = TRUE;
        }
        else if (IS_OID(astOID, oid_eOMC_AlarmLevel))
        {
            stInfo.ulAlarmLevel = snmp_get_int_value((UINT8*)szValue, ulLen);
            stInfo.ulAlarmLevel = GetEOmcAlarmLevel(stInfo.ulAlarmLevel);
            if (stInfo.ulAlarmLevel != ALARM_LEVEL_UNKNOWN)
            {
                abTrapCheck[1] = TRUE;

                if (stInfo.ulAlarmLevel == ALARM_LEVEL_CLEAR)
                {
                    stInfo.ulAlarmStatus = ALARM_STATUS_CLEAR;
                }
                else
                {
                    stInfo.ulAlarmStatus = ALARM_STATUS_RAISE;
                }
            }
        }
        else if (IS_OID(astOID, oid_eOMC_AlarmSpecificproblems))
        {
            CopyUtf8Text(stInfo.acAlarmReason, sizeof(stInfo.acAlarmReason), szValue, ulLen);
            abTrapCheck[2] = TRUE;
        }
        else if (IS_OID(astOID, oid_eOMC_AlarmOccurTime))
        {
            stInfo.ulAlarmRaiseTime = GetEOmcAlarmTime(szValue, ulLen);
            if (stInfo.ulAlarmRaiseTime > 0)
            {
                abTrapCheck[3] = TRUE;
            }
        }
        else if (IS_OID(astOID, oid_eOMC_AlarmProbablecause))
        {
            CopyUtf8Text(stInfo.acAlarmTitle, sizeof(stInfo.acAlarmTitle), szValue, ulLen);
            abTrapCheck[4] = TRUE;
        }
        else if (IS_OID(astOID, oid_eOMC_AlarmExtendInfo))
        {
            CopyUtf8Text(stInfo.acAlarmInfo, sizeof(stInfo.acAlarmInfo), szValue, ulLen);
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

VOID TaskSnmp::CheckAlarmCfg(ALARM_INFO_T &stInfo, UINT32 ulDevType, BOOL &bIsIgnore)
{
	/* 1. ���и����豸(DevType)�Ҹ���澯(AlarmID)�Ķ����澯���ã����Զ����澯����Ϊ׼��
	   2. ���и���澯(AlarmID)��һ���澯���ã�����һ���澯����Ϊ׼��
	   3. ���޸����豸��һ�����澯���ã������豸�ϱ��澯�ȼ�Ϊ׼��
	˵������ͬʱ���ڸø澯��һ�����澯���ã�������澯����������һ���澯���á�
	*/
    for (UINT32 i=0; i<g_vAlarmCfg.size(); i++)
    {
		if ( g_vAlarmCfg[i].ulDevType == ulDevType &&
			 g_vAlarmCfg[i].ulAlarmID == stInfo.ulAlarmID )
        {
            if ( stInfo.ulAlarmLevel != g_vAlarmCfg[i].ulAlarmLevel)
            {
                stInfo.ulAlarmLevel = g_vAlarmCfg[i].ulAlarmLevel;
				bIsIgnore = g_vAlarmCfg[i].bIsIgnore;
				return;
            }
        }
    }
	
	for (UINT32 i=0; i<g_vAlarmCfg.size(); i++)
    {
		if ( g_vAlarmCfg[i].ulDevType == OMC_DEV_TYPE_ALL &&
			 stInfo.ulAlarmID == g_vAlarmCfg[i].ulAlarmID )
        {
            if ( stInfo.ulAlarmLevel != g_vAlarmCfg[i].ulAlarmLevel)
            {
                stInfo.ulAlarmLevel = g_vAlarmCfg[i].ulAlarmLevel;
				bIsIgnore = g_vAlarmCfg[i].bIsIgnore;
				return;
            }
        }
    }

}

// д�ļ���ʽ����澯����
VOID TaskSnmp::StoreAlarmRaiseData()
{
    UINT32  ulCurrTime = gos_get_uptime();
    // m_fpRaise �澯������ʱ�ļ����ļ���������һֱ����
    fseek(m_fpRaise, 0, SEEK_END);
    INT32 iSize = ftell(m_fpRaise) / sizeof(ALARM_INFO_T);
    fseek(m_fpRaise, 0, SEEK_SET);

    /// �澯��������2000����ʱ ACTIVE_ALARM_STORE_TIMEOUT ʱ��󣬰���ʱ�ļ�д���ȡ�ļ� 
    if ((iSize >= 2000) || 
        ((ulCurrTime - m_ulLastAlarmRaiseUpdateTime) > ALARM_STORE_TIMEOUT) && iSize > 0)
    {
        if(!gos_file_exist(m_szRiseFile))
        {
            fclose(m_fpRaise);
            gos_rename_file(m_szRiseTmpFile, m_szRiseFile);
            m_fpRaise = fopen(m_szRiseTmpFile, "a+b");    
        }

        m_ulLastAlarmRaiseUpdateTime = ulCurrTime;
    }
}
// д�ļ���ʽ����澯����
VOID TaskSnmp::StoreAlarmClearData()
{
     UINT32          ulCurrTime = gos_get_uptime();

    // m_fpClear �澯������ʱ�ļ����ļ���������һֱ����
    fseek(m_fpClear, 0, SEEK_END);
    INT32 iSize = ftell(m_fpClear) / sizeof(ALARM_INFO_T);
    // �澯��������2000����ʱ ACTIVE_ALARM_STORE_TIMEOUT ʱ��󣬰���ʱ�ļ�д���ȡ�ļ� 
    fseek(m_fpClear, 0, SEEK_SET);  
    if ( (iSize > 2000) ||
         ((ulCurrTime - m_ulLastAlarmClearUpdateTime) > ALARM_STORE_TIMEOUT) && iSize > 0)
    {
        if (!gos_file_exist(m_szClearFile))
        {
            fclose(m_fpClear);
            gos_rename_file(m_szClearTmpFile, m_szClearFile);
            m_fpClear = fopen(m_szClearTmpFile, "a+b");
        }

        m_ulLastAlarmClearUpdateTime = gos_get_uptime();
    }
}

VOID TaskSnmp::OnAlarmTrap(ALARM_INFO_T &stInfo, UINT32 ulDevType)
{
	BOOL    bIsIgnore = FALSE;
	// ���澯����
	CheckAlarmCfg(stInfo, ulDevType, bIsIgnore);
    
	// ��������澯
    if (stInfo.ulAlarmStatus == ALARM_STATUS_RAISE)
    {
		BOOL    bIsNewAlarm = FALSE;

		// �����Ըø澯����������
		if (bIsIgnore)
		{
			return;
		}
        
        //�жϸø澯�Ƿ����µĸ澯������ne_id��Alarm_IDһ�������µĸ澯
        AddActiveAlarm(stInfo, bIsNewAlarm); 
        if (bIsNewAlarm)
        {
            // �жϸ澯������ʱ�ļ��ĸ澯������д�볬ʱʱ��
            StoreAlarmRaiseData();  
            // �澯����д����ʱ�ļ�
            fwrite(&stInfo, sizeof(ALARM_INFO_T), 1, m_fpRaise);
            fflush(m_fpRaise);
        }
    }
    else if (stInfo.ulAlarmStatus == ALARM_STATUS_CLEAR)    // ���������澯
    {
        // �澯�������ȴӻ�Ծ�澯�Ƴ�������ӵ���ʷ�澯
        DelActiveAlarm(stInfo.acNeID, stInfo.ulAlarmID);
        // �жϸ澯������ʱ�ļ��ĸ澯������д�볬ʱʱ��
        StoreAlarmClearData();
        // �澯����д����ʱ�ļ�
        fwrite(&stInfo, sizeof(ALARM_INFO_T), 1, m_fpClear);
        fflush(m_fpClear);
    }
    else
    {
        TaskLog(LOG_ERROR, "unknown status(%u) of active alarm(%s, %u)", stInfo.ulAlarmStatus, stInfo.acNeID, stInfo.ulAlarmID);
    } 

}

VOID TaskSnmp::OnOnlineTrap(IP_ADDR_T *pstClientAddr, NE_BASIC_INFO_T &stInfo)
{
    OMA_DEV_INFO_T      stDevInfo = {0};
    BOOL                bSave = TRUE;
    UINT32              ulCurrUptime = gos_get_uptime();

    memcpy(&stInfo.stClientAddr, pstClientAddr, sizeof(stInfo.stClientAddr));

    /* MAC��ͻ���: 
    1.��ǰ�����豸��mac�Ѵ��ڣ���IP��ͬ���ж�MAC��ַ��ͻ(���ǵ��豸oma��ʱ��90s������)                 
    */
    if (GetDevInfo(stInfo.acNeID, stDevInfo))
    {
        // MEMORY_CHECK();
        // �ж�MAC��ͬ����£�IP�Ƿ���ͬ����IP����ͬ���ж�MAC��ͻ��
        // �����豸�����ݿ�ʱδ���豸IP���豸��һ������֮ǰ��������û���豸��IP��Ϣ��
        if (stDevInfo.stNeBasicInfo.acDevIp[0] != '\0' &&
            strcmp(stDevInfo.stNeBasicInfo.acDevIp, stInfo.acDevIp) != 0)
        {
            TaskLog(LOG_FATAL, "MAC Address Conflict dev %s is online", stInfo.acDevMac);
            // ��������˳�ͻ��⣬����֮�����ߵ��豸����
            if (g_stLocalCfg.bConflictDetection)
            {
                return;
            }
        }

        // �豸�����Ǳ��������ݿ⣬���߱�����δ�豸�����ֶ�Ϊ��
        //memcpy(stInfo.acDevName, stDevInfo.stNeBasicInfo.acDevName, sizeof(stInfo.acDevName));
        // �����豸������Ϣ����û�и��µ���Ϣ����Ҫ�������ݿ��豸������Ϣ
        if (memcmp(&stDevInfo.stNeBasicInfo, &stInfo, sizeof(stInfo)) == 0)
        {
            bSave = FALSE;
        }
    }
    else if (GetDevInfo(pstClientAddr, stDevInfo))
    {
        // ���ݿⲻ���ڸ��豸MAC����IP���ڡ�
        // IP��ַ��ͻ��⣺1.��IP��ַ�Ѵ��ڣ���MAC��ַ�����ݿⲻ��ͬ�����ж�IP��ַ��ͻ.
        if(strcmp(stDevInfo.stNeBasicInfo.acNeID,stInfo.acNeID) != 0)
        {
            TaskLog(LOG_FATAL, "IP Address Conflict dev %s is exit", stDevInfo.stNeBasicInfo.acDevIp);
            // ��������˳�ͻ��⣬����֮�����ߵ��豸����
            if (g_stLocalCfg.bConflictDetection)
            {
                return;
            }
        }

        // �豸�����Ǳ��������ݿ⣬���߱�����δ�豸�����ֶ�Ϊ��
        //memcpy(stInfo.acDevName, stDevInfo.stNeBasicInfo.acDevName, sizeof(stInfo.acDevName));
        // �����豸������Ϣ����û�и��µ���Ϣ����Ҫ�������ݿ��豸������Ϣ
        if (memcmp(&stDevInfo.stNeBasicInfo, &stInfo, sizeof(stInfo)) == 0)
        {
            bSave = FALSE;
        }
    }
    else
    {
        TaskLog(LOG_DETAIL, "New devices online: %s(%s)", stInfo.acDevMac, stInfo.acDevIp);
    }

    stInfo.ulFirstOnlineTime = ulCurrUptime;
    stInfo.ulLastOnlineTime = ulCurrUptime;
    memcpy(&stDevInfo.stNeBasicInfo, &stInfo, sizeof(stInfo));
    // �����豸����״̬
    SetOnline(stDevInfo, TRUE);
    // �����豸��Ϣ
    SetDevInfo(stDevInfo);

    if (bSave)
    {
        if (!m_pDao->AddNeBasicInfo(stDevInfo.stNeBasicInfo))
        {
            TaskLog(LOG_ERROR, "set dev(%s) basic info failed", stInfo.acDevName);
        }
    }
    // ֪ͨ�豸����
    SendSetManageStateMsg(stDevInfo.stNeBasicInfo.acNeID, TRUE);
    TaskLog(LOG_DETAIL, "Devices online: %s(%s)", stDevInfo.stNeBasicInfo.acDevName, stDevInfo.stNeBasicInfo.acDevMac);
}

VOID TaskSnmp::OnHeartBeatTrap(IP_ADDR_T *pstClientAddr, CHAR *szNeID)
{
    //MEMORY_CHECK();
    OMA_DEV_INFO_T      stInfo;
    ALARM_INFO_T        stAlarmInfo;

    if (!GetDevInfo(szNeID, stInfo))
    {
        return;
    }

    // ����Ѳ������߸澯���������Ծ�澯�е����߸澯
    if (GetActiveAlarmInfo(szNeID,SNMP_ALARM_CODE_DevOffLine,stAlarmInfo))
    {
        DelActiveAlarm(stAlarmInfo.acNeID , stAlarmInfo.ulAlarmID);
        if (!m_pDao->ClearActiveAlarm(stAlarmInfo.acNeID , stAlarmInfo.ulAlarmID, stAlarmInfo.ulAlarmRaiseTime,stAlarmInfo.ulAlarmLevel,(CHAR *)ALARM_CLEAR_AUTO))
        {
            TaskLog(LOG_ERROR, "clear active alarm(%s, %u) from db failed", stInfo.stNeBasicInfo .acNeID , SNMP_ALARM_CODE_DevOffLine);
        }	
    }

    // �����豸�������ʱ��
    stInfo.stNeBasicInfo.ulLastOnlineTime = gos_get_uptime();
    if (stInfo.stNeBasicInfo.ulFirstOnlineTime == 0)
    {
        stInfo.stNeBasicInfo.ulFirstOnlineTime = stInfo.stNeBasicInfo.ulLastOnlineTime;
    }
 
    //memcpy(&stInfo.stNeBasicInfo.stClientAddr, pstClientAddr, sizeof(stInfo.stNeBasicInfo.stClientAddr));

    if (!stInfo.bIsOnLine)
    {
        SetOnline(stInfo, TRUE);
    }

    SetDevInfo(stInfo);
}

VOID TaskSnmp::HandleTrapMsg(IP_ADDR_T *pstClientAddr, VOID *pPDU)
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
    ALARM_INFO_T        stAlarmInfo = {0};
    NE_BASIC_INFO_T     stBasicInfo = {0};
    OMA_DEV_INFO_T      stDevInfo = {0};

    pstVarList = pstPDU->pstVarList;
    if (!pstVarList)
    {
        return;
    }
    
    // parse time stamp oid
    if (snmp_parse_oid_var(pstVarList->stOID.pucValue, pstVarList->stOID.ulLen, astOID, &ulOIDSize) < 0)
    {
        return;
    }

    if (!IS_OID(astOID, astTimeStampOID))
    {
        TaskLog(LOG_ERROR, "parse alarm trap failed, expect time stamp OID");
        return;
    }
    
    ulTimeStamp = snmp_get_int_value(pstVarList->stValue.pucValue, pstVarList->stValue.ulLen);


    // parse trap oid
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
        TaskLog(LOG_ERROR, "parse dev " IP_FMT, IP_ARG(pstClientAddr->aucIP));
        TaskLog(LOG_ERROR, "parse trap failed, expect trap OID");
        return;
    }

    // ��ȡTrap���ͣ����֡������͸澯��
    szValue = (CHAR*)pstVarList->stValue.pucValue;
    ulLen = pstVarList->stValue.ulLen;
    if (snmp_parse_oid_var((UINT8*)szValue, ulLen, astOID, &ulOIDSize) < 0)
    {
        TaskLog(LOG_ERROR, "parse alarm trap failed");
        return;
    }

    // other
    pstVarList = pstVarList->pstNext;

    if (IS_OID(astOID, oid_DiscoveryTrap))
    {
        
        if (!ParseOnlineTrap(pstVarList, stBasicInfo))
        {
            TaskLog(LOG_ERROR, "parse dev(%s) online trap failed", stBasicInfo.acDevMac);
            return;
        }

        OnOnlineTrap(pstClientAddr, stBasicInfo);
    }
    else if (IS_OID(astOID, oid_HeartBeatTrap))
    {
        
        CHAR    acNeID[32];

        if (!ParseHeartBeatTrap(pstVarList, acNeID))
        {
            TaskLog(LOG_ERROR, "parse heart beat trap failed");
            return;
        }

        OnHeartBeatTrap(pstClientAddr, acNeID);
    }
    else if (IS_OID(astOID, oid_CpuUsageTooHighTrap)   ||
             IS_OID(astOID, oid_MemUsageTooHighTrap)   ||
             IS_OID(astOID, oid_DiskUsageTooHighTrap)  ||
             IS_OID(astOID, oid_LanStatusTrap)         ||
             IS_OID(astOID, oid_LteWeakSignalTrap)     ||
             IS_OID(astOID, oid_PowerDownTrap)         ||
             IS_OID(astOID, oid_RebootTrap)            ||
             IS_OID(astOID, oid_DevTempTrap)           ||
             IS_OID(astOID, oid_EmergencyCallTrap)     ||
             IS_OID(astOID, oid_EmergencyBraceTrap)    ||
             IS_OID(astOID, oid_SelfCheckFailTrap)     ||
             IS_OID(astOID, oid_SwitchToMasterTrap)    ||
             IS_OID(astOID, oid_SwitchToSlaveTrap)     ||
             IS_OID(astOID, oid_GetATSDataFailTrap)    ||
             IS_OID(astOID, oid_SyncTimeFailTrap)      ||
             IS_OID(astOID, oid_LinkToLteFailTrap)     ||
             IS_OID(astOID, oid_IPHLinkFailTrap)       ||
             IS_OID(astOID, oid_PALinkFailTrap)        ||
			 IS_OID(astOID, oid_HardDiskMountFailTrap) ||
			 IS_OID(astOID, oid_FileStorageTooHighTrap))
    {
        if (!ParseAlarmTrap(pstVarList, stAlarmInfo))
        {
            TaskLog(LOG_ERROR, "parse alarm trap failed");
            return;
        }
        // TAU4.0�ϱ��ĸ澯trap����AlarmID��ֻ�ܸ����ϱ��澯trap��OID������,����AlarmLevel
        GetDevInfo(stAlarmInfo.acNeID, stDevInfo);
        if (stDevInfo.stNeBasicInfo.ulDevType == OMC_DEV_TYPE_TAU)
        {
            if (IS_OID(astOID, oid_CpuUsageTooHighTrap)) 
            {
				stAlarmInfo.ulAlarmLevel = ALARM_LEVEL_MINOR;
                stAlarmInfo.ulAlarmID = SNMP_ALARM_CODE_CpuUsageTooHigh;
            }
            else if (IS_OID(astOID, oid_MemUsageTooHighTrap))
            {
				stAlarmInfo.ulAlarmLevel = ALARM_LEVEL_MINOR;
                stAlarmInfo.ulAlarmID = SNMP_ALARM_CODE_MemUsageTooHigh;
            }
            else if (IS_OID(astOID, oid_LanStatusTrap))
            {
				stAlarmInfo.ulAlarmLevel = ALARM_LEVEL_NORMAL;
                stAlarmInfo.ulAlarmID = SNMP_ALARM_CODE_LanStatus;
            }
            else if (IS_OID(astOID, oid_PowerDownTrap))
            {
				stAlarmInfo.ulAlarmLevel = ALARM_LEVEL_NORMAL;
                stAlarmInfo.ulAlarmID = SNMP_ALARM_CODE_PowerDown;
            }
            else
            {
                TaskLog(LOG_ERROR, "TAU alarm trap error");
                return;
            }

        }

        OnAlarmTrap(stAlarmInfo, stDevInfo.stNeBasicInfo.ulDevType);
    }
    else if (IS_OID(astOID, oid_eOMC_HeartBeat))
    {
        OnHeartBeatTrap(pstClientAddr, (CHAR*)OMC_MODEL_NAME_EOMC);
    }
    else if (IS_OID(astOID, oid_ZTE_eOMC_HeartBeat))
    {
    }
    else if (IS_OID(astOID, oid_eOMC_AlarmReport))
    {
        if (!ParseEOmcAlarmTrap(pstVarList, stAlarmInfo))
        {
            TaskLog(LOG_ERROR, "parse eOMC alarm trap failed");
            return;
        }

         //OnAlarmTrap(stAlarmInfo);
    }
    else if (IS_OID(astOID, oid_AuthFailed))
    {
        TaskLog(LOG_ERROR, "auth failed to " IP_FMT, IP_ARG(pstClientAddr->aucIP));
    }
    else
    {
        CHAR    acOIDText[128] = {0};
        UINT32  ulLen = 0;

        for (UINT32 i=0; i<ulOIDSize; i++)
        {
            ulLen += snprintf(acOIDText+ulLen, sizeof(acOIDText)-ulLen-1, ".%u", astOID[i]);
        }

        TaskLog(LOG_ERROR, "unknown trap oid: %s", acOIDText+1);
    }
}
