#include "omc_public.h"
#include "TaskSys.h"

#define CHECK_TIMER         TIMER10
#define EV_CHECK_TIMER      GET_TIMER_EV(CHECK_TIMER)

DATA_CLEAR_CFG_T        g_stDataClearCfg = {0};

extern CHAR             *g_szConfigFile;
extern OMC_LOCAL_CFG_T  g_stLocalCfg;

TaskSys::TaskSys(UINT16 usDispatchID):GBaseTask(MODULE_SYS, (CHAR*)"SYS", usDispatchID)
{
    RegisterMsg(EV_DATA_CLEAR_IND);
    RegisterMsg(EV_HARDWARE_RESET_REQ);
}

BOOL TaskSys::Init()
{
    if (gos_is_local_ip(g_stLocalCfg.aucNTPAddr))
    {
        SET_IP(g_stLocalCfg.aucNTPAddr, 127, 0, 0, 1);
    }

    NTPSync();

    SetLoopTimer(CHECK_TIMER, 10000);
    SetTaskStatus(TASK_STATUS_WORK);

    TaskLog(LOG_INFO, "Task sys init successful!");

    return TRUE;
}

VOID TaskSys::OnGetDataClearTimerInd(VOID *pvMsg, UINT32 ulMsgLen)
{
}

VOID TaskSys::OnGetSysResetReq(VOID *pvMsg, UINT32 ulMsgLen)
{
    CHAR                        acProcName[256];
    GET_SYS_RESET_RSP           stRsp;

    if (gos_get_proc_name(acProcName, FALSE))
    {
        stRsp.bRet = TRUE;
    }
    else
    {
        stRsp.bRet = FALSE;
    }

    if (strlen(gos_get_root_path()) < sizeof(stRsp.acWorkDir))
    {
        strcpy(stRsp.acWorkDir, gos_get_root_path());
    }

    strcpy(stRsp.acProcName, acProcName);

    SendRsp(&stRsp, sizeof(stRsp));

    gos_sleep(1);

    exit(0);
}

VOID TaskSys::NTPSync()
{
    static UINT32   m_ulLastNTPSyncTime = 0;
    UINT32          ulCurrTime = gos_get_uptime();
    UINT8           aucLocalAddr[4] = {127,0,0,1};

    if (g_stLocalCfg.ulNTPSyncPeriod == 0)
    {
        return;
    }

    if (GET_INT(g_stLocalCfg.aucNTPAddr) == GET_INT(aucLocalAddr) ||
        gos_is_local_ip(g_stLocalCfg.aucNTPAddr) )
    {
        return;
    }

    if (m_ulLastNTPSyncTime > 0 && (ulCurrTime-m_ulLastNTPSyncTime) < g_stLocalCfg.ulNTPSyncPeriod)
    {
        return;
    }

    INT32   iTime = gos_get_ntp_time(g_stLocalCfg.aucNTPAddr, g_stLocalCfg.usNTPPort);

    if (iTime < 0)
    {
        TaskLog(LOG_INFO, "connect to NTP server " IP_FMT " failed!", IP_ARG(g_stLocalCfg.aucNTPAddr));
        return;
    }

    INT32   iTimeDiff = abs((INT32)(iTime - gos_get_current_time()));

    if (iTimeDiff < 10)
    {
        return;
    }

    TaskLog(LOG_INFO, "the difference between local time and ntp server time is %u", iTimeDiff);

    if (m_ulLastNTPSyncTime == 0)
    {
        TaskLog(LOG_INFO, "sync time with NTP server");
    }

    m_ulLastNTPSyncTime = ulCurrTime;
    if (!gos_set_localtime(iTime))
    {
        TaskLog(LOG_ERROR, "NTP set localtime failed!");

        return;
    }

    TaskLog(LOG_INFO, "sync time with NTP server succ");
}

VOID TaskSys::OnCheckTimer()
{
    NTPSync();
}

VOID TaskSys::TaskEntry(UINT16 usMsgID, VOID *pvMsg, UINT32 ulMsgLen)
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

                case EV_DATA_CLEAR_IND:
                    OnGetDataClearTimerInd(pvMsg, ulMsgLen);
                    break;

                case EV_SYS_RESET_REQ:
                    OnGetSysResetReq(pvMsg, ulMsgLen);
                    break;

                case EV_HARDWARE_RESET_REQ:
                    gos_reboot();
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

