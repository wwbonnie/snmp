#ifndef OMC_DAO_H
#define OMC_DAO_H

#define SYNC_ADD        1
#define SYNC_DEL        (-1)
#define SYNC_NULL       0

#define DaoLog(ulLogLevel, szFormat, ...)      { GosLog("DAO", ulLogLevel, szFormat, ##__VA_ARGS__); }


class OmcDao
{
public :
    OmcDao();
    ~OmcDao();

    BOOL TransBegin();
    BOOL TransRollback();
    BOOL TransCommit();

    BOOL Init(PDBEnvBase *pPDBEnv);

    BOOL CheckConn();

    BOOL Update(const CHAR *szSQL);

    BOOL AddOperLog(UINT32 ulTime, CHAR *szUserName, CHAR *szLogInfo);
    BOOL GetSyncingOperLog(UINT32 ulMaxRecNum, VECTOR<OMT_OPER_LOG_INFO_T> &vAddRec, VECTOR<OMT_OPER_LOG_INFO_T> &vDelRec);
    BOOL SetOperLog(OMT_OPER_LOG_INFO_T &stRec, INT32 iSyncFlag);
    BOOL SyncOverOperLog(OMT_OPER_LOG_INFO_T &stRec, INT32 iSyncFlag);

    INT32 GetRecNum(CHAR *szTable);
    BOOL ClearTable(CHAR *szTable, CHAR *szFieldName, UINT32 ulMaxResvNum);


    BOOL AddOmtUserInfo(OMT_USER_INFO_T &stInfo);
    BOOL DelOmtUserInfo(CHAR *pstInfo);
    BOOL AlterOmtUserInfo(OMT_USER_INFO_T *pstInfo);
    BOOL GetUserInfo(VECTOR<OMT_USER_INFO_T> &vInfo);
    BOOL SetOMTUser(OMT_USER_INFO_T &stInfo, INT32 iSyncFlag);
    BOOL GetSyncingOMTUser(UINT32 ulMaxRecNum, VECTOR<OMT_USER_INFO_T> &vAddRec, VECTOR<OMT_USER_INFO_T> &vDelRec);
    BOOL SyncOverOMTUser(OMT_USER_INFO_T &stRec, INT32 iSyncFlag);


    BOOL AddNe(NE_BASIC_INFO_T &stNEBasicInfo);
    BOOL DelNe(CHAR *szNeID);
    BOOL SetNe(NE_BASIC_INFO_T &stNEBasicInfo, INT32 iSyncFlag);
    BOOL GetSyncingNE(UINT32 ulMaxRecNum, VECTOR<NE_BASIC_INFO_T> &vAddRec, VECTOR<NE_BASIC_INFO_T> &vDelRec);
    BOOL SyncOverNE(NE_BASIC_INFO_T &stRec, INT32 iSyncFlag);

    BOOL AddNeBasicInfo(NE_BASIC_INFO_T &stInfo);
    BOOL SetNeBasicInfo(NE_BASIC_INFO_T &stInfo, INT32 iSyncFlag);
    BOOL GetNEBasicInfo(VECTOR<NE_BASIC_INFO_T> &vInfo);
    BOOL GetSyncingNEBasicInfo(UINT32 ulMaxRecNum, VECTOR<NE_BASIC_INFO_T> &vAddRec, VECTOR<NE_BASIC_INFO_T> &vDelRec);
    BOOL SyncOverNEBasicInfo(NE_BASIC_INFO_T &stRec, INT32 iSyncFlag);

    BOOL GetActiveAlarm(VECTOR<ALARM_INFO_T> &vInfo);
    BOOL AddActiveAlarm(ALARM_INFO_T &stInfo);
    BOOL SetActiveAlarm(ALARM_INFO_T &stInfo, INT32 iSyncFlag);
    BOOL SyncOverActiveAlarm(ALARM_INFO_T &stInfo, INT32 iSyncFlag);
    BOOL GetSyncingActiveAlarm(UINT32 ulMaxRecNum, VECTOR<ALARM_INFO_T> &vAddRec, VECTOR<ALARM_INFO_T> &vDelRec);
    
    // 告警配置处理函数
	BOOL GetAlarmLevelCfg(VECTOR<ALARM_LEVEL_CFG_T> &vCfg);
	BOOL AddAlarmLevelCfg(ALARM_LEVEL_CFG_T &stCfg);
	BOOL DelAlarmLevelCfg(ALARM_LEVEL_CFG_T &stCfg);
	BOOL SetAlarmLevelCfg(ALARM_LEVEL_CFG_T &stCfg);
	BOOL SetAlarmLevelCfg(ALARM_LEVEL_CFG_T &stCfg, INT32 iSyncFlag);

    BOOL GetSyncingAlarmLevel(UINT32 ulMaxRecNum, VECTOR<ALARM_LEVEL_CFG_T> &vAddRec, VECTOR<ALARM_LEVEL_CFG_T> &vDelRec);
    BOOL SyncOverAlarmLevel(ALARM_LEVEL_CFG_T &stRec, INT32 iSyncFlag);

    BOOL AddHistoryAlarm(ALARM_INFO_T &stInfo);
    BOOL SetHistoryAlarm(ALARM_INFO_T &stInfo, INT32 iSyncFlag);
    BOOL ClearActiveAlarm(CHAR *szNeID, UINT32 ulAlarmID, UINT32 ulAlarmRaiseTime, UINT32 ulAlarmLevel, CHAR *szClearInfo);
    BOOL GetHistoryAlarm(UINT32 ulDevType, CHAR *szNeID, UINT32 ulStartTime, UINT32 ulEndTime, CHAR *szAlarmInfoKey, GJsonTupleParam &Param);
    BOOL GetHistoryAlarmStat(UINT32 ulHistoryAlarmDay, OMC_ALARM_STAT_T &stAlarmStat, OMC_ALARM_TREND_T &stAlarmTrend);
    BOOL SyncOverHistoryAlarm(ALARM_INFO_T &stInfo, INT32 iSyncFlag);
    BOOL GetSyncingHistoryAlarm(UINT32 ulMaxRecNum, VECTOR<ALARM_INFO_T> &vAddRec, VECTOR<ALARM_INFO_T> &vDelRec);
    BOOL AddActiveAlarm(VECTOR<ALARM_INFO_T> &vInfo);
    BOOL ClearActiveAlarm(VECTOR<ALARM_INFO_T> &vInfo, CHAR *szClearInfo);
    BOOL UpdateHistoryStatistics(UINT32 ulAlarmRaiseTime, UINT32 ulAlarmLevel);
	BOOL GetSyncingHistoryAlarmStatistic(UINT32 ulMaxRecNum, VECTOR<OMC_ALARM_STAT_T> &vAddRec, VECTOR<OMC_ALARM_STAT_T> &vDelRec);
	BOOL SetHistoryAlarmStatistic(OMC_ALARM_STAT_T &stInfo, INT32 iSyncFlag);
	BOOL SyncOverHistoryAlarmStatistic(OMC_ALARM_STAT_T &stInfo, INT32 iSyncFlag);

    BOOL GetTxStatus(UINT32 ulStartTime, UINT32 ulEndTime, VECTOR<OMA_TX_STATUS_REC_T> &vInfo);
    BOOL GetTxStatus(UINT32 ulStartTime, UINT32 ulEndTime, CHAR *szNeID, OMT_GET_TXSTATUS_RSP_T *pstRsp);
    BOOL GetAllTxStatus(UINT32 ulTime, OMT_GET_ALLTXSTATUS_RSP_T *pstRsp);

    BOOL SetNEStatus(OMA_TX_STATUS_REC_T &vInfo, INT32 iSyncFlag);
    BOOL GetSyncingTxStatus(UINT32 ulMaxRecNum, VECTOR<OMA_TX_STATUS_REC_T> &vAddRec, VECTOR<OMA_TX_STATUS_REC_T> &vDelRec);
    BOOL SyncOverTxStatus(OMA_TX_STATUS_REC_T &stInfo, INT32 iSyncFlag);



    BOOL GetAllDevStatus(UINT32 ulTime, GJsonTupleParam &Param);
    BOOL GetDevStatus(UINT32 ulStartTime, UINT32 ulEndTime, CHAR *szNeID, GJsonTupleParam &Param);
    BOOL GetLteStatus(UINT32 ulStartTime, UINT32 ulEndTime, CHAR *szNeID, GJsonTupleParam &Param);
    BOOL GetTrafficStatus(UINT32 ulStartTime, UINT32 ulEndTime, CHAR *szNeID, GJsonTupleParam &Param);

    BOOL AddDevOperLog(OMA_OPERLOG_T &stLog);
    BOOL SetDevOperLog(OMA_OPERLOG_T &stLog, INT32 iSyncFlag);
    BOOL GetDevOperLog(CHAR *szNeID, UINT32 ulStartTime, UINT32 ulEndTime, GJsonTupleParam &Param);
    BOOL GetSyncingDevOperLog(UINT32 ulMaxRecNum, VECTOR<OMA_OPERLOG_T> &vAddRec, VECTOR<OMA_OPERLOG_T> &vDelRec);
    BOOL SyncOverDevOperLog(OMA_OPERLOG_T &stInfo, INT32 iSyncFlag);

    BOOL AddDevFile(OMA_FILE_INFO_T &stFile);
    BOOL SetDevFile(OMA_FILE_INFO_T &stFile, INT32 iSyncFlag);
    BOOL SetDevFileName(OMA_FILE_INFO_T &stFile, INT32 iSyncFlag);
    BOOL UpdateDevFileExistFlag(CHAR *szNeID, CHAR *szFileName);
    BOOL GetDevFile(UINT32 ulDevFileType, CHAR *szNeID, UINT32 ulStartTime, UINT32 ulEndTime, GJsonTupleParam &Param);
    BOOL GetSyncingDevFile(UINT32 ulMaxRecNum, VECTOR<OMA_FILE_INFO_T> &vAddRec, VECTOR<OMA_FILE_INFO_T> &vDelRec);
    BOOL SyncOverDevFile(OMA_FILE_INFO_T &stRec, INT32 iSyncFlag);

    BOOL GetSwInfo(VECTOR<OMC_SW_INFO_T> &vInfo);//GJsonTupleParam &Param);
    BOOL AddSwInfo(OMC_SW_INFO_T &stFile);
    BOOL SetSwInfo(OMC_SW_INFO_T &stFile, INT32 iSyncFlag);
    BOOL DelSwInfo(CHAR *szSwVer);
    BOOL GetSyncingSwInfo(UINT32 ulMaxRecNum, VECTOR<OMC_SW_INFO_T> &vAddRec, VECTOR<OMC_SW_INFO_T> &vDelRec);
    BOOL SyncOverSwInfo(OMC_SW_INFO_T &stRec, INT32 iSyncFlag);

    BOOL GetSwUpdateInfo(VECTOR<OMC_SW_UPDATE_INFO_T> &vInfo);//GJsonTupleParam &Param);
    BOOL UpdateSw(CHAR *szNewVer, VECTOR<CHAR*> &vNeID);
    BOOL SetUpdateSwInfo(OMC_SW_UPDATE_INFO_T &stInfo, INT32 iSyncFlag);
    BOOL UpdateSwSetFlag(CHAR *szNeID, BOOL bSetFlag);
    BOOL GetSyncingUpdateSwInfo(UINT32 ulMaxRecNum, VECTOR<OMC_SW_UPDATE_INFO_T> &vAddRec, VECTOR<OMC_SW_UPDATE_INFO_T> &vDelRec);
    BOOL SyncOverUpdateSwInfo(OMC_SW_UPDATE_INFO_T &stRec, INT32 iSyncFlag);

    BOOL GetStationInfo(VECTOR<STATION_INFO_T> &vInfo);
    BOOL AddStationInfo(STATION_INFO_T &stInfo);
    BOOL SetStationInfo(STATION_INFO_T &stInfo, INT32 iSyncFlag);
    BOOL DelStationInfo(UINT32 ulStationID);
    BOOL GetSyncingStationInfo(UINT32 ulMaxRecNum, VECTOR<STATION_INFO_T> &vAddRec, VECTOR<STATION_INFO_T> &vDelRec);
    BOOL SyncOverStationInfo(STATION_INFO_T &stRec, INT32 iSyncFlag);

	BOOL AddTauStatus(VECTOR<OMA_TAU_STATUS_REC_T> &vInfo);
	BOOL AddTrafficStatus(VECTOR<OMA_TAU_STATUS_REC_T> &vInfo);
    BOOL AddTxStatus(VECTOR<OMA_TX_STATUS_REC_T> &vInfo);
    BOOL AddFxStatus(VECTOR<OMA_FX_STATUS_REC_T> &vInfo);
	BOOL AddDCStatus(VECTOR<OMA_DC_STATUS_REC_T> &vInfo);
	BOOL AddDISStatus(VECTOR<OMA_DIS_STATUS_REC_T> &vInfo);
	BOOL AddRECStatus(VECTOR<OMA_REC_STATUS_REC_T> &vInfo);

    /**
     * @brief           获取传入起始时间戳之间的历史告警不同等级的数量
     * @param           ulBeginTime         [in]    查询的开始时间
     * @param           ulEndTime           [in]    查询的结束时间
     * @param           stHistoryAlarmStat  [out]   查询结果(不同等级历史告警数量统计结果)
     * @return          BOOL 
     * @author          tuzhenglin(tuzhenglin@gbcom.com.cn)
     * @date            2022-01-26 11:11:41
     * @note
     */
    BOOL GetHistoryAlarmTotalCount(UINT32 ulBeginTime, UINT32 ulEndTime, OMC_ALARM_STAT_T &stHistoryAlarmStat); 
    /**
     * @brief           获取传入时间戳之间每天的历史告警不同等级的数量
     * @param           ulBegainTime    [in]    查询开始的时间
     * @param           ulEndTime       [in]    查询的结束时间
     * @param           vec             [out]   查询结果(每一天不同等级历史告警的数量统计结果)
     * @return          BOOL 
     * @author          tuzhenglin(tuzhenglin@gbcom.com.cn)
     * @date            2022-01-28 10:56:06
     * @note
     */
    BOOL GetHistoryAlarmDayCount(UINT32 ulBegainTime, UINT32 ulEndTime, VECTOR<OMC_ALARM_STAT_T> &vec);
    /**
     * @brief           检查要统计的历史告警信息
     * @param           ulHistoryAlarmStatisticDay    [in]  统计历史告警的天数
     * @return          BOOL 
     * @author          tuzhenglin(tuzhenglin@gbcom.com.cn)
     * @date            2022-02-14 09:22:31
     * @note
     */
    BOOL CheckHistoryAlarmStatistic(UINT32 ulHistoryAlarmStatisticDay);
    BOOL ClearOperLog(UINT32 ulTime);
    BOOL ClearDevFile(UINT32 ulTime);
    BOOL ClearDevOperLog(UINT32 ulTime);
    BOOL ClearHistoryAlarm(UINT32 ulTime);
    BOOL ClearTrafficStatus(UINT32 ulTime);
    BOOL ClearNeBasicStatus(UINT32 ulTime);
    BOOL ClearHistoryAlarmStatistic(UINT32 ulTime);
    BOOL BackupTxStatus(UINT32 ulStartTime, UINT32 ulEndTime, CHAR *szFile);
    BOOL BackupTrafficStatus(UINT32 ulStartTime, UINT32 ulEndTime, CHAR *szFile);
	BOOL GetOperLogInfo(UINT32 ulStartTime, UINT32 ulEndTime, CHAR *szOperatorID, UINT32 ulMaxNum, GJsonTupleParam &JsonTupleParam);

private:
    PDBApp  *m_pPDBApp;

    BOOL   IsRecordExist(const CHAR *szTable, const CHAR *szKey, UINT32 ulKey);
};

extern OmcDao* GetOmcDao();
extern OmcDao* GetOmcDao(PDBEnvBase *pPDBEnv);

#endif
