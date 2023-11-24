#include "omc_public.h"
#include "dtp_public.h"
#include "TaskOmt.h"

#define CHECK_TIMER         TIMER10
#define EV_CHECK_TIMER      GET_TIMER_EV(CHECK_TIMER)

TaskOmt                 *g_TaskOmt = NULL;
UINT32                  g_ulRealBrdSeqID = 0;
CHAR                    *g_szNeBasicInfoFile = (CHAR *)"NeBasicInfo.ini";
extern CHAR             g_acClientVer[];
extern CHAR             *g_szVersion;
extern OMC_LOCAL_CFG_T                 g_stLocalCfg;

extern void test_history_alarm();

extern VECTOR<ALARM_LEVEL_CFG_T>      g_vAlarmCfg;

extern VOID SendSetUploadDevFileMsg(CHAR *szNeID, CHAR *szFile);
extern VOID SendRebootDevMsg(CHAR *szNeID);

TaskOmt::TaskOmt(UINT16 usDispatchID):GBaseTask(MODULE_OMT, "OMT", usDispatchID)
{
    m_bGetUserInfo = FALSE;
    m_bGetAlarmLevelCfg = FALSE;
    m_bGetActiveAlarmInfo = FALSE;
    m_bGetSwInfo = FALSE;
    m_bGetSwUpdate = FALSE;
    m_bGetStationInfo = FALSE;

    m_acUser[0]= '\0';
    m_szNeBasicFile = NULL;
    m_fp = NULL;
    m_pDao = NULL;

    RegisterMsg(EV_OMT_OMC_REQ);
    RegisterMsg(EV_OMT_GET_NEINFO_REQ);
    RegisterMsg(EV_OMT_GET_TXSTATUS_REQ);
    RegisterMsg(EV_OMT_GET_ALLTXSTATUS_REQ);
}

VOID TaskOmt::AddOperLog(const CHAR *szLog, ...)
{
//    return;

    if (!m_pDao)
    {
        return;
    }

    CHAR            acLog[4096];
    va_list         vaLog;

    va_start(vaLog, szLog);

    memset(acLog, 0, sizeof(acLog));
    vsnprintf(acLog, sizeof(acLog)-1, szLog, vaLog);

    va_end(vaLog);

    m_pDao->AddOperLog(gos_get_current_time(), m_acUser, acLog);
}

VOID TaskOmt::KickOutOMT(PID_T *pstPID)
{
    GJsonParam  Param;

    SendClient(pstPID, EV_OMT_OMC_RSP, MSG_OMC_KickOutOMT, 0, Param);
}

BOOL TaskOmt::Init()
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

    if (!m_bGetUserInfo)
    {
        VECTOR<OMT_USER_INFO_T>     vInfo;

        if (!m_pDao->GetUserInfo(vInfo))
        {
            TaskLog(LOG_ERROR, "load user info failed");

            return FALSE;
        }

        // 从数据库加载用户
        InitUserInfo(vInfo);

        TaskLog(LOG_INFO, "load user info succ");
        m_bGetUserInfo = TRUE;
    }

    if(!m_bGetAlarmLevelCfg)
    {
        VECTOR<ALARM_LEVEL_CFG_T>       vCfg;
        if(!m_pDao->GetAlarmLevelCfg(vCfg))
        {
            TaskLog(LOG_ERROR, "load alarm level cfg failed");

            return FALSE;
        }

        InitAlarmLevelCfg(vCfg);

        TaskLog(LOG_INFO, "load alarm level cfg succ");
        m_bGetAlarmLevelCfg = TRUE;
    }

    if (!m_bGetActiveAlarmInfo)
    {
        VECTOR<ALARM_INFO_T>        vInfo;

        if (!m_pDao->GetActiveAlarm(vInfo))
        {
            TaskLog(LOG_ERROR, "load active alarm info failed");

            return FALSE;
        }

        InitActiveAlarm(vInfo);

        TaskLog(LOG_INFO, "load active alarm info succ");
        m_bGetActiveAlarmInfo = TRUE;
    }

    if (!m_bGetSwInfo)
    {
        VECTOR<OMC_SW_INFO_T>       vInfo;

        if (!m_pDao->GetSwInfo(vInfo))
        {
            TaskLog(LOG_ERROR, "load sw info failed");

            return FALSE;
        }

        InitSwInfo(vInfo);

        TaskLog(LOG_INFO, "load sw  info succ");
        m_bGetSwInfo = TRUE;
    }

    if (!m_bGetSwUpdate)
    {
        VECTOR<OMC_SW_UPDATE_INFO_T>        vInfo;

        if (!m_pDao->GetSwUpdateInfo(vInfo))
        {
            TaskLog(LOG_ERROR, "load sw update info failed");

            return FALSE;
        }

        InitSwUpdate(vInfo);

        TaskLog(LOG_INFO, "load sw update info succ");
        m_bGetSwUpdate = TRUE;
    }

    if (!m_bGetStationInfo)
    {
        VECTOR<STATION_INFO_T>      vInfo;
        if  (!m_pDao->GetStationInfo(vInfo))
        {
            TaskLog(LOG_ERROR, "load station info failed");
            return FALSE;
        }

        InitStationInfo(vInfo);

        TaskLog(LOG_INFO, "load station info succ");
        m_bGetStationInfo = TRUE;
    }

    SetLoopTimer(CHECK_TIMER, 30*1000);
    SetTaskStatus(TASK_STATUS_WORK);

    TaskLog(LOG_INFO, "Task init successful!");

    return TRUE;
}

VOID TaskOmt::OnServerDisconnectClient(UINT16 usClientID)
{
}

VOID TaskOmt::OnOmtAddNeReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    UINT8           aucMAC[6];
    NE_BASIC_INFO_T stNEBasicInfo = {0};

    if (!Json.GetValue("MAC", stNEBasicInfo.acDevMac, sizeof(stNEBasicInfo.acDevMac)))
    {
        TaskLog(LOG_ERROR, "invalid MAC in msg %s", szReq);
        return;
    }

    if (!gos_get_mac(stNEBasicInfo.acDevMac, aucMAC))
    {
        TaskLog(LOG_ERROR, "invalid MAC in msg %s", szReq);
        return;
    }

    ToNeID(aucMAC, stNEBasicInfo.acNeID);

    if (!Json.GetValue("DevType", stNEBasicInfo.ulDevType))
    {
        TaskLog(LOG_ERROR, "invalid DevType in msg %s", szReq);
        return;
    }

    if (!CheckDevType(stNEBasicInfo.ulDevType))
    {
        TaskLog(LOG_ERROR, "unknown DevType in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("DevName", stNEBasicInfo.acDevName, sizeof(stNEBasicInfo.acDevName)))
    {
        TaskLog(LOG_ERROR, "invalid DevName in msg %s", szReq);
        return;
    }


    BOOL bRet = m_pDao->AddNe(stNEBasicInfo);

    if (!bRet)
    {
        TaskLog(LOG_ERROR, "add dev(%s) to db failed", stNEBasicInfo.acDevName);
    }
    else
    {
        TaskLog(LOG_INFO, "add ne %s", stNEBasicInfo.acNeID);
        SetNe(stNEBasicInfo.acNeID ,stNEBasicInfo.acDevName, stNEBasicInfo.ulDevType,stNEBasicInfo.acDevMac);
        AddOperLog("添加设备%s(%s)", stNEBasicInfo.acNeID, stNEBasicInfo.acDevName);
    }

    SendRsp(MSG_OMT_AddNe, ulSeqID, bRet);
}

VOID TaskOmt::OnOmtSetNeReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    CHAR    acDevName[32];
    NE_BASIC_INFO_T stNEBasicInfo = {0};

    if (!Json.GetValue("NeID", stNEBasicInfo.acNeID, sizeof(stNEBasicInfo.acNeID)))
    {
        TaskLog(LOG_ERROR, "invalid NeID in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("MAC", stNEBasicInfo.acDevMac, sizeof(stNEBasicInfo.acDevMac)))
    {
        TaskLog(LOG_ERROR, "invalid MAC in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("DevType", stNEBasicInfo.ulDevType))
    {
        TaskLog(LOG_ERROR, "invalid DevType in msg %s", szReq);
        return;
    }

    if (!CheckDevType(stNEBasicInfo.ulDevType))
    {
        TaskLog(LOG_ERROR, "unknown DevType in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("DevName", stNEBasicInfo.acDevName, sizeof(stNEBasicInfo.acDevName)))
    {
        TaskLog(LOG_ERROR, "invalid DevName in msg %s", szReq);
        return;
    }


    BOOL bRet = m_pDao->AddNe(stNEBasicInfo);
/*
    if (!Json.GetValue("NeID", acNeID, sizeof(acNeID)))
    {
        TaskLog(LOG_ERROR, "invalid NeID in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("MAC", acMAC, sizeof(acMAC)))
    {
        TaskLog(LOG_ERROR, "invalid MAC in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("DevType", &ulDevType))
    {
        TaskLog(LOG_ERROR, "invalid DevType in msg %s", szReq);
        return;
    }

    if (!CheckDevType(ulDevType))
    {
        TaskLog(LOG_ERROR, "unknown DevType in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("DevName", acDevName, sizeof(acDevName)))
    {
        TaskLog(LOG_ERROR, "invalid DevName in msg %s", szReq);
        return;
    }

    BOOL bRet = m_pDao->AddNe(acNeID, acDevName, ulDevType, acMAC);
*/
    if (!bRet)
    {
        TaskLog(LOG_ERROR, "save dev(%s) info to db failed", acDevName);
    }
    else
    {
        SetNe(stNEBasicInfo.acNeID ,stNEBasicInfo.acDevName, stNEBasicInfo.ulDevType,stNEBasicInfo.acDevMac);
        AddOperLog("设置设备名称%s(%s)", stNEBasicInfo.acNeID, stNEBasicInfo.acDevName);
    }

    SendRsp(MSG_OMT_SetNe, ulSeqID, bRet);
}

VOID TaskOmt::OnOmtDelNeReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    CHAR    acNeID[32];

    if (!Json.GetValue("NeID", acNeID, sizeof(acNeID)))
    {
        TaskLog(LOG_ERROR, "invalid NeID in msg %s", szReq);
        return;
    }

    BOOL bRet = m_pDao->DelNe(acNeID);

    if (!bRet)
    {
        TaskLog(LOG_ERROR, "delete dev(%s) from db failed", acNeID);
    }
    else
    {
        TaskLog(LOG_INFO, "delete ne %s", acNeID);
        DelNe(acNeID);
        AddOperLog("删除设备%s", acNeID);
    }

    SendRsp(MSG_OMT_DelNe, ulSeqID, bRet);
}

VOID TaskOmt::OnOmtRebootNeReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    CHAR    acNeID[32];

    if (!Json.GetValue("NeID", acNeID, sizeof(acNeID)))
    {
        TaskLog(LOG_ERROR, "invalid NeID in msg %s", szReq);
        return;
    }

    TaskLog(LOG_INFO, "reboot ne %s", acNeID);

    SendRsp(MSG_OMT_RebootNe, ulSeqID, TRUE);
    AddOperLog("重启设备%s", acNeID);
    SendRebootDevMsg(acNeID);
}
#if 0
void test_convert()
{
    VECTOR<OMA_DEV_INFO_T> vInfo;
    OMA_DEV_INFO_T      stDev;
    
    memset(&stDev, 0xff, sizeof(stDev));

    strcpy(stDev.stNeBasicInfo.acNeID, "1234567890ab");
    strcpy(stDev.stNeBasicInfo.acDevMac, "12-34-56-78-90-ab");
    strcpy(stDev.stNeBasicInfo.acDevIp, "192.168.30.150");
    
    SET_IP(stDev.stNeBasicInfo.stClientAddr.aucIP, 1,2,3,4);

    stDev.stNeBasicInfo.ulDevType = OMC_DEV_TYPE_TX;
    strcpy(stDev.stNeBasicInfo.acDevName, "调度台123");
    strcpy(stDev.stNeBasicInfo.acDevAlias, "调度台123");

    strcpy(stDev.stNeBasicInfo.acSoftwareVer, "调度台123");
    strcpy(stDev.stNeBasicInfo.acHardwareVer, "调度台123");
    strcpy(stDev.stNeBasicInfo.acModelName, "调度台123");
    strcpy(stDev.stNeBasicInfo.acRadioVer, "调度台123");
    strcpy(stDev.stNeBasicInfo.acAndroidVer, "调度台123");

  //  strcpy(stDev.stNeInfo.acVersion, "调度台123");
   // stDev.unDevStatus.stTxStatus.stLteStatus.

    UINT32      ulNeNum = 1600;

    for (UINT32 i=0; i<ulNeNum; i++)
    {
        vInfo.push_back(stDev);
    }

    gos_time_init();
   
    UINT32      ulNum = 100;

    for (UINT32 i=0; i<ulNum; i++)
    {
        static GJsonTupleParam JsonTupleParam;

        //UINT32      ulTime = gos_get_uptime_1ms();
        INT64       i64Time = gos_get_uptime_1ns();

        ConvertNeBasicInfoToJson(vInfo, JsonTupleParam);

        //ulTime = gos_get_uptime_1ms() - ulTime;
        //GosLog(LOG_INFO, "time = %u", ulTime);

        i64Time = (gos_get_uptime_1ns() - i64Time)/1000000;
        GosLog(LOG_INFO, "time = "FORMAT_I64, i64Time);

     //   gos_sleep_1ms(1);
    }

    getchar();
    
    return;
}
#endif

VOID TaskOmt::OnOmtGetNeBasicInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    static  VECTOR<OMA_DEV_INFO_T>      vDevInfo;
    static  GJsonTupleParam             JsonTupleParam;
    GJsonParam  RspJsonParam;
    CHAR        acFile[128] = {0};

    // 写Json文件,将文件名发生给客户端
    GetDevInfo(vDevInfo); // 获取设备基础信息

    ConvertNeBasicInfoToJson(vDevInfo, JsonTupleParam);

    sprintf(acFile, "%s/ne_basic_info_%u.dat", gos_get_root_path(), GetMsgSender()->usInstID);
    gos_save_string_to_file(acFile, JsonTupleParam.GetString());

    RspJsonParam.Add("FileName", acFile+strlen(gos_get_root_path())+1);

    SendRsp(MSG_OMT_GetNeBasicInfo, ulSeqID, RspJsonParam);
}

// 处理 omt 获取网元信息的请求
VOID TaskOmt::OnOmtGetNeInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    CHAR            acNeID[32];
    OMA_DEV_INFO_T  stInfo;
    GJsonParam      JsonParam;

    if (!Json.GetValue("NeID", acNeID, sizeof(acNeID)))
    {
        TaskLog(LOG_ERROR, "invalid NeID in msg %s", szReq);
        return;
    }

    if (!GetDevInfo(acNeID, stInfo))
    {
        TaskLog(LOG_ERROR, "invalid NeID in msg %s", szReq);
        return;
    }

    ConvertNeInfoToJson(stInfo, JsonParam);

    SendRsp(MSG_OMT_GetNeInfo, ulSeqID, JsonParam);
}

VOID TaskOmt::OnOmtClearActiveAlarmReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    VECTOR<ALARM_INFO_T>    vInfo;
    CHAR                    acNeID[32];
    UINT32                  ulAlarmID;
    UINT32                  ulAlarmLevel;
    UINT32                  ulAlarmRaiseTime;
    CHAR                    acAlarmClearInfo[64];
    BOOL                    bRet;

    if (!Json.GetValue("ne_id", acNeID, sizeof(acNeID)))
    {
        TaskLog(LOG_ERROR, "invalid ne_id in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("alarm_id", &ulAlarmID))
    {
        TaskLog(LOG_ERROR, "invalid alarm_reason_id in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("alarm_level", &ulAlarmLevel))
    {
        TaskLog(LOG_ERROR, "invalid alarm_level in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("alarm_raise_time", &ulAlarmRaiseTime))
    {
        TaskLog(LOG_ERROR, "invalid alarm_raise_time in msg %s", szReq);
        return;
    }

    sprintf(acAlarmClearInfo, "用户%s手动清除",m_acUser);
    bRet = m_pDao->ClearActiveAlarm(acNeID, ulAlarmID, ulAlarmRaiseTime, ulAlarmLevel, acAlarmClearInfo);
    if (!bRet)
    {
        TaskLog(LOG_ERROR, "TaskOmt clear active alarm %s %u failed", acNeID, ulAlarmID);
    }
    else
    {
        DelActiveAlarm(acNeID, ulAlarmID);
        AddOperLog("清除活跃告警%s(%d)",acNeID, ulAlarmID);
    }

    SendRsp(MSG_OMT_ClearActiveAlarm, ulSeqID, bRet);
}

VOID TaskOmt::OnOmtGetActiveAlarmReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    //PROFILER();
    VECTOR<ALARM_INFO_T>    vInfo;
    GJsonTupleParam         JsonTupleParam;
    CHAR                    acFile[64] = {0};

    GetActiveAlarm(vInfo);
    ConvertActiveAlarmInfoToJson(vInfo, JsonTupleParam);
    // 若活跃告警数量超过平台发送一个包的大小，则改为传文件
    // 若未超过则直接回Json报文
    if (JsonTupleParam.GetStringSize() >= (DTP_MAX_MSG_LEN-1000))
    {
        // 每个客户端生成唯一的文件
        sprintf(acFile, "%s/active_alarm_info_%u.dat", gos_get_root_path(), GetMsgSender()->usInstID);
        // 保存成Json格式
        gos_save_string_to_file(acFile, JsonTupleParam.GetString());

        GJsonParam      ParamResult;
        // 返回相对路径到客户端
        ParamResult.Add("File", acFile+strlen(gos_get_root_path())+1);
        SendRsp(MSG_OMT_GetActiveAlarm, ulSeqID, ParamResult);     
    }
    else
    {
        SendRsp(MSG_OMT_GetActiveAlarm, ulSeqID, JsonTupleParam);
    }
}

VOID TaskOmt::OnOmtGetHistoryAlarmReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    GJsonTupleParam     JsonTupleParam;
    UINT32              ulDevType = 0;
    CHAR                *szNeID = NULL;
    UINT32              ulStartTime;
    UINT32              ulEndTime;
    CHAR                *szAlarmInfoKey;

    if (!Json.GetValue("DevType", &ulDevType))
    {
        TaskLog(LOG_ERROR, "invalid DevType in msg %s", szReq);
        return;
    }

    szNeID = Json.GetValue("NeID");
    if (!szNeID)
    {
        TaskLog(LOG_ERROR, "invalid NeID in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("StartTime", &ulStartTime))
    {
        TaskLog(LOG_ERROR, "invalid StartTime in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("EndTime", &ulEndTime))
    {
        TaskLog(LOG_ERROR, "invalid EndTime in msg %s", szReq);
        return;
    }

    szAlarmInfoKey = Json.GetValue("AlarmInfoKey");
    if (!szAlarmInfoKey)
    {
        TaskLog(LOG_ERROR, "invalid AlarmInfoKey in msg %s", szReq);
        return;
    }

    if (!m_pDao->GetHistoryAlarm(ulDevType, szNeID, ulStartTime, ulEndTime, szAlarmInfoKey, JsonTupleParam))
    {
        return;
    }

    SendRsp(MSG_OMT_GetHistoryAlarm, ulSeqID, JsonTupleParam);
}

VOID TaskOmt::OnOmtGetAlarmStatReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    GJsonParam          AlarmStatParam;
    GJsonTupleParam     AlarmTrendTupleParam;
    GJsonParam          RspParam;
    OMC_ALARM_STAT_T    stAlarmStat;
    OMC_ALARM_TREND_T   stAlarmTrend;
    OMC_ALARM_STAT_T    stHistoryAlarmStat = {0};

    // 获取活跃告警统计信息，从内存中获取。
    // 获取24小时内活跃告警统计结果
    GetActiveAlarmStat(stAlarmStat, stAlarmTrend);
    AlarmStatParam.Add("CriticalAlarm", stAlarmStat.ulCriticalAlarmNum);
    AlarmStatParam.Add("MainAlarm", stAlarmStat.ulMainAlarmNum);
    AlarmStatParam.Add("MinorAlarm", stAlarmStat.ulMinorAlarmNum);
    AlarmStatParam.Add("NormalAlarm", stAlarmStat.ulNormalAlarmNum);
    RspParam.Add("ActiveAlarmStat", AlarmStatParam);

    // 获取24小时内，每个小时的活跃告警统计结果
    for (UINT32 i=0; i<stAlarmTrend.ulNum; i++)
    {
        AlarmStatParam.Clear();
        memcpy(&stAlarmStat, &stAlarmTrend.astAlarmTrend[i], sizeof(stAlarmStat));

        AlarmStatParam.Add("Time", stAlarmStat.ulTime);
        AlarmStatParam.Add("CriticalAlarm", stAlarmStat.ulCriticalAlarmNum);
        AlarmStatParam.Add("MainAlarm", stAlarmStat.ulMainAlarmNum);
        AlarmStatParam.Add("MinorAlarm", stAlarmStat.ulMinorAlarmNum);
        AlarmStatParam.Add("NormalAlarm", stAlarmStat.ulNormalAlarmNum);
        AlarmTrendTupleParam.Add(AlarmStatParam);
    }
    RspParam.Add("ActiveAlarmTrend", AlarmTrendTupleParam);

    // 获取历史告警统计信息，从数据库 history_alarm_statistic 表获取数据
    UINT32 ulQueryEndTime= gos_get_day_from_time(gos_get_current_time()); // 当前时间零点的时间戳
    UINT32 ulQueryBeginTime = ulQueryEndTime - g_stLocalCfg.ulHistoryAlarmStatisticDay * 24 * 60 * 60; // 推后统计历史告警时间天的秒数
    VECTOR<OMC_ALARM_STAT_T> vecAlarmLevelDayCount; // 90天内每一天不同等级告警数量统计
    // 获取90天内不同等级告警的数量总和
    if (!m_pDao->GetHistoryAlarmTotalCount(ulQueryBeginTime, ulQueryEndTime, stHistoryAlarmStat))
    {
        GosLog(LOG_ERROR, "GetHistoryAlarmTotalCount(%u, %u) failed!", ulQueryBeginTime, ulQueryEndTime);
        return;
    }
    else
    {
        // 90天内历史告警统计结果
        AlarmStatParam.Clear();
        AlarmStatParam.Add("CriticalAlarm", stHistoryAlarmStat.ulCriticalAlarmNum);
        AlarmStatParam.Add("MainAlarm", stHistoryAlarmStat.ulMainAlarmNum);
        AlarmStatParam.Add("MinorAlarm", stHistoryAlarmStat.ulMinorAlarmNum);
        AlarmStatParam.Add("NormalAlarm", stHistoryAlarmStat.ulNormalAlarmNum);
    }   
    RspParam.Add("HistoryAlarmStat", AlarmStatParam);

    // 获取90天内，每一天不同等级告警的数量
    if (!m_pDao->GetHistoryAlarmDayCount(ulQueryBeginTime, ulQueryEndTime, vecAlarmLevelDayCount))
    {
        GosLog(LOG_ERROR, "GetHistoryAlarmDayCount(%u, %u) failed!", ulQueryBeginTime, ulQueryEndTime);
        return;
    }
    else
    {
        AlarmTrendTupleParam.Clear();
        for (UINT32 i=0; i<vecAlarmLevelDayCount.size(); i++)
        {
            AlarmStatParam.Clear();
            memcpy(&stAlarmStat, &vecAlarmLevelDayCount[i], sizeof(stAlarmStat));

            AlarmStatParam.Add("Time", vecAlarmLevelDayCount[i].ulTime);
            AlarmStatParam.Add("CriticalAlarm", vecAlarmLevelDayCount[i].ulCriticalAlarmNum);
            AlarmStatParam.Add("MainAlarm", vecAlarmLevelDayCount[i].ulMainAlarmNum);
            AlarmStatParam.Add("MinorAlarm", vecAlarmLevelDayCount[i].ulMinorAlarmNum);
            AlarmStatParam.Add("NormalAlarm", vecAlarmLevelDayCount[i].ulNormalAlarmNum);
            AlarmTrendTupleParam.Add(AlarmStatParam);
        }
    
    }
    RspParam.Add("HistoryAlarmTrend", AlarmTrendTupleParam);
    SendRsp(MSG_OMT_GetAlarmStat, ulSeqID, RspParam);
}

VOID TaskOmt::OnOmtGetAlarmLevelCfgReq(CHAR *szReq, UINT32 ulSeqID,GJson &Json)
{
    VECTOR<ALARM_LEVEL_CFG_T> vCfg;
    GJsonTupleParam         JsonTupleParam;

    GetAlarmLevelCfg(vCfg);
    ConvertAlarmLevelCfgToJson(vCfg, JsonTupleParam);
    SendRsp(MSG_OMT_GetAlarmLevelCfg, ulSeqID, JsonTupleParam);
}

VOID TaskOmt::OnOmtAddAlarmLevelCfgReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    ALARM_LEVEL_CFG_T  stCfg = {0};

	if (!Json.GetValue("dev_type", &stCfg.ulDevType))
    {
       	TaskLog(LOG_ERROR, "invalid DevType in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("alarm_id", &stCfg.ulAlarmID))
    {
        TaskLog(LOG_ERROR, "invalid AlarmID in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("alarm_level", &stCfg.ulAlarmLevel))
    {
        TaskLog(LOG_ERROR, "invalid AlarmLevel in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("alarm_status", &stCfg.bIsIgnore))
    {
        TaskLog(LOG_ERROR, "invalid AlarmStatus in msg %s", szReq);
        return;
    }

    BOOL bRet = m_pDao->AddAlarmLevelCfg(stCfg);
    if (!bRet)
    {
		TaskLog(LOG_ERROR, "add dev_alarm_cfg(%u-%u) to db failed", stCfg.ulDevType, stCfg.ulAlarmID);
    }
    else
    {
		TaskLog(LOG_INFO, "add dev_alarm_cfg(%u-%u) succ", stCfg.ulDevType, stCfg.ulAlarmID);
		AddOperLog("添加告警配置(%u-%u)", stCfg.ulDevType, stCfg.ulAlarmID);
    }

    AddAlarmLevelCfg(stCfg);
    SendRsp(MSG_OMT_AddAlarmLevelCfg, ulSeqID, bRet);
}

VOID TaskOmt::OnOmtDelAlarmLevelCfgReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    ALARM_LEVEL_CFG_T  stCfg = {0};

	if (!Json.GetValue("dev_type", &stCfg.ulDevType))
    {
       	TaskLog(LOG_ERROR, "invalid DevType in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("alarm_id", &stCfg.ulAlarmID))
    {
        TaskLog(LOG_ERROR, "invalid AlarmID in msg %s", szReq);
        return;
    }

    BOOL bRet = m_pDao->DelAlarmLevelCfg(stCfg);
    if (!bRet)
    {
		TaskLog(LOG_ERROR, "del dev_alarm_cfg(%u-%u) to db failed", stCfg.ulDevType, stCfg.ulAlarmID);
    }
    else
    {
        TaskLog(LOG_INFO, "del dev_alarm_cfg(%u-%u) succ", stCfg.ulDevType, stCfg.ulAlarmID);
        AddOperLog("删除告警配置(%u-%u)", stCfg.ulDevType, stCfg.ulAlarmID);
    }

    DelAlarmLevelCfg(stCfg);
    SendRsp(MSG_OMT_DelAlarmLevelCfg, ulSeqID, bRet);
}

VOID TaskOmt::OnOmtSetAlarmLevelCfgReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    ALARM_LEVEL_CFG_T  stCfg = {0};

	if (!Json.GetValue("dev_type", &stCfg.ulDevType))
    {
       	TaskLog(LOG_ERROR, "invalid DevType in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("alarm_id", &stCfg.ulAlarmID))
    {
        TaskLog(LOG_ERROR, "invalid AlarmID in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("alarm_level", &stCfg.ulAlarmLevel))
    {
        TaskLog(LOG_ERROR, "invalid AlarmLevel in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("alarm_status", &stCfg.bIsIgnore))
    {
        TaskLog(LOG_ERROR, "invalid AlarmStatus in msg %s", szReq);
        return;
    }

    BOOL bRet = m_pDao->SetAlarmLevelCfg(stCfg);
    if (!bRet)
    {
		TaskLog(LOG_ERROR, "set dev_alarm_cfg(%u-%u) to db failed", stCfg.ulDevType, stCfg.ulAlarmID);
    }
    else
    {
        TaskLog(LOG_INFO, "set dev_alarm_cfg(%u-%u) succ", stCfg.ulDevType, stCfg.ulAlarmID);
		AddOperLog("修改告警配置(%u-%u)", stCfg.ulDevType, stCfg.ulAlarmID);
    }

    SetAlarmLevelCfg(stCfg);
    SendRsp(MSG_OMT_SetAlarmLevelCfg, ulSeqID, bRet);
}

VOID TaskOmt::OnOmtGetAllDevStatusReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    UINT32          ulTime;
    CHAR            acFile[256];

    if (!Json.GetValue("Time", &ulTime))
    {
        TaskLog(LOG_ERROR, "invalid Time in msg %s", szReq);
        return;
    }

    GJsonParam      Param;
    GJsonTupleParam TupleParam;
    // 查询设备的实时基本状态信息，直接从内存中获取，无需查询数据库操作。
    if (ulTime == 0xffffffff)
    {
        VECTOR<OMA_DEV_INFO_T>      vDevInfo;
        UINT32                      ulCurrUptime = gos_get_uptime();

        GetDevInfo(vDevInfo);

        for (UINT32 i=0; i<vDevInfo.size(); i++)
        {
            OMA_DEV_INFO_T      &stInfo = vDevInfo[i];

            Param.Clear();

            Param.Add("Time", stInfo.ulTime);
            Param.Add("NeID", stInfo.stNeBasicInfo.acNeID);

            if (stInfo.stNeBasicInfo.ulLastOnlineTime == 0 ||
                (ulCurrUptime - stInfo.stNeBasicInfo.ulLastOnlineTime) > g_stLocalCfg.ulOnlineCheckPeriod)
            {
                Param.Add("Online", FALSE);
            }
            else
            {
                Param.Add("Online", TRUE);
            }

            if (stInfo.stNeBasicInfo.ulDevType == OMC_DEV_TYPE_TX)
            {
                Param.Add("Cpu", stInfo.unDevStatus.stTxStatus.stResStatus.ulCpuUsage);
                Param.Add("Mem", stInfo.unDevStatus.stTxStatus.stResStatus.ulMemUsage);
                Param.Add("Disk", stInfo.unDevStatus.stTxStatus.stResStatus.ulDiskUsage);

                Param.Add("Station", stInfo.unDevStatus.stTxStatus.stLteStatus.ulStationID);
                Param.Add("PCI", stInfo.unDevStatus.stTxStatus.stLteStatus.ulPCI);
                Param.Add("RSRP", stInfo.unDevStatus.stTxStatus.stLteStatus.iRSRP);
                Param.Add("RSRQ", stInfo.unDevStatus.stTxStatus.stLteStatus.iRSRQ);
                Param.Add("RSSI", stInfo.unDevStatus.stTxStatus.stLteStatus.iRSSI);
                Param.Add("SINR", stInfo.unDevStatus.stTxStatus.stLteStatus.iSINR);
            }
            else if (stInfo.stNeBasicInfo.ulDevType == OMC_DEV_TYPE_FX)
            {
                Param.Add("Cpu", stInfo.unDevStatus.stFxStatus.stResStatus.ulCpuUsage);
                Param.Add("Mem", stInfo.unDevStatus.stFxStatus.stResStatus.ulMemUsage);
                Param.Add("Disk", stInfo.unDevStatus.stFxStatus.stResStatus.ulDiskUsage);

                Param.Add("Station", stInfo.unDevStatus.stFxStatus.stLteStatus.ulStationID);
                Param.Add("PCI", stInfo.unDevStatus.stFxStatus.stLteStatus.ulPCI);
                Param.Add("RSRP", stInfo.unDevStatus.stFxStatus.stLteStatus.iRSRP);
                Param.Add("RSRQ", stInfo.unDevStatus.stFxStatus.stLteStatus.iRSRQ);
                Param.Add("RSSI", stInfo.unDevStatus.stFxStatus.stLteStatus.iRSSI);
                Param.Add("SINR", stInfo.unDevStatus.stFxStatus.stLteStatus.iSINR);
            }
            else if (stInfo.stNeBasicInfo.ulDevType == OMC_DEV_TYPE_DC)
            {
                Param.Add("Cpu", stInfo.unDevStatus.stDcStatus.stResStatus.ulCpuUsage);
                Param.Add("Mem", stInfo.unDevStatus.stDcStatus.stResStatus.ulMemUsage);
                Param.Add("Disk", stInfo.unDevStatus.stDcStatus.stResStatus.ulDiskUsage);
            }
            else if (stInfo.stNeBasicInfo.ulDevType == OMC_DEV_TYPE_DIS)
            {
                Param.Add("Cpu", stInfo.unDevStatus.stDisStatus.stResStatus.ulCpuUsage);
                Param.Add("Mem", stInfo.unDevStatus.stDisStatus.stResStatus.ulMemUsage);
                Param.Add("Disk", stInfo.unDevStatus.stDisStatus.stResStatus.ulDiskUsage);
                Param.Add("ClusterStatus", stInfo.unDevStatus.stDisStatus.ulClusterStatus);
            }
            else if (stInfo.stNeBasicInfo.ulDevType == OMC_DEV_TYPE_TAU)
            {
                Param.Add("Cpu", stInfo.unDevStatus.stTauStatus.stResStatus.ulCpuUsage);
                Param.Add("Mem", stInfo.unDevStatus.stTauStatus.stResStatus.ulMemUsage);
                Param.Add("Disk", stInfo.unDevStatus.stTauStatus.stResStatus.ulDiskUsage);

                Param.Add("PCI", stInfo.unDevStatus.stTauStatus.stLteStatus.ulPCI);
                Param.Add("RSRP", stInfo.unDevStatus.stTauStatus.stLteStatus.iRSRP);
                Param.Add("RSRQ", stInfo.unDevStatus.stTauStatus.stLteStatus.iRSRQ);
                Param.Add("RSSI", stInfo.unDevStatus.stTauStatus.stLteStatus.iRSSI);
                Param.Add("SINR", stInfo.unDevStatus.stTauStatus.stLteStatus.iSINR);
            }

            TupleParam.Add(Param);
        }
    }
    else if (!m_pDao->GetAllDevStatus(ulTime, TupleParam)) // 查询设备的历史状态信息，从数据库查询。
    {
        TaskLog(LOG_ERROR, "get all dev status failed");
        return;
    }

    // 每个客户端生成唯一的文件
    sprintf(acFile, "%s/dev_status_%u.dat", gos_get_root_path(), GetMsgSender()->usInstID);
    // 保存成Json格式
    gos_save_string_to_file(acFile, TupleParam.GetString());

    GJsonParam      ParamResult;
    // 返回相对路径到客户端
    ParamResult.Add("File", acFile+strlen(gos_get_root_path())+1);

    SendRsp(MSG_OMT_GetAllDevStatus, ulSeqID, ParamResult);
}

VOID TaskOmt::OnOmtGetDevStatusReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    //PROFILER();
    CHAR            *szFilePrefix = (CHAR*)"ne_basic_status_";
    CHAR            acTime[256];
    CHAR            acFile[256];
    CHAR            acNewFile[256];
    UINT32          ulStartTime = 0;
    UINT32          ulEndTime = 0;
    UINT32          ulTime = 0;
    CHAR            acNeID[NE_ID_LEN] = {0};

    if (!Json.GetValue("NeID", acNeID, sizeof(acNeID)) ||
        !Json.GetValue("Time", &ulTime))
    {
        TaskLog(LOG_ERROR, "invalid msg %s", szReq);
        return;
    }

    if (ulTime == 0xffffffff)
    {
        GJsonParam      Param;
        GJsonTupleParam TupleParam;

        ulEndTime = gos_get_current_time();
        ulStartTime = ulEndTime - 3600;
        if (!m_pDao->GetDevStatus(ulStartTime, ulEndTime, acNeID, TupleParam))
        {
            TaskLog(LOG_ERROR, "get dev status failed");
            return;
        }
    
        SendRsp(MSG_OMT_GetDevStatus, ulSeqID, TupleParam);
    }
    else
    {
        GJson Json;

        gos_get_text_time_ex(&ulTime, "%04u%02u%02u%02u", acTime);
        sprintf(acFile, "%s/%s%s.zip", g_stLocalCfg.acBackupDir, szFilePrefix, acTime);
        if (!gos_file_exist(acFile))
        {
            TaskLog(LOG_ERROR, "file %s does not exist", acFile);
            return;
        }
        else
        {
            sprintf(acNewFile, "%s/%s%s.zip", gos_get_root_path(), szFilePrefix, acTime);
            gos_copy_file(acFile, acNewFile);
        }

        GJsonParam      ParamResult;
        // 返回相对路径到客户端
        ParamResult.Add("File", acNewFile+strlen(gos_get_root_path())+1);
        SendRsp(MSG_OMT_GetDevStatus, ulSeqID, ParamResult);      
    }
}

VOID TaskOmt::OnOmtGetLteStatusReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    CHAR            *szFilePrefix = (CHAR*)"ne_basic_status_";
    UINT32          ulStartTime = 0;
    UINT32          ulEndTime = 0;
    UINT32          ulTime = 0;
    CHAR            acTime[256];
    CHAR            acFile[256];
    CHAR            acNewFile[256];
    CHAR            acNeID[NE_ID_LEN];

    if (!Json.GetValue("NeID", acNeID, sizeof(acNeID)) ||
        !Json.GetValue("Time", &ulTime))
    {
        TaskLog(LOG_ERROR, "invalid msg %s", szReq);
        return;
    }

    if (ulTime == 0xffffffff)
    {
        GJsonParam      Param;
        GJsonTupleParam TupleParam;

        ulEndTime = gos_get_current_time();
        ulStartTime = ulEndTime - 3600;
        if (!m_pDao->GetLteStatus(ulStartTime, ulEndTime, acNeID, TupleParam))
        {
            TaskLog(LOG_ERROR, "get lte status failed");
            return;
        }

        SendRsp(MSG_OMT_GetLteStatus, ulSeqID, TupleParam);
    }
    else
    {
        GJson Json;

        gos_get_text_time_ex(&ulTime, "%04u%02u%02u%02u", acTime);
        sprintf(acFile, "%s/%s%s.zip", g_stLocalCfg.acBackupDir, szFilePrefix, acTime);
        if (!gos_file_exist(acFile))
        {
            TaskLog(LOG_ERROR, "file %s does not exist", acFile);
            return;
        }
        else
        {
            sprintf(acNewFile, "%s/%s%s.zip", gos_get_root_path(), szFilePrefix, acTime);
            gos_copy_file(acFile, acNewFile);
        }

        GJsonParam      ParamResult;
        // 返回相对路径到客户端
        ParamResult.Add("File", acNewFile+strlen(gos_get_root_path())+1);
        SendRsp(MSG_OMT_GetLteStatus, ulSeqID, ParamResult);
    }
}

VOID TaskOmt::OnOmtGetTrafficStatusReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    CHAR            *szFilePrefix = (CHAR*)"traffic_status_";
    CHAR            acFile[256];
    CHAR            acTime[256];
    CHAR            acNewFile[256];
    UINT32          ulStartTime;
    UINT32          ulEndTime;
    UINT32          ulTime = 0;
    CHAR            acNeID[NE_ID_LEN];

    if (!Json.GetValue("NeID", acNeID, sizeof(acNeID)) ||
        !Json.GetValue("Time", &ulTime)
        )
    {
        TaskLog(LOG_ERROR, "invalid msg %s", szReq);
        return;
    }

    // 获取当前1小时之内的数据
    if (ulTime == 0xffffffff)
    {        
        GJsonParam      Param;
        GJsonTupleParam TupleParam;

        ulEndTime = gos_get_current_time();
        ulStartTime = ulEndTime - 3600;

        if (!m_pDao->GetTrafficStatus(ulStartTime, ulEndTime, acNeID, TupleParam))
        {
            TaskLog(LOG_ERROR, "get lte status failed");
            return;
        }
        SendRsp(MSG_OMT_GetTrafficStatus, ulSeqID, TupleParam);
    }
    else
    {
        GJsonParam      ParamResult;

        gos_get_text_time_ex(&ulTime, "%04u%02u%02u%02u", acTime);
        sprintf(acFile, "%s/%s%s.zip", g_stLocalCfg.acBackupDir, szFilePrefix, acTime);
        if (!gos_file_exist(acFile))
        {
            TaskLog(LOG_ERROR, "file %s does not exist", acFile);
            return;
        }
        else
        {
            sprintf(acNewFile, "%s/%s%s.zip", gos_get_root_path(), szFilePrefix, acTime);
            gos_copy_file(acFile, acNewFile);
        }
        
        // 返回相对路径到客户端
        ParamResult.Add("File", acFile+strlen(g_stLocalCfg.acBackupDir)+1);
        SendRsp(MSG_OMT_GetTrafficStatus, ulSeqID, ParamResult);
    }
}

VOID TaskOmt::OnOmtGetDevOperLogReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    CHAR    acNeID[32];
    UINT32  ulStartTime;
    UINT32  ulEndTime;
    GJsonTupleParam     TupleParam;

    if (!Json.GetValue("NeID", acNeID, sizeof(acNeID)))
    {
        TaskLog(LOG_ERROR, "invalid NeID in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("StartTime", &ulStartTime))
    {
        TaskLog(LOG_ERROR, "invalid StartTime in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("EndTime", &ulEndTime))
    {
        TaskLog(LOG_ERROR, "invalid EndTime in msg %s", szReq);
        return;
    }

    if (!m_pDao->GetDevOperLog(acNeID, ulStartTime, ulEndTime, TupleParam))
    {
        TaskLog(LOG_ERROR, "get dev(%s) oper log failed", acNeID);
        return;
    }

    SendRsp(MSG_OMT_GetDevOperLog, ulSeqID, TupleParam);
}

VOID TaskOmt::OnOmtGetDevFileListReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    UINT32  ulDevFileType;
    CHAR    acNeID[32];
    UINT32  ulStartTime;
    UINT32  ulEndTime;
    GJsonTupleParam     TupleParam;

    if (!Json.GetValue("DevFileType", &ulDevFileType))
    {
        TaskLog(LOG_ERROR, "invalid DevFileType in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("NeID", acNeID, sizeof(acNeID)))
    {
        TaskLog(LOG_ERROR, "invalid NeID in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("StartTime", &ulStartTime))
    {
        TaskLog(LOG_ERROR, "invalid StartTime in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("EndTime", &ulEndTime))
    {
        TaskLog(LOG_ERROR, "invalid EndTime in msg %s", szReq);
        return;
    }

    if (!m_pDao->GetDevFile(ulDevFileType, acNeID, ulStartTime, ulEndTime, TupleParam))
    {
        TaskLog(LOG_ERROR, "get dev(%s) file list failed", acNeID);
        return;
    }

    SendRsp(MSG_OMT_GetDevFileList, ulSeqID, TupleParam);
}

VOID TaskOmt::SendConfirmSaveRsp(UINT32 ulSeqID)
{
    GJsonParam  Param;

    Param.Add("Result", "true");
    Param.Add("UserID", m_acUser);

    SendClient(GetMsgSender(), EV_OMT_OMC_RSP, MSG_DCConfirmSave, ulSeqID, Param);
}

/*
VOID TaskOmt::OnOmtGetNeInfoReq(CHAR *szReq, UINT32 ulMsgLen)
{
    CHAR            *szNeID = (CHAR*)szReq;
    OMA_DEV_INFO_T  stDevInfo;

    if (!GetDevInfo(szNeID, stDevInfo))
    {
        TaskLog(LOG_ERROR, "unknown dev %s", szNeID);
        return;
    }

    DevInfoHton(stDevInfo);

    SendRsp(&stDevInfo, sizeof(stDevInfo));
}
*/

VOID TaskOmt::OnOmtGetTxStatusReq(CHAR *szReq, UINT32 ulMsgLen)
{
    OMT_GET_TXSTATUS_REQ_T  *pstReq = (OMT_GET_TXSTATUS_REQ_T*)szReq;
    OMT_GET_TXSTATUS_RSP_T  *pstRsp = (OMT_GET_TXSTATUS_RSP_T*)AllocBuffer(sizeof(OMT_GET_TXSTATUS_RSP_T));
    VECTOR<OMA_TX_STATUS_REC_T>     vRec;
    UINT32                          ulRspMsgLen = 0;

    if (ulMsgLen != sizeof(OMT_GET_TXSTATUS_REQ_T))
    {
        TaskLog(LOG_ERROR, "invalid msg len %u of OMT_GET_TXSTATUS_REQ", ulMsgLen);
        return;
    }

    NTOHL_IT(pstReq->ulStartTime);
    NTOHL_IT(pstReq->ulEndTime);

    if (!m_pDao->GetTxStatus(pstReq->ulStartTime, pstReq->ulEndTime, pstReq->acNeID, pstRsp))
    {
        TaskLog(LOG_ERROR, "get tx status failed");
        return;
    }

    ulRspMsgLen = STRUCT_OFFSET(OMT_GET_TXSTATUS_RSP_T, astRec) + pstRsp->ulNum*sizeof(pstRsp->astRec[0]);

    for (UINT32 i=0; i<pstRsp->ulNum; i++)
    {
        HTONL_IT(pstRsp->astRec[i].ulTime);
        HTONL_IT(pstRsp->astRec[i].stStatus.stLteStatus.ulStationID);
        HTONL_IT(pstRsp->astRec[i].stStatus.stResStatus.ulCpuUsage);
        HTONL_IT(pstRsp->astRec[i].stStatus.stResStatus.ulMemUsage);
        HTONL_IT(pstRsp->astRec[i].stStatus.stResStatus.ulDiskUsage);
        HTONL_IT(pstRsp->astRec[i].stStatus.stLteStatus.ulPCI);
        HTONL_IT(pstRsp->astRec[i].stStatus.stLteStatus.iRSRP);
        HTONL_IT(pstRsp->astRec[i].stStatus.stLteStatus.iRSRQ);
        HTONL_IT(pstRsp->astRec[i].stStatus.stLteStatus.iRSSI);
        HTONL_IT(pstRsp->astRec[i].stStatus.stLteStatus.iSINR);
        HTONL_IT(pstRsp->astRec[i].stStatus.ulRunStatus);
    }

    HTONL_IT(pstRsp->ulNum);

    SendRsp(pstRsp, ulRspMsgLen);
}

VOID TaskOmt::OnOmtGetAllTxStatusReq(CHAR *szReq, UINT32 ulMsgLen)
{
    OMT_GET_ALLTXSTATUS_REQ_T   *pstReq = (OMT_GET_ALLTXSTATUS_REQ_T*)szReq;
    OMT_GET_ALLTXSTATUS_RSP_T   *pstRsp = (OMT_GET_ALLTXSTATUS_RSP_T*)AllocBuffer(sizeof(OMT_GET_ALLTXSTATUS_RSP_T));
    UINT32                      ulRspMsgLen = 0;

    if (ulMsgLen != sizeof(OMT_GET_ALLTXSTATUS_REQ_T))
    {
        TaskLog(LOG_ERROR, "invalid msg len %u of OMT_GET_ALLTXSTATUS_REQ", ulMsgLen);
        return;
    }

    NTOHL_IT(pstReq->ulTime);

    if (pstReq->ulTime == 0xffffffff)
    {
        VECTOR<OMA_DEV_INFO_T>      vDevInfo;
        UINT32                      ulTime = gos_get_current_time();

        GetDevInfo(OMC_DEV_TYPE_TX, vDevInfo);

        pstRsp->ulNum = vDevInfo.size();
        if (pstRsp->ulNum > ARRAY_SIZE(pstRsp->astRec))
        {
            pstRsp->ulNum = ARRAY_SIZE(pstRsp->astRec);
        }

        for (UINT32 i=0; i<pstRsp->ulNum; i++)
        {
            pstRsp->astRec[i].ulTime = ulTime;
            strcpy(pstRsp->astRec[i].acNeID, vDevInfo[i].stNeBasicInfo.acNeID);
            memcpy(&pstRsp->astRec[i].stStatus, &vDevInfo[i].unDevStatus.stTxStatus, sizeof(OMA_TX_STATUS_REC_T));
        }
    }
    else if (!m_pDao->GetAllTxStatus(pstReq->ulTime, pstRsp))
    {
        TaskLog(LOG_ERROR, "get tx status failed");
        return;
    }

    ulRspMsgLen = STRUCT_OFFSET(OMT_GET_TXSTATUS_RSP_T, astRec) + pstRsp->ulNum*sizeof(pstRsp->astRec[0]);

    for (UINT32 i=0; i<pstRsp->ulNum; i++)
    {
        HTONL_IT(pstRsp->astRec[i].ulTime);
        HTONL_IT(pstRsp->astRec[i].stStatus.stLteStatus.ulStationID);
        HTONL_IT(pstRsp->astRec[i].stStatus.stResStatus.ulCpuUsage);
        HTONL_IT(pstRsp->astRec[i].stStatus.stResStatus.ulMemUsage);
        HTONL_IT(pstRsp->astRec[i].stStatus.stResStatus.ulDiskUsage);
        HTONL_IT(pstRsp->astRec[i].stStatus.stLteStatus.ulPCI);
        HTONL_IT(pstRsp->astRec[i].stStatus.stLteStatus.iRSRP);
        HTONL_IT(pstRsp->astRec[i].stStatus.stLteStatus.iRSRQ);
        HTONL_IT(pstRsp->astRec[i].stStatus.stLteStatus.iRSSI);
        HTONL_IT(pstRsp->astRec[i].stStatus.stLteStatus.iSINR);
        HTONL_IT(pstRsp->astRec[i].stStatus.ulRunStatus);
    }

    HTONL_IT(pstRsp->ulNum);

    SendRsp(pstRsp, ulRspMsgLen);
}

VOID TaskOmt::OnOmtGetSwInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    GJsonTupleParam         JsonTupleParam;
    VECTOR<OMC_SW_INFO_T>   vInfo;

    GetSwInfo(vInfo);
    ConvertSwInfo(vInfo, JsonTupleParam);

    SendRsp(MSG_OMT_GetSwInfo, ulSeqID, JsonTupleParam);
}

VOID TaskOmt::OnOmtAddSwInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    OMC_SW_INFO_T       stInfo;
    BOOL                bRet = FALSE;

    if (!Json.GetValue("sw_ver", stInfo.acSwVer, sizeof(stInfo.acSwVer)))
    {
        TaskLog(LOG_ERROR, "invalid sw_ver in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("sw_file", stInfo.acSwFile, sizeof(stInfo.acSwFile)))
    {
        TaskLog(LOG_ERROR, "invalid sw_file in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("sw_type", &stInfo.ulSwType))
    {
        TaskLog(LOG_ERROR, "invalid sw_type in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("sw_time", &stInfo.ulSwTime))
    {
        TaskLog(LOG_ERROR, "invalid sw_time in msg %s", szReq);
        return;
    }

    CHAR    acFile[256];
    CHAR    acFtpFile[256];

    sprintf(acFile, "%s/%s", g_stLocalCfg.acFtpDir, stInfo.acSwFile);

    if (!gos_file_exist(acFile))
    {
        TaskLog(LOG_ERROR, "add sw(%s) failed, file %s is not existed", stInfo.acSwVer, stInfo.acSwFile);
        goto end;
    }

    sprintf(acFtpFile, "%s/%s", g_stLocalCfg.acFtpVerDir, stInfo.acSwFile);
    if (!gos_rename_file(acFile, acFtpFile))
    {
        TaskLog(LOG_ERROR, "remove %s to ftp dir failed", stInfo.acSwFile);
        goto end;
    }

    bRet = m_pDao->AddSwInfo(stInfo);
    if (!bRet)
    {
        TaskLog(LOG_ERROR, "add sw(%s) to db failed", stInfo.acSwVer);
        goto end;
    }
    else
    {
        AddSwInfo(stInfo);
        TaskLog(LOG_INFO, "add sw(%s) succ", stInfo.acSwVer);
        AddOperLog("添加软件版本%s", stInfo.acSwVer);
    }

end:
    SendRsp(MSG_OMT_AddSwInfo, ulSeqID, bRet);
}

VOID TaskOmt::OnOmtDelSwInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    CHAR            *szSwVer;
    CHAR            *szSwFile;
    CHAR            acFile[256];

    szSwVer = Json.GetValue("sw_ver");
    if (!szSwVer)
    {
        TaskLog(LOG_ERROR, "invalid sw_ver in msg %s", szReq);
        return;
    }

    szSwFile = Json.GetValue("sw_file");
    if (!szSwVer)
    {
        TaskLog(LOG_ERROR, "invalid sw_file in msg %s", szReq);
        return;
    }

    BOOL bRet = m_pDao->DelSwInfo(szSwVer);

    if (!bRet)
    {
        TaskLog(LOG_ERROR, "delete sw(%s) to db failed", szSwVer);
        goto end;
    }
    else
    {
        DelSwInfo(szSwVer);
		DelSwUpdate(szSwVer);

        sprintf(acFile, "%s/%s", g_stLocalCfg.acFtpVerDir, szSwFile);
        gos_delete_file(acFile);

        TaskLog(LOG_INFO, "delete sw(%s) succ", szSwVer);
        AddOperLog("删除软件版本%s", szSwVer);
    }

end:
    SendRsp(MSG_OMT_DelSwInfo, ulSeqID, bRet);
}

VOID TaskOmt::OnOmtGetSwUpdateInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    GJsonTupleParam         JsonTupleParam;
    VECTOR<OMC_SW_UPDATE_INFO_T>    vInfo;

    if (!m_pDao->GetSwUpdateInfo(vInfo))
    {
        return;
    }

    ConvertSwUpdate(vInfo, JsonTupleParam);

    SendRsp(MSG_OMT_GetSwUpdateInfo, ulSeqID, JsonTupleParam);
}

VOID TaskOmt::OnOmtUpdateSwReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    GJson           SubJson;
    VECTOR<CHAR*>   vNeID;
    CHAR            *szNewVer;
    CHAR            *szNeID;

    szNewVer = Json.GetValue("sw_ver");
    if (!szNewVer)
    {
        TaskLog(LOG_ERROR, "invalid sw_ver in msg %s", szReq);
        return;
    }

    szNeID = Json.GetValue("ne_list");
    if (!szNeID)
    {
        TaskLog(LOG_ERROR, "invalid ne_list in msg %s", szReq);
        return;
    }

    SubJson.Parse(szNeID);

    VECTOR<GJson*>      vSubJson = SubJson.GetSubJson();

    for (UINT32 i=0; i<vSubJson.size(); i++)
    {
        szNeID = vSubJson[i]->GetValue("ne_id");
        vNeID.push_back(szNeID);
    }

    BOOL bRet = m_pDao->UpdateSw(szNewVer, vNeID);

    if (!bRet)
    {
        TaskLog(LOG_ERROR, "update sw(%s) to db failed", szNewVer);
        goto end;
    }
    else
    {
        SetSwUpdate(szNewVer, vNeID);

        for (UINT32 i=0; i<vNeID.size(); i++)
        {
            TaskLog(LOG_INFO, "update %s sw(%s) succ", vNeID[i], szNewVer);
            AddOperLog("升级设备版本%s(%s)", vNeID[i], szNewVer);
        }

    }

end:
    SendRsp(MSG_OMT_DelSwInfo, ulSeqID, bRet);
}

VOID TaskOmt::OnOmtUploadDevFileReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    GJsonParam      RspJsonParam;
    GJson           SubJson;
    VECTOR<CHAR*>   vNeID;
    CHAR            *szNeID;
    CHAR            *szFile;
    CHAR            *szRawFile;
    CHAR            acFileName[256];

    szNeID = Json.GetValue("NeID");
    if (!szNeID)
    {
        TaskLog(LOG_ERROR, "invalid NeID in msg %s", szReq);
        return;
    }

    szFile = Json.GetValue("File");
    if (!szFile)
    {
        TaskLog(LOG_ERROR, "invalid File in msg %s", szReq);
        return;
    }

    szRawFile = gos_right_strchr(szFile, '/');
    if (!szRawFile)
    {
        szRawFile = szFile;
    }
    else
    {
        szRawFile ++;
    }
    sprintf(acFileName, "%s/%s", g_stLocalCfg.acFtpLogFileDir, szRawFile);

    if (gos_file_exist(acFileName))
    {
        BOOL bRet = m_pDao->UpdateDevFileExistFlag(szNeID, szFile);
        if (!bRet)
        {
            TaskLog(LOG_ERROR, "set dev_file_exist_flag %s %s  to db failed", szNeID,szFile);
        }
    }
	else
	{
		SendSetUploadDevFileMsg(szNeID, szFile);
	}
}

VOID TaskOmt::OnOmtGetStationInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    GJsonTupleParam         JsonTupleParam;
    VECTOR<STATION_INFO_T>   vInfo;

    GetStationInfo(vInfo);
    ConvertStationInfo(vInfo, JsonTupleParam);

    SendRsp(MSG_OMT_GetStationInfo, ulSeqID, JsonTupleParam);
}

VOID TaskOmt::OnOmtAddStationInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    GJsonParam      JsonParam;
    STATION_INFO_T stInfo = {0};

    if (!Json.GetValue("StationID", &stInfo.ulStationID))
    {
        TaskLog(LOG_ERROR, "invalid  StationID in msg %s", szReq);
        return;
    }
    if (!Json.GetValue("StationName", stInfo.acStationName, sizeof(stInfo.acStationName)))
    {
        TaskLog(LOG_ERROR, "invalid StationName in msg %s", szReq);
        return;
    }
    if (!Json.GetValue("StationType", &stInfo.ulStationType))
    {
        TaskLog(LOG_ERROR, "invalid StationType in msg %s", szReq);
    }

    BOOL bRet = m_pDao->AddStationInfo(stInfo);

    if (!bRet)
    {
        TaskLog(LOG_ERROR, "add station(%u)  to db failed", stInfo.ulStationID);
    }
    else
    {
        AddStationInfo(stInfo);
        TaskLog(LOG_INFO, "add station(%u) succ", stInfo.ulStationID);
        AddOperLog("添加车站%u(%s)",stInfo.ulStationID, stInfo.acStationName);
    }

    SendRsp(MSG_OMT_AddStationInfo, ulSeqID, bRet);
}

VOID TaskOmt::OnOmtSetStationInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    GJsonParam      JsonParam;
    STATION_INFO_T stInfo = {0};

    if (!Json.GetValue("StationID", &stInfo.ulStationID))
    {
        TaskLog(LOG_ERROR, "invalid StationID in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("StationName", stInfo.acStationName, sizeof(stInfo.acStationName)))
    {
        TaskLog(LOG_ERROR, "invalid StationName in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("StationType", &stInfo.ulStationType))
    {
        TaskLog(LOG_ERROR, "invalid StationType in msg %s", szReq);
        return;
    }

    BOOL bRet = m_pDao->AddStationInfo(stInfo);
    if (!bRet)
    {
        TaskLog(LOG_ERROR, "set station(%u)  to db failed", stInfo.ulStationID);
    }
    else
    {
        SetStationInfo(stInfo);
        TaskLog(LOG_INFO, "set station(%u) succ", stInfo.ulStationID);
        AddOperLog("设置车站%u", stInfo.ulStationID);
    }

    SendRsp(MSG_OMT_SetStationInfo, ulSeqID, bRet);
}

VOID TaskOmt::OnOmtDelStationInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    UINT32 ulStationID;
    GJsonParam JsonParam;

    if (!Json.GetValue("StationID", &ulStationID))
    {
        TaskLog(LOG_ERROR, "invalid  StationID in msg %s", szReq);
        return;
    }

    BOOL bRet = m_pDao->DelStationInfo(ulStationID);

    if (!bRet)
    {
        TaskLog(LOG_ERROR, "delete station(%u) from db failed", ulStationID);
        JsonParam.Add("Result", "Fail");
    }
    else
    {
        JsonParam.Add("Result", "Succ");
        DelStationInfo(ulStationID);
        TaskLog(LOG_INFO, "delete station%u succ", ulStationID);
        AddOperLog("删除车站%u", ulStationID);
    }

    SendRsp(MSG_OMT_AddStationInfo, ulSeqID, bRet);
}

VOID TaskOmt::OnOmtGetClusterStatusReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    VECTOR<OMA_DEV_INFO_T>  vInfo;
    GJsonParam      JsonParam;
    GJsonTupleParam JsonTupleParam;

    GetDevInfo(OMC_DEV_TYPE_DIS, vInfo);
    if (vInfo.size() == 0)
    {
        return;
    }

    ConvertClusterStatusInfoToJson(vInfo, JsonTupleParam);

    SendRsp(MSG_OMT_GetClusterStatus, ulSeqID, JsonTupleParam);
}

VOID TaskOmt::OnOmtLoadOperLogInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    UINT32  ulMaxNum = 1000;
    UINT32  ulStartTime;
    UINT32  ulEndTime;
	CHAR	acOperatorID[32];
    GJsonTupleParam     JsonTupleParam;

    if (!Json.GetValue("StartTime", &ulStartTime))
    {
        TaskLog(LOG_ERROR, "invalid StartTime in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("EndTime", &ulEndTime))
    {
        TaskLog(LOG_ERROR, "invalid EndTime in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("OperatorID", acOperatorID, sizeof(acOperatorID)))
    {
        TaskLog(LOG_ERROR, "invalid UserName in msg %s", szReq);
        return;
    }

    if (!m_pDao->GetOperLogInfo(ulStartTime, ulEndTime, acOperatorID, ulMaxNum, JsonTupleParam))
    {
        TaskLog(LOG_ERROR, "load omt operation log info  failed");
        return;
    }

	SendRsp(MSG_OMT_LoadOperLogInfo, ulSeqID, JsonTupleParam);
}

VOID TaskOmt::OnOmtOmcReq(CHAR *szReq, UINT32 ulMsgLen)
{
    GJson       Json;
    GJson       SubJson;
    BOOL        bRet = Json.Parse(szReq);
    CHAR        *szMsgType;
    CHAR        *szMsgInfo;
    UINT32      ulSeqID;
    CHAR        acUserID[64];

    if (!bRet)
    {
        goto error;
    }

    // 从 omt 发送过来的 request 报文中，获取 MsgName
    szMsgType = Json.GetValue("MsgName");

    if (!szMsgType)
    {
        TaskLog(LOG_ERROR, "get MsgName failed: %s", szReq);
        goto error;
    }

    if (!Json.GetValue("MsgSeqID", &ulSeqID))
    {
        TaskLog(LOG_ERROR, "get MsgSeqID failed: %s", szReq);
        goto error;
    }

    if (!Json.GetValue("UserID", acUserID, sizeof(acUserID)))
    {
        TaskLog(LOG_ERROR, "get UserID failed: %s", szReq);
        goto error;
    }

    //SetDCLogin(ulDCUserID, GetMsgSender());

    szMsgInfo = Json.GetValue("MsgInfo");
    if (!szMsgInfo)
    {
        TaskLog(LOG_ERROR, "get MsgInfo failed: %s", szReq);
        goto error;
    }

    if (!SubJson.Parse(szMsgInfo))
    {
        TaskLog(LOG_ERROR, "parse MsgInfo failed: %s", szReq);
        goto error;
    }

    strcpy(m_acUser, acUserID);

    TaskLog(LOG_INFO, "omt msg: %s start", szReq);

    // 判断是哪一项业务
    if (strcmp(szMsgType, MSG_OMT_Login) == 0)
    {
        OnOmtLogin(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_Logout) == 0)
    {
        OnOmtLogout(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_AddNewUser) == 0)
    {
        //处理OMT添加新用户请求 ZWJ
        OnOmtAddNewUser(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_DeleteUser) == 0)
    {
        //处理OMT删除用户请求 ZWJ
        OnOmtDeleteUser(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_GetUserInfo) == 0)
    {
        //处理OMT获取用户信息请求 ZWJ
        OnOmtGetUserInfo(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_AlterUserInfo) == 0)
    {
        //处理OMT修改用户信息请求 ZWJ
        OnOmtAlterUserInfo(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_GetCfg) == 0)
    {
        OnOmtGetCfgReq(szReq, ulSeqID, SubJson); //omt获取ftp相关配置
    }
    else if (strcmp(szMsgType, MSG_OMT_AddNe) == 0)
    {
        OnOmtAddNeReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_SetNe) == 0)
    {
        OnOmtSetNeReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_DelNe) == 0)
    {
        OnOmtDelNeReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_RebootNe) == 0)
    {
        OnOmtRebootNeReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_GetNeBasicInfo) == 0)
    {
        OnOmtGetNeBasicInfoReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_GetNeInfo) == 0)
    {
        OnOmtGetNeInfoReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_GetActiveAlarm) == 0)
    {
        OnOmtGetActiveAlarmReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_ClearActiveAlarm) == 0)
    {
        OnOmtClearActiveAlarmReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_GetHistoryAlarm) == 0)
    {
        OnOmtGetHistoryAlarmReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_GetAlarmLevelCfg) == 0)
    {
        OnOmtGetAlarmLevelCfgReq(szReq, ulSeqID, SubJson);
    }
	else if (strcmp(szMsgType, MSG_OMT_AddAlarmLevelCfg) == 0)
    {
        OnOmtAddAlarmLevelCfgReq(szReq, ulSeqID, SubJson);
    }
	else if (strcmp(szMsgType, MSG_OMT_DelAlarmLevelCfg) == 0)
    {
        OnOmtDelAlarmLevelCfgReq(szReq, ulSeqID, SubJson);
    }
	else if (strcmp(szMsgType, MSG_OMT_SetAlarmLevelCfg) == 0)
    {
        OnOmtSetAlarmLevelCfgReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_GetAlarmStat) == 0)
    {
        OnOmtGetAlarmStatReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_GetAllDevStatus) == 0)
    {
        OnOmtGetAllDevStatusReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_GetDevStatus) == 0)
    {
        OnOmtGetDevStatusReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_GetLteStatus) == 0)
    {
        OnOmtGetLteStatusReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_GetTrafficStatus) == 0)
    {
        OnOmtGetTrafficStatusReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_GetDevOperLog) == 0)
    {
        OnOmtGetDevOperLogReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_GetDevFileList) == 0)
    {
        OnOmtGetDevFileListReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_GetSwInfo) == 0)
    {
        OnOmtGetSwInfoReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_AddSwInfo) == 0)
    {
        OnOmtAddSwInfoReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_DelSwInfo) == 0)
    {
        OnOmtDelSwInfoReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_GetSwUpdateInfo) == 0)
    {
        OnOmtGetSwUpdateInfoReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_UpdateSw) == 0)
    {
        OnOmtUpdateSwReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_UploadDevFile) == 0)
    {
        OnOmtUploadDevFileReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_GetStationInfo) == 0)
    {
        OnOmtGetStationInfoReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_AddStationInfo) == 0)
    {
        OnOmtAddStationInfoReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_DelStationInfo) == 0)
    {
        OnOmtDelStationInfoReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_SetStationInfo) == 0)
    {
        OnOmtSetStationInfoReq(szReq, ulSeqID, SubJson);
    }
    else if (strcmp(szMsgType, MSG_OMT_GetClusterStatus) == 0)
    {
        OnOmtGetClusterStatusReq(szReq, ulSeqID, SubJson);
    }
	else if (strcmp(szMsgType, MSG_OMT_LoadOperLogInfo) == 0)
	{
		OnOmtLoadOperLogInfoReq(szReq, ulSeqID, SubJson);
	}
    else
    {
        TaskLog(LOG_ERROR, "invalid omt msg: %s", szReq);
        goto error;
    }

    TaskLog(LOG_INFO, "omt msg: %s end", szReq);

    return;

error:
    return;
}

VOID TaskOmt::OnOmtLogin(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    GJsonParam          JsonParam;
    OMT_OPERATOR_INFO_T stUser = {0};
	CHAR                acErrInfo[256];

    if (!Json.GetValue("User", stUser.acName, sizeof(stUser.acName)))
    {
        TaskLog(LOG_ERROR, "invalid User in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("Pwd", stUser.acPassword, sizeof(stUser.acPassword)))
    {
        TaskLog(LOG_ERROR, "invalid Pwd in msg %s", szReq);
        return;
    }

    UINT32  ulOmtType = OPER_TYPE_ADMIN;
    BOOL    bRet = CheckUser(stUser.acName, stUser.acPassword, &ulOmtType, acErrInfo);

    if (!bRet)
    {
        TaskLog(LOG_ERROR, "omt %s login failed", stUser.acName);

        JsonParam.Add("Result", "Fail");
		JsonParam.Add("ErrInfo", acErrInfo);
    }
    else
    {
        PID_T       *pstClient = GetMsgSender();
        PID_T       stPrevPID;
        UINT8       aucPrevAddr[4] = {0};
        UINT8       aucCurrAddr[4] = {0};
        UINT16      usPort;

        strcpy(m_acUser, stUser.acName);

        GosGetClientAddr(pstClient->usInstID, aucCurrAddr, &usPort);

        if (GetOMTPID(stUser.acName, &stPrevPID) &&
            pstClient->usInstID != stPrevPID.usInstID)
        {
            // 登录同一个账号，踢出前面的账号
            KickOutOMT(&stPrevPID);
            if (GosGetClientAddr(stPrevPID.usInstID, aucPrevAddr, &usPort) )
            {
                TaskLog(LOG_FATAL, "omt %s logined from " IP_FMT " will be kick out", stUser.acName, IP_ARG(aucPrevAddr));
            }
        }

        JsonParam.Add("Result", "Succ");
        JsonParam.Add("OperType", ulOmtType);

        SetOmtLogin(stUser.acName, pstClient);
        AddOperLog("网管员登录");

        TaskLog(LOG_INFO, "omt %s from " IP_FMT " logined with client id %u", stUser.acName, IP_ARG(aucCurrAddr), pstClient->usInstID);
    }

    SendRsp(MSG_OMT_Login, ulSeqID, JsonParam);
}

VOID TaskOmt::OnOmtLogout(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    CHAR                acUser[32];

    if (!Json.GetValue("User", acUser, sizeof(acUser)))
    {
        TaskLog(LOG_ERROR, "invalid User in msg %s", szReq);
        return;
    }

    TaskLog(LOG_INFO, "omt %s logout", acUser);

    if (strcmp(m_acUser, acUser) == 0)
    {
        AddOperLog("网管员退出");
    }
}

//修改omt用户信息  不允许修改用户名
VOID TaskOmt::OnOmtAlterUserInfo(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    GJsonParam          JsonParam;
    CHAR                acUser[32];
    CHAR                acPwd[32];
    UINT32              ulOmtType;
    OMT_USER_INFO_T     OmtUserInfo;

    if (!Json.GetValue("User", acUser, sizeof(acUser)))
    {
        TaskLog(LOG_ERROR, "invalid User in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("Pwd", acPwd, sizeof(acPwd)))
    {
        TaskLog(LOG_ERROR, "invalid Pwd in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("UserType", &ulOmtType))
    {
        TaskLog(LOG_ERROR, "invalid UserType in msg %s", szReq);
        return;
    }

    OmtUserInfo.ulUserType = ulOmtType;
    memcpy(&OmtUserInfo.acUser ,&acUser ,sizeof(OmtUserInfo.acUser));
    memcpy(&OmtUserInfo.acPwd  ,&acPwd ,sizeof(OmtUserInfo.acPwd));

    if (!m_pDao->AlterOmtUserInfo (&OmtUserInfo))
    {
        TaskLog(LOG_ERROR,"OMT user insert pdb fail !");

        JsonParam.Add("Result", "Fail");
        JsonParam.Add("Type", ulOmtType);

        goto END ;
    }
    else
    {
        AlterUserInfo(OmtUserInfo);

        JsonParam.Add("Result", "Succ");
        JsonParam.Add("Type", ulOmtType);

        AddOperLog("修改用户%s",acUser);
        TaskLog(LOG_INFO, "OMT %s alter succ!", acUser);
    }

    TaskLog(LOG_INFO, "OMT %s alter succ!", acUser);
END:
    SendRsp(MSG_OMT_AlterUserInfo, ulSeqID, JsonParam);
    return ;
}

VOID TaskOmt::OnOmtAddNewUser(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    GJsonParam          JsonParam;
    CHAR                acUser[32];
    CHAR                acPwd[32];
    UINT32              ulOmtType;
	CHAR                acErrInfo[256];

    if (!Json.GetValue("User", acUser, sizeof(acUser)))
    {

        TaskLog(LOG_ERROR, "invalid User in msg %s", szReq);
        return;
    }

    if (!Json.GetValue("Pwd", acPwd, sizeof(acPwd)))
    {

        TaskLog(LOG_ERROR, "invalid Pwd in msg %s", szReq);
        return;
    }

    BOOL    bRet = CheckUser(acUser, acPwd, &ulOmtType, acErrInfo);//检查用户

    if (bRet)
    {
        strcpy(m_acUser, acUser);


        JsonParam.Add("Result", "Fail");
        JsonParam.Add("Type", ulOmtType);

        AddOperLog("OMT用户已经存在");

        TaskLog(LOG_INFO, "omt %s already existed", acUser);
    }
    else
    {
        OMT_USER_INFO_T     OmtUser;

        if (!Json.GetValue("Type", &ulOmtType))
        {
            TaskLog(LOG_ERROR, "invalid UserType in msg %s", szReq);
            return;
        }

        memcpy(&OmtUser.acUser, &acUser, sizeof(OmtUser.acUser));
        memcpy(&OmtUser.acPwd , &acPwd, sizeof(OmtUser.acPwd ));
        OmtUser.ulUserType = ulOmtType;

        if (!m_pDao->AddOmtUserInfo(OmtUser))
        {
            TaskLog(LOG_ERROR,"OMT user insert pdb fail !");

            JsonParam.Add("Result", "Fail");
            JsonParam.Add("Type", ulOmtType);

            goto END ;
        }

        AddUserInfo(OmtUser);

        JsonParam.Add("Result", "Succ");
        JsonParam.Add("Type", ulOmtType);

        AddOperLog("添加用户%s", acUser);

        TaskLog(LOG_INFO, "OMT %s add succ!", acUser);
    }

END:
    SendRsp(MSG_OMT_AddNewUser, ulSeqID, JsonParam);
    return ;
}

VOID TaskOmt::OnOmtDeleteUser(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    GJsonParam          JsonParam;
    CHAR                acUser[32];

    if (!Json.GetValue("User", acUser, sizeof(acUser)))
    {
        TaskLog(LOG_ERROR, "invalid User in msg %s", szReq);
        return;
    }

    if (!m_pDao->DelOmtUserInfo(acUser))
    {
        TaskLog(LOG_ERROR,"OMT user delete for  pdb fail !");

        JsonParam.Add("Result", "Fail");

        goto END ;
    }
    else
    {
        DelUserInfo(acUser);

        JsonParam.Add("Result", "Succ");

        AddOperLog("删除用户%s",acUser);

        TaskLog(LOG_INFO, "OMT %s delete succ!", acUser);
    }


    TaskLog(LOG_INFO, "OMT %s delete succ!", acUser);

END:
    SendRsp(MSG_OMT_DeleteUser, ulSeqID, JsonParam);

    return ;
}

VOID TaskOmt::OnOmtGetUserInfo(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    GJsonTupleParam             JsonTupleParam;
    VECTOR<OMT_USER_INFO_T>     vOmtInfo;

    GetUserInfo(vOmtInfo); // 此出不使用全局变量的引用是因为线程安全考虑
    ConvertOmtUserInfoToJson(vOmtInfo, JsonTupleParam);

    TaskLog(LOG_INFO, "OMT get user info  succ!");

    SendRsp(MSG_OMT_GetUserInfo, ulSeqID, JsonTupleParam);
    return ;
}

VOID TaskOmt::OnOmtGetCfgReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
    GJsonParam          JsonParam;

    JsonParam.Add("ftp_port", g_stLocalCfg.usFtpPort);
    JsonParam.Add("ftp_ver_dir", g_stLocalCfg.acFtpVerDir+strlen(g_stLocalCfg.acFtpDir)+1);
    JsonParam.Add("ftp_log_dir", g_stLocalCfg.acFtpLogFileDir+strlen(g_stLocalCfg.acFtpDir)+1);
    JsonParam.Add("ftp_user", g_stLocalCfg.acFtpUser);
    JsonParam.Add("ftp_pwd", g_stLocalCfg.acFtpPwd);
    JsonParam.Add("transfile_port", g_stLocalCfg.usTransFilePort);
    JsonParam.AddIP("NTPAddr", g_stLocalCfg.aucNTPAddr);
    JsonParam.Add("NTPPort", g_stLocalCfg.usNTPPort);
    JsonParam.Add("NTPSyncPeriod", g_stLocalCfg.ulNTPSyncPeriod);
    JsonParam.Add("OMCVersion", g_szVersion);
    SendRsp(MSG_OMT_GetCfg, ulSeqID, JsonParam);
}

VOID TaskOmt::OnCheckTimer()
{
    m_pDao->CheckConn();
}

VOID TaskOmt::TaskEntry(UINT16 usMsgID, VOID *pvMsg, UINT32 ulMsgLen)
{
    UINT32  ulTaskStatus = GetTaskStatus();

    switch(ulTaskStatus)
    {
        case TASK_STATUS_IDLE:
            switch(usMsgID)
            {
                case EV_TASK_INIT_REQ:
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
                        TaskLog(LOG_FATAL, "Init failed!");
                        gos_sleep(1);
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

                case EV_OMT_OMC_REQ:
                    OnOmtOmcReq((CHAR*)pvMsg, ulMsgLen);
                    break;

    /*          case EV_OMT_GET_NEINFO_REQ:
                    OnOmtGetNeInfoReq((CHAR*)pvMsg, ulMsgLen);
                    break;*/

                case EV_OMT_GET_TXSTATUS_REQ:
                    OnOmtGetTxStatusReq((CHAR*)pvMsg, ulMsgLen);
                    break;

                case EV_OMT_GET_ALLTXSTATUS_REQ:
                    OnOmtGetAllTxStatusReq((CHAR*)pvMsg, ulMsgLen);
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
