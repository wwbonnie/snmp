#ifndef TASK_QUERY_H
#define TASK_QUERY_H

class TaskQuery: public GBaseTask
{
public :
    TaskQuery(UINT16 usDispatchID);

    virtual VOID TaskEntry(UINT16 usMsgID, VOID *pvMsg, UINT32 ulMsgLen);

private:
    OmcDao      *m_pDao;
    BOOL Init();

    VOID OnCheckTimer();   //检查定时器
    VOID CheckDev(OMA_DEV_INFO_T &stInfo); // 检查设备
    BOOL CreateOfflineAlarm(OMA_DEV_INFO_T &stInfo);

    virtual VOID OnClientDisconnectServer(UINT16 usServerID); // 客户端发现服务器
};


#endif
