#ifndef OMC_PUBLIC_H
#define OMC_PUBLIC_H

#include "ds_public.h"
#include "GPublic.h"
#include "pdb_app.h"
#include "msg.h"
#include "snmp_nms.h"
#include "omc.h"
#include "OmcDao.h"

#define TIMER_INIT          TIMER0
#define EV_INIT_IND         EV_TIMER0

typedef struct
{
    //  SNMP 配置
    UINT32              ulSnmpVer;              // SNMP 版本
    UINT32              ulRecvTimeout;          // 接收超时时间

    UINT16              usSnmpPort;             // SNMP 端口
    UINT16              usTrapPort;             // Trap 端口

    CHAR                acGetCommunity[32];     // Get 共同体
    CHAR                acSetCommunity[32];     // Set 共同体
    CHAR                acTrapCommunity[32];    // Trap 共同体

    // NTP 配置
    UINT8               aucNTPAddr[4];          //< NTP 服务端IP地址
    UINT16              usNTPPort;              //< NTP 服务端端口
    UINT32              ulNTPSyncPeriod;        //< NTP 的同步周期，单位秒

    // 系统管理
    UINT32              ulClearDataHour;                // 数据清理时间
    UINT32              ulResvDataDays;                 // 普通数据(库）保留天数
    UINT32              ulVolumnResvDataDays;           // 流水表数据(库）保留天数
    UINT32              ulBackupFileNum;                // 数据备份文件个数
    UINT32              ulHistoryAlarmStatisticDay;     // 历史告警统计天数
    UINT32              ulClearTableInterval;           // 流水表数据库清理数据间隔
    CHAR                acBackupDir[256];               // 数据库备份目录
    UINT32              ulOnlineCheckPeriod;            // 检查在线周期
    UINT16              usTransFilePort;                // TransFile 端口 
    BOOL                bConflictDetection;             // 是否进行设备上线MAC/IP冲突处理
    UINT32              ulSaveDevInfoPeriod;             // 保存设备状态信息的周期

    // 集群配置
    UINT32              ulClusterDefaultStatus;         // 主备运行状态
    UINT8               aucClusterPeerAddr[4];          // 对端地址（内部心跳地址）
    UINT8               aucClusterMasterAddr[4];        // 主用状态下的地址（对外地址）

    // FTP配置
    UINT8               aucFtpAddr[4];
    UINT16              usFtpPort;
    CHAR                acFtpDir[256];
    CHAR                acFtpVerDir[256];;
    CHAR                acFtpLogFileDir[256];
    CHAR                acFtpUser[32];
    CHAR                acFtpPwd[32];
}OMC_LOCAL_CFG_T;   // OMC本地相关配置


extern VOID SendAllClient(UINT16 usSenderTaskID, UINT16 usMsgID, VOID *pvMsg, UINT32 ulMsgLen);
extern VOID OperLog(OPER_LOG_INFO_T *pstReport);
extern VOID OperLog(UINT16 usInstID, OPER_LOG_INFO_T *pstReport);
extern VOID SendAllUser(UINT16 usSenderTaskID, UINT16 usMsgID, VOID *pvMsg, UINT32 ulMsgLen);

extern HANDLE  g_hSnmpMemPool;

void InitSnmpMemPool();
void PrintSnmpMemPool();
BOOL LoadLocalCfg();
OMC_LOCAL_CFG_T& GetLocalCfg();
VOID CopyText(CHAR *szDst, UINT32 ulMaxDstLen, CHAR *szSrc, UINT32 ulMaxSrcLen);
VOID CopyUtf8Text(CHAR *szDst, UINT32 ulMaxDstLen, CHAR *szSrc, UINT32 ulSrcLen);
VOID ToNeID(UINT8 *pucMAC, CHAR *szAlarmNeID);

BOOL CheckDevType(UINT32 ulDevType);
BOOL CheckDevArea(UINT32 ulDevArea);

VOID DevInfoHton(OMA_DEV_INFO_T &stDevInfo);
VOID InitDevInfo(VECTOR<NE_BASIC_INFO_T> &vInfo);
VOID SetDevInfo(OMA_DEV_INFO_T &stInfo);
BOOL GetDevInfoByIndex(UINT32 &ulIndex, OMA_DEV_INFO_T &stInfo);
BOOL GetDevInfo(CHAR *szNeID, OMA_DEV_INFO_T &stInfo);
BOOL GetDevInfo(IP_ADDR_T *pstClientAddr, OMA_DEV_INFO_T &stInfo);
VOID GetDevInfo(VECTOR<OMA_DEV_INFO_T> &vInfo);
VOID GetDevInfo(VECTOR<NE_BASIC_INFO_T> &vInfo);
UINT32 GetDevNum();
VOID GetDevInfo(UINT32 ulDevType, VECTOR<OMA_DEV_INFO_T> &vInfo);
VOID SetNe(CHAR *szNeID, CHAR *szDevName, UINT32 ulDevType, CHAR *szMAC);
VOID DelNe(CHAR *szNeID);

VOID SetResStatus(OMA_RES_STATUS_T &stStatus);
VOID SetLteStatus(OMA_LTE_STATUS_T &stStatus);
VOID SetTxRunStatus(OMA_TX_RUN_STATUS_T &stStatus);
VOID SetFxRunStatus(OMA_FX_RUN_STATUS_T &stStatus);

BOOL CheckUser(CHAR *szUser, CHAR *szPwd, UINT32 *pulType, CHAR *szErrInfo);
VOID InitUserInfo(VECTOR<OMT_USER_INFO_T> &vInfo);
VOID GetUserInfo(VECTOR<OMT_USER_INFO_T> &vInfo);
VOID AddUserInfo(OMT_USER_INFO_T &stInfo);
VOID DelUserInfo(CHAR *stInfo);
VOID AlterUserInfo(OMT_USER_INFO_T &stInfo);
VOID SetOmtLogin(CHAR *szUser, PID_T *pstPID);
BOOL GetOMTPID(CHAR *szUser, PID_T *pstPID);

VOID InitAlarmLevelCfg(VECTOR<ALARM_LEVEL_CFG_T> &vCfg);
VOID GetAlarmLevelCfg(VECTOR<ALARM_LEVEL_CFG_T> &vCfg);
VOID SetAlarmLevelCfg(ALARM_LEVEL_CFG_T &Cfg);
VOID AddAlarmLevelCfg(ALARM_LEVEL_CFG_T &Cfg);
VOID DelAlarmLevelCfg(ALARM_LEVEL_CFG_T &Cfg);

VOID InitActiveAlarm(VECTOR<ALARM_INFO_T> &vInfo);
VOID GetActiveAlarm(VECTOR<ALARM_INFO_T> &vInfo);
VOID AddActiveAlarm(ALARM_INFO_T &stInfo, BOOL &bIsNewAlarm);
VOID DelActiveAlarm(CHAR *szNeID, UINT32 ulAlarmID);
VOID GetActiveAlarmStat(OMC_ALARM_STAT_T &stAlarmStat, OMC_ALARM_TREND_T &stAlarmTrend);
BOOL GetActiveAlarmInfo(CHAR *szNeID, UINT32 ulAlarmID, ALARM_INFO_T &stInfo);
VOID GetActiveAlarmInfo(CHAR *szNeID, VECTOR<ALARM_INFO_T> &vInfo);

VOID AddDevOperLog(OMA_OPERLOG_T &stInfo);
VOID GetDevOperLog(CHAR *szNeID, UINT32 ulStartTime, UINT32 ulEndTime, VECTOR<OMA_OPERLOG_T> &vLog);

VOID InitSwInfo(VECTOR<OMC_SW_INFO_T> &vInfo);
VOID AddSwInfo(OMC_SW_INFO_T &stInfo);
VOID DelSwInfo(CHAR *szSwVer);
BOOL GetSwInfo(CHAR *szSwVer, OMC_SW_INFO_T &stInfo);
VOID GetSwInfo(VECTOR<OMC_SW_INFO_T> &vInfo);

VOID InitSwUpdate(VECTOR<OMC_SW_UPDATE_INFO_T> &vInfo);
VOID SetSwUpdate(CHAR *szNewVer, VECTOR<CHAR*> &vNeID);
VOID GetSwUpdate(VECTOR<OMC_SW_UPDATE_INFO_T> &vInfo);
BOOL GetSwUpdate(CHAR *szNeID, OMC_SW_UPDATE_INFO_T &stInfo);
BOOL SetSwUpdateSetFlag(CHAR *szNeID, BOOL bSetFlag);
VOID DelSwUpdate(CHAR *szSwVer);

VOID InitStationInfo(VECTOR<STATION_INFO_T> &vInfo);
VOID GetStationInfo(VECTOR<STATION_INFO_T>  &vInfo);
VOID AddStationInfo(STATION_INFO_T  &stInfo);
VOID SetStationInfo(STATION_INFO_T  &stInfo);
VOID DelStationInfo(UINT32  ulStationID);

VOID ConvertOmtUserInfoToJson(VECTOR<OMT_USER_INFO_T> &vInfo, GJsonTupleParam &JsonTupleParam);

VOID ConvertNeBasicInfoToJson(VECTOR<OMA_DEV_INFO_T> &vInfo, GJsonTupleParam &JsonTupleParam);
VOID ConvertNeInfoToJson(OMA_DEV_INFO_T &stInfo, GJsonParam &JsonParam);
//VOID ConvertNeInfoToJson(CHAR *szNeID, NE_INFO_T &stInfo, GJsonParam &JsonParam);
VOID ConvertClusterStatusInfoToJson(VECTOR<OMA_DEV_INFO_T> &vInfo, GJsonTupleParam &JsonTupleParam);

VOID ConvertAlarmLevelCfgToJson(VECTOR<ALARM_LEVEL_CFG_T> &vCfg, GJsonTupleParam &JsonTupleParam);
VOID ConvertActiveAlarmInfoToJson(VECTOR<ALARM_INFO_T> &vInfo, GJsonTupleParam &JsonTupleParam);
VOID ConvertSwInfo(VECTOR<OMC_SW_INFO_T> &vInfo, GJsonTupleParam &JsonTupleParam);
VOID ConvertSwUpdate(VECTOR<OMC_SW_UPDATE_INFO_T> &vInfo, GJsonTupleParam &JsonTupleParam);
UINT32 ConvertTxRunStatus(OMA_TX_RUN_STATUS_T &stInfo);
VOID ConvertTxRunStatus(UINT32 ulStatus, OMA_TX_RUN_STATUS_T &stInfo);

UINT32 ConvertFxRunStatus(OMA_FX_RUN_STATUS_T &stInfo);
VOID ConvertFxRunStatus(UINT32 ulStatus, OMA_FX_RUN_STATUS_T &stInfo);
VOID ConvertStationInfo(VECTOR<STATION_INFO_T> &vInfo, GJsonTupleParam &JsonTupleParam);

VOID SetOnline(OMA_DEV_INFO_T &stInfo, BOOL bIsOnline);
VOID RemoveFile(CHAR *szDir, CHAR *szFilePrefix);
VOID RemoveFile(CHAR *szDir, CHAR *szFilePrefix, UINT32 ulMaxFileNum);
#endif
