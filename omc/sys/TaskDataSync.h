#ifndef TASK_DATA_SYNC_H
#define TASK_DATA_SYNC_H


class TaskDataSync:public GBaseTask
{
public:
    TaskDataSync(UINT16 usDispatchID);

    BOOL Init();
    //BOOL InitCheckTimer();
    virtual VOID TaskEntry(UINT16 usMsgID, VOID *pvMsg, UINT32 ulMsgLen);
private:
    UINT32      m_ulLastUpdateTime;
    OmcDao      *m_pDao;
    FILE        *m_fpRaise;
    FILE        *m_fpClear;
    CHAR        m_acAlarmRaiseFile[128];
    CHAR        m_acAlarmClearFile[128];

    VOID OnCheckTimer();   //¼ì²é¶¨Ê±Æ÷
    VOID SynAlarmRaise();
    VOID SyncAlarmClear();

};

#endif