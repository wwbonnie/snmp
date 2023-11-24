#ifndef TASK_SYS_H
#define TASK_SYS_H

#define TIMER_DATA_CLEAR                    TIMER13
#define EV_DATA_CLEAR_IND                   EV_TIMER13

#define TIMER_SET_STATUS                    TIMER14
#define EV_SET_STATUS_IND                   EV_TIMER14

class TaskSys: public GBaseTask
{
public :
    TaskSys(UINT16 usDispatchID);
    BOOL InitDataClearCfg();
    BOOL Init();
    virtual VOID TaskEntry(UINT16 usMsgID, VOID *pvMsg, UINT32 ulMsgLen);

private:
    GMutex      *m_Mutex;

    VOID OnCheckTimer();
    VOID OnGetDataClearTimerInd(VOID *pvMsg, UINT32 ulMsgLen);
    BOOL ClearTable(const CHAR *szTableName, const CHAR *szColumnName, PDBApp *pDcco);

    VOID OnGetSysResetReq(VOID *pvMsg, UINT32 ulMsgLen);
    VOID NTPSync();
};

#endif
