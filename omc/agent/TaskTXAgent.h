#ifndef TASK_TX_AGENT_H
#define TASK_TX_AGENT_H

class TaskTXAgent: public GBaseTask
{
public :
    TaskTXAgent(UINT16 usDispatchID);

    BOOL Init();
    virtual VOID TaskEntry(UINT16 usMsgID, VOID *pvMsg, UINT32 ulMsgLen);

private:
    CHAR        m_acMsg[64*1024];
    OmcDao      *m_pDao;

    BOOL        m_bGetNEBasicInfo;
    //UINT32        m_ulDCUserID;

    virtual VOID OnServerDisconnectClient(UINT16 usClientID);

    VOID OnCheckTimer();

    // OMA
    VOID OnOmaReq(CHAR *szMsg, UINT32 ulMsgLen);

    VOID OnOmaOnlineReq(CHAR *szMsgInfo, GJson &Json);

};

#endif
