#include "omc_public.h"
#include "TaskTXAgent.h"

#define CHECK_TIMER         TIMER10
#define EV_CHECK_TIMER      GET_TIMER_EV(CHECK_TIMER)

extern APP_BASIC_CFG_T      g_stAPPBasicCfg;

TaskTXAgent::TaskTXAgent(UINT16 usDispatchID):GBaseTask(MODULE_TX_AGENT, "TXAgt", usDispatchID)
{
    m_pDao = NULL;
    m_bGetNEBasicInfo = FALSE;
    //m_ulDCUserID = INVALID_USER_ID;

    RegisterMsg(EV_OMA_OMC_REQ);
}

BOOL TaskTXAgent::Init()
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

    SetLoopTimer(CHECK_TIMER, 3*1000);

    SetTaskStatus(TASK_STATUS_WORK);

    TaskLog(LOG_INFO, "Task init successful!");

    return TRUE;
}

VOID TaskTXAgent::OnServerDisconnectClient(UINT16 usClientID)
{

}

VOID TaskTXAgent::OnOmaReq(CHAR *szReq, UINT32 ulMsgLen)
{
    GJson       Json;
    GJson       SubJson;
    BOOL        bRet;
    CHAR        *szMsgType;
    CHAR        *szMsgInfo;
    UINT32      ulSeqID;

    bRet = Json.Parse(szReq);
    if (!bRet)
    {
        goto error;
    }

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

    TaskLog(LOG_DETAIL, "recv OMA msg: %s", szReq);

    if (strcmp(szMsgType, MSG_OMA_OnlineReq) == 0)
    {
        OnOmaOnlineReq(szMsgInfo, SubJson);
    }
    else
    {
        TaskLog(LOG_ERROR, "invalid SA msg: %s", szReq);
        goto error;
    }

    return;

error:
    return;
}

VOID TaskTXAgent::OnOmaOnlineReq(CHAR *szMsgInfo, GJson &Json)
{
    NE_BASIC_INFO_T     stInfo = {0};
    UINT8               aucDevMac[6];
    UINT8               aucDevIp[4];
    UINT32              ulCurrUptime = gos_get_uptime();

    memset(&stInfo, 0, sizeof(stInfo));

    // 获取MAC地址、固件版本
    if (!Json.GetMAC("mac", aucDevMac) ||  // tool-dev_id
        !Json.GetIP("ip", aucDevIp) ||  // cfg_app_ip
        !Json.GetValue("type", &stInfo.ulDevType) ||  // 5678
//        !Json.GetValue("name", stInfo.acDevName, sizeof(stInfo.acDevName)) ||
        !Json.GetValue("sw_ver", stInfo.acSoftwareVer, sizeof(stInfo.acSoftwareVer)) || // APP_STATUS_AppVersion
        !Json.GetValue("hw_ver", stInfo.acHardwareVer, sizeof(stInfo.acHardwareVer)) || // running——ver
        !Json.GetValue("model", stInfo.acModelName, sizeof(stInfo.acModelName)) )       // H2000-TX
    {
        TaskLog(LOG_ERROR, "invalid msg format: %s", szMsgInfo);
        return;
    }

    sprintf(stInfo.acNeID, "%02X%02X%02X%02X%02X%02X", GOS_MAC_ARG(aucDevMac));

    sprintf(stInfo.acDevMac, GOS_MAC_FMT, GOS_MAC_ARG(aucDevMac));
    sprintf(stInfo.acDevIp, IP_FMT, IP_ARG(aucDevIp));

    OMA_DEV_INFO_T      stDevInfo;
    BOOL                bSave = TRUE;

    memset(&stDevInfo, 0, sizeof(stDevInfo));

    if (!GetDevInfo(stInfo.acNeID, stDevInfo))
    {
//      stInfo.ulOnlineTime = gos_get_current_time();
        stInfo.ulFirstOnlineTime = ulCurrUptime;
        stInfo.ulLastOnlineTime = stInfo.ulFirstOnlineTime;

        memcpy(&stDevInfo.stNeBasicInfo, &stInfo, sizeof(stInfo));
        SetDevInfo(stDevInfo);
        TaskLog(LOG_INFO, "dev %s(%s) is online", stInfo.acDevName, stInfo.acDevMac);
    }
    else
    {
    //  stInfo.ulOnlineTime = stDevInfo.stNeBasicInfo.ulOnlineTime;
        stInfo.ulFirstOnlineTime = stDevInfo.stNeBasicInfo.ulFirstOnlineTime;

        stInfo.ulLastOnlineTime = ulCurrUptime;
        stDevInfo.stNeBasicInfo.ulLastOnlineTime = stInfo.ulLastOnlineTime;

        if (memcmp(&stDevInfo.stNeBasicInfo, &stInfo, sizeof(stInfo)) == 0)
        {
            bSave = FALSE;
        }

        memcpy(&stDevInfo.stNeBasicInfo, &stInfo, sizeof(stInfo));
        SetDevInfo(stDevInfo);
    }

    if (bSave)
    {
        if (!m_pDao->AddNeBasicInfo(stDevInfo.stNeBasicInfo))
        {
            TaskLog(LOG_ERROR, "set dev(%s) basic info failed", stInfo.acDevName);
        }
    }
}

/*
VOID TaskTXAgent::OnSAGetNTPCfgReq(CHAR *szMsgInfo, GJson &Json)
{
    UINT32      ulTrainUnitID;

    if (!Json.GetValue("TrainUnitID", &ulTrainUnitID) )
    {
        TaskLog(LOG_ERROR, "invalid TrainUnitID in MsgInfo: %s", szMsgInfo);
        return;
    }

    GJsonParam      Param;

    Param.Add("NTPServerAddr", g_stAPPBasicCfg.acNTPServerAddr);
    Param.Add("NTPServerPort", g_stAPPBasicCfg.ulNTPServerPort);
    Param.Add("NTPSyncPeriod", g_stAPPBasicCfg.ulNTPSyncPeriod);

    SendTrainSA(MSG_SAGetNTPCfg, ulTrainUnitID, Param);
}

VOID TaskTXAgent::OnSAGetCfgReq(CHAR *szMsgInfo, GJson &Json)
{
    GJsonParam      Param;

    Param.Add("ResetButtonCheckTime", g_ulRadioResetButtonCheckTime);
    Param.Add("ResetAPPCheckTime", g_ulRadioResetAPPCheckTime);
    Param.Add("NTPServerAddr", g_stAPPBasicCfg.acNTPServerAddr);
    Param.Add("NTPServerPort", g_stAPPBasicCfg.ulNTPServerPort);
    Param.Add("NTPSyncPeriod", g_stAPPBasicCfg.ulNTPSyncPeriod);

    SendRsp(MSG_SAGetCfg, 0, Param);
}
*/

/*
VOID TaskTXAgent::OnDCAcceptIPHTalkReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json)
{
  VECTOR<GJson*>    &vSubJson = Json.GetSubJson();
    UINT32          ulTrainUnitID = 0xffffffff;
    UINT32          ulCarriageID;
    UINT32          ulIPHDevID;
    IPH_TALK_INFO_T *pstInfo;
    BOOL            bRet = FALSE;

    if (vSubJson.size() == 0)
    {
        return;
    }

    for (UINT32 i=0; i<vSubJson.size(); i++)
    {
        GJson   *SubJson = vSubJson[i];

        if (!SubJson->GetValue("TrainUnitID", &ulTrainUnitID))
        {
            TaskLog(LOG_ERROR, "get TrainUnitID of MsgInfo failed: %s", szReq);
            return;
        }

        if (!SubJson->GetValue("CarriageID", &ulCarriageID))
        {
            TaskLog(LOG_ERROR, "get CarriageID of MsgInfo failed: %s", szReq);
            return;
        }

        if (!SubJson->GetValue("IPHDevID", &ulIPHDevID))
        {
            TaskLog(LOG_ERROR, "get IPHDevID of MsgInfo failed: %s", szReq);
            return;
        }

        if (i == 0)
        {
            bRet = SendTrainSA(ulTrainUnitID, szReq);
            if (!bRet)
            {
                break;
            }
        }

        pstInfo = AcceptIPHTalk(ulTrainUnitID, ulCarriageID, ulIPHDevID, m_ulDCUserID);
        if (pstInfo)
        {
            m_pDao->UpdateIPHTalk(*pstInfo);
        }
    }

    GJsonParam  JsonParam;

    JsonParam.Add("Result", bRet?"Succ":"Fail");

    SendRsp(MSG_DCAcceptIPHTalk, ulSeqID, JsonParam);


}
    */
VOID TaskTXAgent::OnCheckTimer()
{
}

VOID TaskTXAgent::TaskEntry(UINT16 usMsgID, VOID *pvMsg, UINT32 ulMsgLen)
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
                case EV_TASK_CHECK_REQ:
                    SendRsp();
                    break;

                case EV_CHECK_TIMER:
                    OnCheckTimer();
                    break;

                case EV_OMA_OMC_REQ:
                    OnOmaReq((CHAR*)pvMsg, ulMsgLen);
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
