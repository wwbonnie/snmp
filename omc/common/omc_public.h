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
    //  SNMP ����
    UINT32              ulSnmpVer;              // SNMP �汾
    UINT32              ulRecvTimeout;          // ���ճ�ʱʱ��

    UINT16              usSnmpPort;             // SNMP �˿�
    UINT16              usTrapPort;             // Trap �˿�

    CHAR                acGetCommunity[32];     // Get ��ͬ��
    CHAR                acSetCommunity[32];     // Set ��ͬ��
    CHAR                acTrapCommunity[32];    // Trap ��ͬ��

    // NTP ����
    UINT8               aucNTPAddr[4];          //< NTP �����IP��ַ
    UINT16              usNTPPort;              //< NTP ����˶˿�
    UINT32              ulNTPSyncPeriod;        //< NTP ��ͬ�����ڣ���λ��

    // ϵͳ����
    UINT32              ulClearDataHour;                // ��������ʱ��
    UINT32              ulResvDataDays;                 // ��ͨ����(�⣩��������
    UINT32              ulVolumnResvDataDays;           // ��ˮ������(�⣩��������
    UINT32              ulBackupFileNum;                // ���ݱ����ļ�����
    UINT32              ulHistoryAlarmStatisticDay;     // ��ʷ�澯ͳ������
    UINT32              ulClearTableInterval;           // ��ˮ�����ݿ��������ݼ��
    CHAR                acBackupDir[256];               // ���ݿⱸ��Ŀ¼
    UINT32              ulOnlineCheckPeriod;            // �����������
    UINT16              usTransFilePort;                // TransFile �˿� 
    BOOL                bConflictDetection;             // �Ƿ�����豸����MAC/IP��ͻ����
    UINT32              ulSaveDevInfoPeriod;             // �����豸״̬��Ϣ������

    // ��Ⱥ����
    UINT32              ulClusterDefaultStatus;         // ��������״̬
    UINT8               aucClusterPeerAddr[4];          // �Զ˵�ַ���ڲ�������ַ��
    UINT8               aucClusterMasterAddr[4];        // ����״̬�µĵ�ַ�������ַ��

    // FTP����
    UINT8               aucFtpAddr[4];
    UINT16              usFtpPort;
    CHAR                acFtpDir[256];
    CHAR                acFtpVerDir[256];;
    CHAR                acFtpLogFileDir[256];
    CHAR                acFtpUser[32];
    CHAR                acFtpPwd[32];
}OMC_LOCAL_CFG_T;   // OMC�����������


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
