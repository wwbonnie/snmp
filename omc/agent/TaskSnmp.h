#ifndef TASK_SNMP_H
#define TASK_SNMP_H

class TaskSnmp: public GBaseTask
{
public :
    TaskSnmp(UINT16 usDispatchID);

    virtual VOID TaskEntry(UINT16 usMsgID, VOID *pvMsg, UINT32 ulMsgLen);

private:
    BOOL        m_bInitDevInfo;
    OmcDao      *m_pDao;

    FILE        *m_fpRaise;
    FILE        *m_fpClear;
    CHAR        *m_szRiseTmpFile;
    CHAR        *m_szRiseFile;
    CHAR        *m_szClearFile;
    CHAR        *m_szClearTmpFile;
    UINT32      m_ulLastAlarmRaiseUpdateTime;
    UINT32      m_ulLastAlarmClearUpdateTime;
    VECTOR<ALARM_INFO_T>      m_vRaiseAlarm;
    VECTOR<ALARM_INFO_T>      m_vClearAlarm;
    SOCKET      m_stTrapSocket;
	std::map<std::string, BOOL>  m_mOnline;
    UINT32		m_ulLastSaveTime;
    BOOL Init();

    VOID ListenSnmp();
    BOOL ListenSnmp(UINT16 usSnmpPort, UINT16 usTrapPort);
    VOID SaveDevInfo();
	VOID SaveTXInfo(UINT32 ulCurrTime);
	VOID SaveFXInfo(UINT32 ulCurrTime);
	VOID SaveTAUInfo(UINT32 ulCurrTime);
	VOID SaveDCInfo(UINT32 ulCurrTime);
	VOID SaveDISInfo(UINT32 ulCurrTime);
	VOID SaveRECInfo(UINT32 ulCurrTime);

    // snmp msg
    VOID HandleSnmpMsg(SNMP_GET_RSP_T &stSnmpGetRsp);

    VOID OnGetSysInfoRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData);
    VOID OnGetDISStatusRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData);
    VOID OnGetDCStatusRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData);

    VOID OnGetTXStatusRsp(OMA_DEV_INFO_T &stNEBasicInfo, VECTOR<SNMP_DATA_T> &vData);
    VOID OnGetFXStatusRsp(OMA_DEV_INFO_T &stNEBasicInfo, VECTOR<SNMP_DATA_T> &vData);
    VOID OnGetLteStatusRsp(OMA_DEV_INFO_T &stNEBasicInfo, VECTOR<SNMP_DATA_T> &vData);
    VOID OnGetTAULteStatusRsp(OMA_DEV_INFO_T &stNEBasicInfo, VECTOR<SNMP_DATA_T> &vData);
    VOID OnGetTrafficRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData);

    VOID OnGetOperLogIndexRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData);
    VOID OnGetOperLogRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData);

    VOID OnGetDevFileIndexRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData);
    VOID OnGetDevFileRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData);

    VOID OnGetDevNetInfoRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData);

    VOID OnGetDevSwUpdateRsp(OMA_DEV_INFO_T &stDevInfo, VECTOR<SNMP_DATA_T> &vData);

    // trap smg
    VOID HandleTrapMsg(IP_ADDR_T *pstClientAddr, VOID *pPDU);

    BOOL CreateOfflineAlarm (OMA_DEV_INFO_T &stDevInfo);
    VOID OnAlarmTrap(ALARM_INFO_T &stInfo, UINT32 ulDevType);
    VOID OnOnlineTrap(IP_ADDR_T *pstClientAddr, NE_BASIC_INFO_T &stInfo);
    VOID OnHeartBeatTrap(IP_ADDR_T *pstClientAddr, CHAR *szNeID);
	VOID CheckAlarmCfg(ALARM_INFO_T &stInfo, UINT32 ulDevType, BOOL &bIsIgnore);
    virtual VOID OnClientDisconnectServer(UINT16 usServerID);

    /// 把缓存的活跃告警从临时文件到读取文件
    VOID StoreAlarmRaiseData();
    VOID StoreAlarmClearData();
};


#endif
