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

    VOID OnCheckTimer();   //��鶨ʱ��
    VOID CheckDev(OMA_DEV_INFO_T &stInfo); // ����豸
    BOOL CreateOfflineAlarm(OMA_DEV_INFO_T &stInfo);

    virtual VOID OnClientDisconnectServer(UINT16 usServerID); // �ͻ��˷��ַ�����
};


#endif
