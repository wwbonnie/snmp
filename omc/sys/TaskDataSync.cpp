#include "omc_public.h"
#include "TaskDataSync.h"
#include "TaskSnmp.h"

#define CHECK_TIMER         TIMER10
#define EV_CHECK_TIMER      GET_TIMER_EV(CHECK_TIMER)

extern CHAR     *g_szAlarmRaiseFile;
extern CHAR     *g_szAlarmClearFile;

UINT32             g_ulCheckPerid = 1;
TaskDataSync::TaskDataSync(UINT16 usDispatchID):GBaseTask(MODULE_FILESYNC, (CHAR*)"DataSync", usDispatchID)
{
    m_ulLastUpdateTime =0;
    m_pDao = NULL;
    m_fpRaise = NULL;
    m_fpClear = NULL;
}

BOOL TaskDataSync::Init()
{
    CHAR        acRaiseFile[128] = {0};
    CHAR        acClearFile[128] = {0};

   sprintf(acRaiseFile, "%s/%s", gos_get_root_path(), g_szAlarmRaiseFile);
   sprintf(acClearFile, "%s/%s", gos_get_root_path(), g_szAlarmClearFile);

    gos_format_path(acRaiseFile);
    gos_format_path(acClearFile);

    strcpy(m_acAlarmRaiseFile, acRaiseFile);
    strcpy(m_acAlarmClearFile, acClearFile);

    if (!m_pDao)
    {
        m_pDao = GetOmcDao();
        if (!m_pDao)
        {
            TaskLog(LOG_ERROR, "get db dao failed");

            return FALSE;
        }
    }

    SetLoopTimer(CHECK_TIMER, g_ulCheckPerid);
    SetTaskStatus(TASK_STATUS_WORK);

    TaskLog(LOG_INFO, "TaskDataSync init successful!");

    return TRUE;
}

VOID TaskDataSync::SynAlarmRaise()
{
    VECTOR<ALARM_INFO_T>    vInfo;
    ALARM_INFO_T            stInfo ={0}; 
    INT32                   iSize = 0;

    if (!gos_file_exist(m_acAlarmRaiseFile))
    {
        return;
    }

    m_fpRaise = fopen(m_acAlarmRaiseFile, "r+b");
    if (NULL == m_fpRaise)
    {
       TaskLog(LOG_ERROR, "SyncAlarmRaise: open %s fail", m_acAlarmRaiseFile); 
       return;
    }

    fseek(m_fpRaise, 0, SEEK_END);
    iSize = ftell(m_fpRaise);
    iSize = iSize / sizeof(ALARM_INFO_T);
    fseek(m_fpRaise, 0, SEEK_SET);

    for (UINT32 i=0; i < iSize; i++)
    {
        fread(&stInfo, sizeof(ALARM_INFO_T), 1, m_fpRaise);
        vInfo.push_back(stInfo);
    }

    fclose(m_fpRaise);
    TaskLog(LOG_INFO, "start insert raise_alarm ");
    if (m_pDao->AddActiveAlarm(vInfo))
    {
        TaskLog(LOG_INFO, "Read %d raise_alarm succc", vInfo.size());
        gos_delete_file(m_acAlarmRaiseFile);
    }
    else
    {
        TaskLog(LOG_ERROR, "Read raise_alarm fail");
    } 
}

VOID TaskDataSync::SyncAlarmClear()
{
    VECTOR<ALARM_INFO_T>    vInfo;
    ALARM_INFO_T            stInfo ={0}; 
    INT32                   iSize = 0;

    if (!gos_file_exist(m_acAlarmClearFile))
    {
        return;
    }

    m_fpClear = fopen(m_acAlarmClearFile, "r+b");
    if (NULL == m_fpClear)
    {
       TaskLog(LOG_ERROR, "SyncAlarmClear: open %s fail", m_acAlarmClearFile); 
       return;
    }
    fseek(m_fpClear, 0, SEEK_END);
    iSize = ftell(m_fpClear);
    iSize = iSize / sizeof(ALARM_INFO_T);
    fseek(m_fpClear, 0, SEEK_SET);

    for (UINT32 i=0; i < iSize; i++)
    {
        fread(&stInfo, sizeof(ALARM_INFO_T), 1, m_fpClear);
        vInfo.push_back(stInfo);
    }
    
    fclose(m_fpClear);
    TaskLog(LOG_INFO, "start insert clear_alarm ");
             
 
    if (m_pDao->ClearActiveAlarm(vInfo, (CHAR *)ALARM_CLEAR_AUTO))
    {
        TaskLog(LOG_INFO, "Read %d clear_alarm succc", vInfo.size());
        gos_delete_file(m_acAlarmClearFile);
    }
    else
    {
        TaskLog(LOG_ERROR, "Read clear_alarm fail");
    } 
}

VOID TaskDataSync::OnCheckTimer()
{
   SynAlarmRaise();
   SyncAlarmClear();
}

VOID TaskDataSync::TaskEntry(UINT16 usMsgID, VOID *pvMsg, UINT32 ulMsgLen)
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

        default:
            TaskLog(LOG_ERROR, "unknown msg %u", usMsgID);
            break;
        }

        break;

    default:
        break;
    }
}