#ifndef TASK_OMT_H
#define TASK_OMT_H

class TaskOmt: public GBaseTask
{
public :
    TaskOmt(UINT16 usDispatchID);

    BOOL Init();
    virtual VOID TaskEntry(UINT16 usMsgID, VOID *pvMsg, UINT32 ulMsgLen);

private:
    BOOL        m_bGetUserInfo;
    BOOL        m_bGetAlarmLevelCfg;
    BOOL        m_bGetActiveAlarmInfo;
    BOOL        m_bGetSwInfo;
    BOOL        m_bGetSwUpdate;
    BOOL        m_bGetStationInfo;
    CHAR        m_acUser[64];
    FILE        *m_fp;
    CHAR        *m_szNeBasicFile;
    OmcDao      *m_pDao;

    virtual VOID OnServerDisconnectClient(UINT16 usClientID);

    VOID AddOperLog(const CHAR *szOperLog, ...);

    VOID OnGetClientVer(VOID *pvMsg, UINT32 ulMsgLen);
    VOID OnCheckTimer();

    VOID OnOmtOmcReq(CHAR *szReq, UINT32 ulMsgLen);

    VOID SendConfirmSaveRsp(UINT32 ulSeqID);

    VOID OnOmtLogin(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtLogout(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID KickOutOMT(PID_T *pstPID);
    /*************************OMT鐢ㄦ埛澧炲垹鏀规煡ZWJ***********************************/
    VOID OnOmtAlterUserInfo(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtGetUserInfo(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtDeleteUser(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtAddNewUser(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    /*************************OMT鐢ㄦ埛澧炲垹鏀规煡END***********************************/
    VOID OnOmtGetCfgReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);

    VOID OnOmtGetNeBasicInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtGetNeInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtGetClusterStatusReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);

    VOID OnOmtAddNeReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtSetNeReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtDelNeReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtRebootNeReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);

    VOID OnOmtGetActiveAlarmReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtClearActiveAlarmReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtGetHistoryAlarmReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtGetAlarmStatReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);

    VOID OnOmtGetAlarmLevelCfgReq(CHAR *szReq, UINT32 ulSeqID,GJson &Json);		// 获取告警等级配置
	VOID OnOmtAddAlarmLevelCfgReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);	// 添加告警等级配置
	VOID OnOmtDelAlarmLevelCfgReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);	// 删除告警等级配置
    VOID OnOmtSetAlarmLevelCfgReq(CHAR *szReq, UINT32 ulSeqID,GJson &Json);		// 修改告警等级配置

    VOID OnOmtGetAllDevStatusReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtGetDevStatusReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtGetStatusReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtGetLteStatusReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtGetTrafficStatusReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);

    VOID OnOmtGetDevOperLogReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtGetDevFileListReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);

    //VOID OnOmtGetNeInfoReq(CHAR *szReq, UINT32 ulMsgLen);
    VOID OnOmtGetTxStatusReq(CHAR *szReq, UINT32 ulMsgLen);
    VOID OnOmtGetAllTxStatusReq(CHAR *szReq, UINT32 ulMsgLen);

    VOID OnOmtGetSwInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtAddSwInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtDelSwInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);

    VOID OnOmtGetSwUpdateInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtUpdateSwReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);

    VOID OnOmtUploadDevFileReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);

    VOID OnOmtGetStationInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtAddStationInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtDelStationInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
    VOID OnOmtSetStationInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);

	VOID OnOmtLoadOperLogInfoReq(CHAR *szReq, UINT32 ulSeqID, GJson &Json);
};

#endif
