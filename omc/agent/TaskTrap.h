#ifndef TASK_TRAP_H
#define TASK_TRAP_H

class TaskTrap: public GBaseTask
{
public :
    TaskTrap(UINT16 usDispatchID);

    virtual VOID TaskEntry(UINT16 usMsgID, VOID *pvMsg, UINT32 ulMsgLen);

private:
    BOOL        m_bGetCfg;
    OmcDao      *m_pDao;

    BOOL Init();

    VOID ListenTrap();
    BOOL ListenTrap(UINT16 usTrapPort);
    VOID HandleTrapMsg(IP_ADDR_T *pstClientAddr, VOID *pPDU);
    VOID OnAlarmTrap(ALARM_INFO_T &stAlarm);
    VOID OnOnlineTrap(IP_ADDR_T *pstClientAddr, NE_BASIC_INFO_T &stInfo);
    VOID OnHeartBeatTrap(CHAR *szNeID);

    virtual VOID OnClientDisconnectServer(UINT16 usServerID);
};

VOID SetTrapCfg(UINT16 usTrapPort);

#endif
