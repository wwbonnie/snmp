#ifndef TASK_DB_SYNC_H
#define TASK_DB_SYNC_H

#include "omc_public.h"
#include "GTransFile.h"


class TaskDBSync: public GBaseTask
{
public :
    TaskDBSync(UINT16 usDispatchID);

    BOOL Init();
    BOOL InitCheckTimer();
    virtual VOID TaskEntry(UINT16 usMsgID, VOID *pvMsg, UINT32 ulMsgLen);

private:
    GMutex      *m_Mutex;
    OmcDao      *m_pDao;
    OmcDao      *m_pPeerDao;
    FILE        *m_fpRaise;
    FILE        *m_fpClear;
    CHAR        m_acAlarmRaiseFile[128];
    CHAR        m_acAlarmClearFile[128];
    VOID OnTimerCheck(VOID *pvMsg, UINT32 ulMsgLen);

    BOOL InitPeerDBEnv();
    BOOL InitPeerDBConn();

    VOID BackupTable();
    VOID ClearTable();
    VOID ClearNeBasicStatusTable();
    VOID ClearTrafficStatusTable();

    VOID SyncOMTUser();
    VOID SyncOperLog();
    VOID SyncStation();
    VOID SyncActiveAlarm();
    VOID SyncAlarmLevel();
    VOID SyncNEBasicInfo();
    VOID SyncHistoryAlarm();
	VOID SyncHistoryAlarmStatistic();
    VOID SyncNEStatus();
    VOID SyncDevOperLog();
    VOID SyncSwInfo();
    VOID SyncUpdateSwInfo();
    VOID SyncDevFile();
    VOID SyncDevFileName();

    VOID SyncAlarmRaise();
    VOID SyncAlarmClear();

};
#endif
