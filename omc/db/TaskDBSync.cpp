#include "TaskDBSync.h"

#define TIMER_CHECK_STATUS                  TIMER4
#define EV_CHECK_STATUS_TIMER               EV_TIMER4

UINT32  g_ulCheckPeriod = 3;
UINT32  g_ulSyncRecNum = 300;

extern CHAR     *g_szAlarmRaiseFile;
extern CHAR     *g_szAlarmClearFile;
PDBEnvBase      *g_PeerPDBEnv = NULL;
extern GTransFile          *g_TransFile;
extern PDB_CONN_PARAM_T     g_stPeerConnParam;
extern OMC_LOCAL_CFG_T      g_stLocalCfg;

extern UINT32              g_ulClusterDefaultStatus;
extern UINT8               g_aucClusterPeerAddr[4];
extern BOOL ZipFile(CHAR *szFile, CHAR *szZipFile);

BOOL TaskDBSync::InitPeerDBEnv()
{
    /*��ʼ�����ݿ⻷��*/
    if (g_stPeerConnParam.ulDBType == PDBT_NOSQL)
    {
        g_PeerPDBEnv = new PDBNoSQLEnv();
    }
#if 1//def HAVE_MYSQL
    else if (g_stPeerConnParam.ulDBType == PDBT_MYSQL)
    {
        g_PeerPDBEnv = new PDBMySQLEnv();
    }
#endif
#ifdef _OSWIN32_
    else if (g_stPeerConnParam.ulDBType & PDBT_ODBC)
    {
        g_PeerPDBEnv = new PDBODBCEnv();
    }
#endif
#ifdef HAVE_ORACLE
    else if (g_stPeerConnParam.ulDBType == PDBT_ORACLE)
    {
        g_PeerPDBEnv = new PDBOracleEnv();
    }
#endif

    if (g_PeerPDBEnv == NULL)
    {
        TaskLog(LOG_FATAL, "init peer db env failed, db type is %u", g_stPeerConnParam.ulDBType);
        return FALSE;
    }

    TaskLog(LOG_INFO, "peer db param: type=%u, server=%s, port=%u, dbname=%s, character-set=%s",
        g_stPeerConnParam.ulDBType, g_stPeerConnParam.acServer, g_stPeerConnParam.usPort,
        g_stPeerConnParam.acDBName, g_stPeerConnParam.acCharacterSet);

    return TRUE;
}

BOOL TaskDBSync::InitPeerDBConn()
{
    static BOOL bInit = FALSE;

    if (bInit)
    {
        return TRUE;
    }

    UINT32  ulTimeout = 10;
    BOOL bRet = g_PeerPDBEnv->InitEnv(&g_stPeerConnParam,
                                PDB_PREFETCH_ROW_NUM,
                                ulTimeout,
                                1);

    if (PDB_SUCCESS != bRet)
    {
        TaskLog(LOG_FATAL, "init peer db connection failed");
        return FALSE;
    }

    bInit = TRUE;
    TaskLog(LOG_INFO, "init peer db connection successful!");
    return TRUE;
}

TaskDBSync::TaskDBSync(UINT16 usDispatchID):GBaseTask(MODULE_DBSYNC, (CHAR*)"Sync", usDispatchID)
{
    m_pDao = NULL;
    m_pPeerDao = NULL;
    m_fpRaise = NULL;
    m_fpClear = NULL;
    //m_acRaiseFile[128] = {0};
    //m_acRaiseTmpFile[128] = {0};
}

BOOL TaskDBSync::Init()
{
    if (!m_pDao)
    {
        m_pDao = GetOmcDao();
        if (m_pDao == NULL)
        {
            return FALSE;
        }
    }

    if (!g_PeerPDBEnv)
    {
        if (!InitPeerDBEnv())
        {
            return FALSE;
        }
    }

    if (!m_pPeerDao)
    {
        if (!InitPeerDBConn())
        {
            return FALSE;
        }

        m_pPeerDao = GetOmcDao(g_PeerPDBEnv);
        if (m_pPeerDao == NULL)
        {
            return FALSE;
        }
    }

    CHAR acRaiseFile[128] = {0};
    CHAR acClearFile[128] = {0};

    sprintf(acRaiseFile, "%s/%s", gos_get_root_path(), g_szAlarmRaiseFile);
    sprintf(acClearFile, "%s/%s", gos_get_root_path(), g_szAlarmClearFile);

    gos_format_path(acRaiseFile);
    gos_format_path(acClearFile);
    strcpy(m_acAlarmRaiseFile, acRaiseFile);
    strcpy(m_acAlarmClearFile, acClearFile);

    if(!InitCheckTimer())
    {
        TaskLog(LOG_ERROR, "init check timer failed");
        return FALSE;
    }

    TaskLog(LOG_INFO, "Task init successful!");
    SetTaskStatus(TASK_STATUS_WORK);

    return TRUE;
}

BOOL TaskDBSync::InitCheckTimer()
{
    ClearTimer(TIMER_CHECK_STATUS);

    if (!SetLoopTimer(TIMER_CHECK_STATUS, g_ulCheckPeriod*1000))
    {
        return FALSE;
    }

    return TRUE;
}

VOID TaskDBSync::SyncAlarmRaise()
{
    VECTOR<ALARM_INFO_T>    vInfo;
    ALARM_INFO_T            stInfo ={0}; 
    INT32                   iSize = 0;

    if (!gos_file_exist(m_acAlarmRaiseFile))
    {
        return;
    }

    m_fpRaise = fopen(m_acAlarmRaiseFile, "r+b");
    if (NULL == m_fpRaise)
    {
       TaskLog(LOG_ERROR, "SyncAlarmRaise: open %s fail", m_acAlarmRaiseFile); 
       return;
    }

    fseek(m_fpRaise, 0, SEEK_END);
    iSize = ftell(m_fpRaise);
    iSize = iSize / sizeof(ALARM_INFO_T);
    fseek(m_fpRaise, 0, SEEK_SET);

    for (UINT32 i=0; i < iSize; i++)
    {
        fread(&stInfo, sizeof(ALARM_INFO_T), 1, m_fpRaise);
        vInfo.push_back(stInfo);
    }

    fclose(m_fpRaise);
    TaskLog(LOG_INFO, "start insert raise_alarm ");
    if (m_pDao->AddActiveAlarm(vInfo))
    {
        TaskLog(LOG_INFO, "Read %d raise_alarm succc", vInfo.size());
        gos_delete_file(m_acAlarmRaiseFile);
    }
    else
    {
        TaskLog(LOG_ERROR, "Read raise_alarm fail");
    }    
}

VOID TaskDBSync::SyncAlarmClear()
{
    VECTOR<ALARM_INFO_T>    vInfo;
    ALARM_INFO_T            stInfo ={0}; 
    INT32                   iSize = 0;

    if (!gos_file_exist(m_acAlarmClearFile))
    {
        return;
    }

    m_fpClear = fopen(m_acAlarmClearFile, "r+b");
    if (NULL == m_fpClear)
    {
       TaskLog(LOG_ERROR, "SyncAlarmClear: open %s fail", m_acAlarmClearFile); 
       return;
    }
    fseek(m_fpClear, 0, SEEK_END);
    iSize = ftell(m_fpClear);
    iSize = iSize / sizeof(ALARM_INFO_T);
    fseek(m_fpClear, 0, SEEK_SET);

    for (UINT32 i=0; i < iSize; i++)
    {
        fread(&stInfo, sizeof(ALARM_INFO_T), 1, m_fpClear);
        vInfo.push_back(stInfo);
    }
    
    fclose(m_fpClear);
    TaskLog(LOG_INFO, "start insert clear_alarm ");
    if (m_pDao->ClearActiveAlarm(vInfo, (CHAR *)ALARM_CLEAR_AUTO))
    {
        TaskLog(LOG_INFO, "Read %d clear_alarm succc", vInfo.size());
        gos_delete_file(m_acAlarmClearFile);
    }
    else
    {
        TaskLog(LOG_ERROR, "Read clear_alarm fail");
    }    
}

VOID TaskDBSync::BackupTable()
{
    CHAR    acCmd[1024];
    CHAR    acBackupDir[256];
    CHAR    acFile[256];
    CHAR    acTime[256];
    CHAR    *szDBFilePrefix = (CHAR*)"db_";
	CHAR    *szStatusFilePrefix = (CHAR*)"ne_basic_status_";
	CHAR    *szTrafficFilePrefix = (CHAR*)"traffic_status_";

    TaskLog(LOG_INFO, "start backup tables");

    sprintf(acBackupDir, "%s/bak", gos_get_root_path());
    if (!gos_file_exist(acBackupDir))
    {
        gos_create_dir(acBackupDir);
    }

    gos_get_text_time_ex(NULL, "%04u%02u%02u", acTime);
    sprintf(acFile, "%s/%s%s", acBackupDir, szDBFilePrefix, acTime);

#ifdef _OSWIN32_
    sprintf(acCmd, "%s/backup_db.bat %s", gos_get_root_path(), acFile);
#else
    sprintf(acCmd, "%s/backup_db.sh %s", gos_get_root_path(), acFile);
#endif

    gos_system(acCmd);

    // �������ݿ�����ļ�(����Ϊ��λ���ɣ�һ��һ���ļ�)
    RemoveFile(acBackupDir, szDBFilePrefix, g_stLocalCfg.ulBackupFileNum);
	// �����豸״̬��Ϣ����ѹ���ļ�(��СʱΪ��λ���ɣ�һ��24��ѹ���ļ�)
	RemoveFile(acBackupDir, szStatusFilePrefix, g_stLocalCfg.ulVolumnResvDataDays*24);
	// �����豸������Ϣ����ѹ���ļ�(��СʱΪ��λ���ɣ�һ��24��ѹ���ļ�)
	RemoveFile(acBackupDir, szTrafficFilePrefix, g_stLocalCfg.ulVolumnResvDataDays*24);

	// ����omc���и�Ŀ¼�µ�ѹ���ļ�
	// omtͨ�����·����ȡ��ʷ״̬����ʱ��omc���ļ���bakĿ¼�¸��Ƶ����и�Ŀ¼��
	RemoveFile(gos_get_root_path(), szStatusFilePrefix);
	RemoveFile(gos_get_root_path(), szTrafficFilePrefix);
}

VOID TaskDBSync::ClearNeBasicStatusTable()
{
    static  BOOL     bIsFirst = TRUE;
    CHAR    *szFilePrefix = (CHAR*)"ne_basic_status_";
    CHAR    acFile[256];
    CHAR    acZipFile[256];
    CHAR    acTime[256];
    UINT32  ulCurrTime = gos_get_current_time();
    UINT32  ulUptime = gos_get_uptime();
    static  UINT32  ulBackTime = 0;
    static  UINT32   ulLastBackTime = 0;
    GOS_DATETIME_T  stTime;

    /*
    ToDo:
        ������ˮ���ݱ�(ne_basic_status/traffic_status)���ö�α��ݴ���
        ����Сʱ���ݣ�ÿһ��Сʱ����һ��Jsonѹ���ļ�
        �ͻ��˲鿴��ˮ��Ϣ��ʵʱ��Ϣ�ʹ����ݿ��ѯ����ʷ��������ѹ����Json�ļ�
        ���ݱ���Ҫ֧�֣��洢�ռ䲻�㵼�±���ʧ�ܺ�洢�ռ����ݺ��ܼ������ݣ���ǰ����ʧ�ܵ�����������
        ֧�ֶԱ���·�������ã�ȱʡ��ǰĿ¼�µ�backup��
    */
    // ÿ������������ı�����������

    // С�����ݿⱸ��ʱ�������򲻱���
    if (ulUptime - ulLastBackTime < g_stLocalCfg.ulClearTableInterval)
    {
        return;
    }
    
    /*
        ����ʱ��ȡ���ñ���������¼��ʼ��ʱ��,���ڼ�����ʱ���ڵı����ļ��Ƿ񶼴���
    */
    if (bIsFirst)
    {
        ulBackTime =  gos_get_current_time() - g_stLocalCfg.ulVolumnResvDataDays*86400;
        // ��ȡ��ʼ����ʱ�������ʱ���
        gos_get_localtime(&ulBackTime, &stTime);
        stTime.ucMinute = 0;
        stTime.ucSecond = 0;
        ulBackTime = gos_mktime(&stTime);
        bIsFirst = FALSE;
    }

    while (ulBackTime < ulCurrTime - 3600)
    {
        gos_get_text_time_ex(&ulBackTime, "%04u%02u%02u%02u", acTime);
        // ne_basic_status �����ļ�
        sprintf(acFile, "%s/%s%s", g_stLocalCfg.acBackupDir, szFilePrefix, acTime);
        sprintf(acZipFile, "%s.zip", acFile);

        // ����Ƿ���ڱ����ļ�
        if (!gos_file_exist(acZipFile))
        {
            if (m_pDao->BackupTxStatus(ulBackTime, ulBackTime+3600, acFile) &&
                ZipFile(acFile, acZipFile))
            {
               m_pDao->ClearNeBasicStatus(ulBackTime);
               gos_delete_file(acFile); 
            }
            else
            {
                TaskLog(LOG_ERROR, "backup table ne_basic_status fail: %s", acFile);
            }

            ulBackTime += 3600;
            // ��δ���
            break;
        }

        ulBackTime += 3600;  
    }

    // ��¼���һ�������ʱ��
    ulLastBackTime = gos_get_uptime();
}

VOID TaskDBSync::ClearTrafficStatusTable()
{
    CHAR    *szFilePrefix = (CHAR*)"traffic_status_";
    CHAR    acFile[256];
    CHAR    acZipFile[256];
    CHAR    acTime[256];
    static  BOOL    bIsFirst = TRUE;
    static  UINT32  ulBackTime = 0;
    static  UINT32  ulLastBackTime = 0;
    UINT32  ulCurrTime = gos_get_current_time();
    UINT32  ulUptime = gos_get_uptime();
    GOS_DATETIME_T  stTime;

    /*
    ToDo:
        ������ˮ���ݱ�(ne_basic_status/traffic_status)���ö�α��ݴ���
        ����Сʱ���ݣ�ÿһ��Сʱ����һ��Jsonѹ���ļ�
        �ͻ��˲鿴��ˮ��Ϣ��ʵʱ��Ϣ�ʹ����ݿ��ѯ����ʷ��������ѹ����Json�ļ�
        ���ݱ���Ҫ֧�֣��洢�ռ䲻�㵼�±���ʧ�ܺ�洢�ռ����ݺ��ܼ������ݣ���ǰ����ʧ�ܵ�����������
        ֧�ֶԱ���·�������ã�ȱʡ��ǰĿ¼�µ�backup��
    */
    
    // С�����ݿⱸ��ʱ�������򲻱���
    if (ulUptime - ulLastBackTime < g_stLocalCfg.ulClearTableInterval)
    {
        return;
    }
    
    /*
        ����ʱ��ȡ���ñ���������¼��ʼ��ʱ��,���ڼ�����ʱ���ڵı����ļ��Ƿ񶼴���
    */
    if (bIsFirst)
    {
        ulBackTime =  gos_get_current_time() - g_stLocalCfg.ulVolumnResvDataDays*86400;
        // ��ȡ��ʼ����ʱ�������ʱ���
        gos_get_localtime(&ulBackTime, &stTime);
        stTime.ucMinute = 0;
        stTime.ucSecond = 0;
        ulBackTime = gos_mktime(&stTime);
        bIsFirst = FALSE;
    }

    while (ulBackTime < ulCurrTime - 3600)
    {
        gos_get_text_time_ex(&ulBackTime, "%04u%02u%02u%02u", acTime);
        sprintf(acFile, "%s/%s%s", g_stLocalCfg.acBackupDir, szFilePrefix, acTime);
        sprintf(acZipFile, "%s.zip", acFile);

        // ����Ƿ���ڱ����ļ�
        if (!gos_file_exist(acZipFile))
        {
            if (m_pDao->BackupTrafficStatus(ulBackTime, ulBackTime+3600, acFile) &&
                ZipFile(acFile, acZipFile))
            {
               m_pDao->ClearTrafficStatus(ulBackTime);
               gos_delete_file(acFile); 
            }
            else
            {
                TaskLog(LOG_ERROR, "backup table traffic_status fail: %s", acFile);
            }

            ulBackTime += 3600;
            // ��δ���
            break;
        }

        ulBackTime += 3600;  
    }
    // ��¼���һ�������ʱ��
    ulLastBackTime = gos_get_uptime();    
}

VOID TaskDBSync::ClearTable()
{
    static  UINT32   ulLastClearTime = 0;
    UINT32  ulCurrTime = gos_get_current_time();
    UINT32  ulDeadTime = gos_get_current_time() - g_stLocalCfg.ulResvDataDays*86400;
    UINT32  ulUptime = gos_get_uptime();
    UINT32  ulSpendTime = 0;
    GOS_DATETIME_T  stTime;

    gos_get_localtime(&ulCurrTime, &stTime);
    if (stTime.ucHour != g_stLocalCfg.ulClearDataHour)
    {
        return;
    }

    // 1���ڲ��ظ�����
    if (ulLastClearTime > 0 &&
        (ulLastClearTime + 86400) > ulUptime)
    {
        return;
    }
    // ��¼���һ�������ʱ��
    ulLastClearTime = gos_get_uptime();

    /*ToDo
        omc���ݿⱸ�ݣ�����������ÿ�κ�ʱ���������Ż�
        ���ݳ�ѹ���ļ�
    */
    ulSpendTime = gos_get_uptime_1ms();
    BackupTable();
    TaskLog(LOG_INFO, "backup tables spend: %u ms", gos_get_uptime_1ms()-ulSpendTime);

    /*ToDo
      ����omc������ÿ��洢�����ݱȽϴ�һ��������ʱ����ܻ����
      ����������ϴ�ı����������������Ƕ����������ǰ���һ���������
    */
    ulSpendTime = gos_get_uptime_1ms();
    TaskLog(LOG_INFO, "start clear tables");
    m_pDao->CheckHistoryAlarmStatistic(g_stLocalCfg.ulHistoryAlarmStatisticDay);
    m_pDao->ClearDevFile(ulDeadTime);
    m_pDao->ClearDevOperLog(ulDeadTime); // �ն˲�����־
    m_pDao->ClearOperLog(ulDeadTime);    // OMT������־
    m_pDao->ClearHistoryAlarm(ulDeadTime);
    m_pDao->ClearHistoryAlarmStatistic(ulDeadTime);

    TaskLog(LOG_INFO, "clear tables spend time: %u ms", gos_get_uptime_1ms()-ulSpendTime);
}

VOID TaskDBSync::SyncOMTUser()
{
    VECTOR<OMT_USER_INFO_T>     vAddRec;
    VECTOR<OMT_USER_INFO_T>     vDelRec;

    if (!m_pDao->GetSyncingOMTUser(g_ulSyncRecNum, vAddRec, vDelRec) ||
        (vAddRec.size() == 0 && vDelRec.size() == 0) )
    {
        return;
    }

    m_pPeerDao->TransBegin();
    m_pDao->TransBegin();

    for (UINT32 i=0; i<vAddRec.size(); i++)
    {
        OMT_USER_INFO_T     &stRec = vAddRec[i];

        if (m_pPeerDao->SetOMTUser(stRec, SYNC_NULL) )
        {
            m_pDao->SyncOverOMTUser(stRec, SYNC_ADD);

            TaskLog(LOG_DETAIL, "sync omt_user succ: UserID=%s UserType=%s", stRec.acUser, stRec.acPwd);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync omt_user failed: UserID=%s UserType=%s", stRec.acUser, stRec.acPwd);
            goto end;
        }
    }

    for (UINT32 i=0; i<vDelRec.size(); i++)
    {
        OMT_USER_INFO_T     &stRec = vDelRec[i];

        if (m_pPeerDao->SyncOverOMTUser(stRec, SYNC_DEL) )
        {
            m_pDao->SyncOverOMTUser(stRec, SYNC_DEL);

            TaskLog(LOG_DETAIL, "sync omt_user succ: UserID=%s UserType=%s", stRec.acUser, stRec.acPwd);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync omt_user failed: UserID=%s UserType=%s", stRec.acUser, stRec.acPwd);
            goto end;
        }
    }

end:
    if (m_pPeerDao->TransCommit())
    {
        m_pDao->TransCommit();
    }
    else
    {
        m_pDao->TransRollback();
    }
}
VOID TaskDBSync::SyncOperLog()
{
    VECTOR<OMT_OPER_LOG_INFO_T>     vAddRec;
    VECTOR<OMT_OPER_LOG_INFO_T>     vDelRec;

    if (!m_pDao->GetSyncingOperLog(g_ulSyncRecNum, vAddRec, vDelRec) ||
        (vAddRec.size() == 0 && vDelRec.size() == 0))
    {
        return;
    }

    m_pPeerDao->TransBegin();
    m_pDao->TransBegin();

    for (UINT32 i=0; i<vAddRec.size(); i++)
    {
        OMT_OPER_LOG_INFO_T     &stRec = vAddRec[i];

        if (m_pPeerDao->SetOperLog(stRec, SYNC_NULL) )
        {
            m_pDao->SyncOverOperLog(stRec, SYNC_ADD);

            TaskLog(LOG_DETAIL, "sync operlog succ: %u %u", stRec.ulTime, stRec.acUserID);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync operlog failed: %u %u", stRec.ulTime, stRec.acUserID);
            goto end;
        }
    }

    for (UINT32 i=0; i<vDelRec.size(); i++)
    {
        OMT_OPER_LOG_INFO_T     &stRec = vDelRec[i];

        if (m_pPeerDao->SyncOverOperLog(stRec, SYNC_DEL))
        {
            m_pDao->SyncOverOperLog(stRec, SYNC_DEL);

            TaskLog(LOG_DETAIL, "sync operlog succ: %u %u", stRec.ulTime, stRec.acUserID);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync operlog failed: %u %u", stRec.ulTime, stRec.acUserID);
            goto end;
        }
    }

end:
    if (m_pPeerDao->TransCommit())
    {
        m_pDao->TransCommit();
    }
    else
    {
        m_pDao->TransRollback();
    }
}

VOID TaskDBSync::SyncActiveAlarm()
{
    VECTOR<ALARM_INFO_T>     vAddRec;
    VECTOR<ALARM_INFO_T>     vDelRec;

    if (!m_pDao->GetSyncingActiveAlarm(g_ulSyncRecNum, vAddRec, vDelRec) ||
        (vAddRec.size() == 0 && vDelRec.size() == 0))
    {
        return;
    }

    m_pPeerDao->TransBegin();
    m_pDao->TransBegin();

    for (UINT32 i=0; i<vAddRec.size(); i++)
    {
        ALARM_INFO_T     &stRec = vAddRec[i];

        if (m_pPeerDao->SetActiveAlarm(stRec, SYNC_NULL))
        {
            m_pDao->SyncOverActiveAlarm(stRec, SYNC_ADD);

            TaskLog(LOG_DETAIL, "sync active_alarm succ: %s %u", stRec.acNeID, stRec.ulAlarmID);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync active_alarm failed: %s %u", stRec.acNeID, stRec.ulAlarmID);
            goto end;
        }
    }

    for (UINT32 i=0; i<vDelRec.size(); i++)
    {
        ALARM_INFO_T     &stRec = vDelRec[i];

        if (m_pPeerDao->SyncOverActiveAlarm(stRec, SYNC_DEL))
        {
            m_pDao->SyncOverActiveAlarm(stRec, SYNC_DEL);

            TaskLog(LOG_DETAIL, "sync active_alarm succ: %s %u", stRec.acNeID, stRec.ulAlarmID);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync active_alarm failed: %s %u", stRec.acNeID, stRec.ulAlarmID);
            goto end;
        }
    }

end:
    if (m_pPeerDao->TransCommit())
    {
        m_pDao->TransCommit();
    }
    else
    {
        m_pDao->TransRollback();
    }
}

VOID TaskDBSync::SyncAlarmLevel()
{
    VECTOR<ALARM_LEVEL_CFG_T>     vAddRec;
    VECTOR<ALARM_LEVEL_CFG_T>     vDelRec;

    if (!m_pDao->GetSyncingAlarmLevel(g_ulSyncRecNum, vAddRec, vDelRec) ||
        (vAddRec.size() == 0 && vDelRec.size() == 0) )
    {
        return;
    }

    m_pPeerDao->TransBegin();
    m_pDao->TransBegin();

    for (UINT32 i=0; i<vAddRec.size(); i++)
    {
        ALARM_LEVEL_CFG_T     &stRec = vAddRec[i];

        if (m_pPeerDao->SetAlarmLevelCfg(stRec, SYNC_NULL))
        {
            m_pDao->SyncOverAlarmLevel(stRec, SYNC_ADD);

            TaskLog(LOG_DETAIL, "sync alarm_level_cfg succ: %u %u %u", stRec.ulAlarmID, stRec.ulAlarmLevel, stRec.bIsIgnore);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync alarm_level_cfg failed: %u %u %u ", stRec.ulAlarmID, stRec.ulAlarmLevel, stRec.bIsIgnore);
            goto end;
        }
    }

    for (UINT32 i=0; i<vDelRec.size(); i++)
    {
        ALARM_LEVEL_CFG_T     &stRec = vDelRec[i];

        if (m_pPeerDao->SyncOverAlarmLevel(stRec, SYNC_DEL))
        {
            m_pDao->SyncOverAlarmLevel(stRec, SYNC_DEL);

            TaskLog(LOG_DETAIL, "sync alarm_level_cfg succ: %u %u %u", stRec.ulAlarmID, stRec.ulAlarmLevel, stRec.bIsIgnore);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync alarm_level_cfg failed: %u %u %u ", stRec.ulAlarmID, stRec.ulAlarmLevel, stRec.bIsIgnore);
            goto end;
        }
    }

end:
    if (m_pPeerDao->TransCommit())
    {
        m_pDao->TransCommit();
    }
    else
    {
        m_pDao->TransRollback();
    }
}

VOID TaskDBSync::SyncHistoryAlarm()
{
    VECTOR<ALARM_INFO_T>     vAddRec;
    VECTOR<ALARM_INFO_T>     vDelRec;

    if (!m_pDao->GetSyncingHistoryAlarm(g_ulSyncRecNum, vAddRec, vDelRec) ||
        (vAddRec.size() == 0 && vDelRec.size() == 0))
    {
        return;
    }

    m_pPeerDao->TransBegin();
    m_pDao->TransBegin();

    for (UINT32 i=0; i<vAddRec.size(); i++)
    {
        ALARM_INFO_T     &stRec = vAddRec[i];

        if (m_pPeerDao->SetHistoryAlarm(stRec, SYNC_NULL))
        {
            m_pDao->SyncOverHistoryAlarm(stRec, SYNC_ADD);

			TaskLog(LOG_DETAIL, "sync history_alarm succ: %s_%u_%u", stRec.acNeID, stRec.ulAlarmID, stRec.ulAlarmRaiseTime);
        }
        else
        {
			TaskLog(LOG_ERROR, "sync history_alarm failed: %s_%u_%u", stRec.acNeID, stRec.ulAlarmID, stRec.ulAlarmRaiseTime);
            goto end;
        }
    }

    for (UINT32 i=0; i<vDelRec.size(); i++)
    {
        ALARM_INFO_T     &stRec = vDelRec[i];

        if (m_pPeerDao->SyncOverHistoryAlarm(stRec, SYNC_DEL))
        {
            m_pDao->SyncOverHistoryAlarm(stRec, SYNC_DEL);

			TaskLog(LOG_DETAIL, "sync history_alarm succ: %s_%u_%u", stRec.acNeID, stRec.ulAlarmID, stRec.ulAlarmRaiseTime);
        }
        else
        {
			TaskLog(LOG_ERROR, "sync history_alarm failed: %s_%u_%u", stRec.acNeID, stRec.ulAlarmID, stRec.ulAlarmRaiseTime);
            goto end;
        }
    }

end:
    if (m_pPeerDao->TransCommit())
    {
        m_pDao->TransCommit();
    }
    else
    {
        m_pDao->TransRollback();
    }
}

VOID TaskDBSync::SyncHistoryAlarmStatistic()
{
    VECTOR<OMC_ALARM_STAT_T>     vAddRec;
    VECTOR<OMC_ALARM_STAT_T>     vDelRec;

    if (!m_pDao->GetSyncingHistoryAlarmStatistic(g_ulSyncRecNum, vAddRec, vDelRec) ||
        (vAddRec.size() == 0 && vDelRec.size() == 0))
    {
        return;
    }

    m_pPeerDao->TransBegin();
    m_pDao->TransBegin();

    for (UINT32 i=0; i<vAddRec.size(); i++)
    {
        OMC_ALARM_STAT_T     &stRec = vAddRec[i];

        if (m_pPeerDao->SetHistoryAlarmStatistic(stRec, SYNC_NULL))
        {
            m_pDao->SyncOverHistoryAlarmStatistic(stRec, SYNC_ADD);

			TaskLog(LOG_DETAIL, "sync history_alarm_statistic succ: %u", stRec.ulTime);
        }
        else
        {
			TaskLog(LOG_ERROR, "sync history_alarm_statistic failed: %u", stRec.ulTime);
            goto end;
        }
    }

    for (UINT32 i=0; i<vDelRec.size(); i++)
    {
        OMC_ALARM_STAT_T     &stRec = vDelRec[i];

        if (m_pPeerDao->SyncOverHistoryAlarmStatistic(stRec, SYNC_DEL))
        {
            m_pDao->SyncOverHistoryAlarmStatistic(stRec, SYNC_DEL);

			TaskLog(LOG_DETAIL, "sync history_alarm_statistic succ: %u", stRec.ulTime);
        }
        else
        {
			TaskLog(LOG_ERROR, "sync history_alarm_statistic failed: %u", stRec.ulTime);
            goto end;
        }
    }

end:
    if (m_pPeerDao->TransCommit())
    {
        m_pDao->TransCommit();
    }
    else
    {
        m_pDao->TransRollback();
    }
}

VOID TaskDBSync::SyncStation()
{
    VECTOR<STATION_INFO_T>      vAddRec;
    VECTOR<STATION_INFO_T>      vDelRec;

    if (!m_pDao->GetSyncingStationInfo(g_ulSyncRecNum, vAddRec, vDelRec) ||
        (vAddRec.size() == 0 && vDelRec.size() == 0))
    {
        return;
    }

    m_pPeerDao->TransBegin();
    m_pDao->TransBegin();

    for (UINT32 i=0; i<vAddRec.size(); i++)
    {
        STATION_INFO_T  &stRec = vAddRec[i];

        if (m_pPeerDao->SetStationInfo(stRec, SYNC_NULL))
        {
            m_pDao->SyncOverStationInfo(stRec, SYNC_ADD);

            TaskLog(LOG_DETAIL, "sync station succ: %s %u", stRec.acStationName, stRec.ulStationID);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync station failed: %s %u", stRec.acStationName, stRec.ulStationID);
            goto end;
        }
    }

    for (UINT32 i=0; i<vDelRec.size(); i++)
    {
        STATION_INFO_T      &stRec = vDelRec[i];

        if (m_pPeerDao->SyncOverStationInfo(stRec, SYNC_DEL))
        {
            m_pDao->SyncOverStationInfo(stRec, SYNC_DEL);

            TaskLog(LOG_DETAIL, "sync Station succ: %s %u", stRec.acStationName, stRec.ulStationID);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync Station failed: %s %u", stRec.acStationName, stRec.ulStationID);
            goto end;
        }
    }

end:
    if (m_pPeerDao->TransCommit())
    {
        m_pDao->TransCommit();
    }
    else
    {
        m_pDao->TransRollback();
    }
}

VOID TaskDBSync::SyncNEBasicInfo()
{
    VECTOR<NE_BASIC_INFO_T>      vAddRec;
    VECTOR<NE_BASIC_INFO_T>      vDelRec;

    if (!m_pDao->GetSyncingNEBasicInfo(g_ulSyncRecNum, vAddRec, vDelRec) ||
        (vAddRec.size() == 0 && vDelRec.size() == 0))
    {
        return;
    }

    m_pPeerDao->TransBegin();
    m_pDao->TransBegin();

    for (UINT32 i=0; i<vAddRec.size(); i++)
    {
        NE_BASIC_INFO_T  &stRec = vAddRec[i];

        if (m_pPeerDao->SetNeBasicInfo(stRec, SYNC_NULL) )
        {
            m_pDao->SyncOverNEBasicInfo(stRec, SYNC_ADD);

            TaskLog(LOG_DETAIL, "sync ne_basic_info succ: %s", stRec.acDevMac);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync ne_basic_info failed: %s", stRec.acDevMac);
            goto end;
        }
    }

    for (UINT32 i=0; i<vDelRec.size(); i++)
    {
        NE_BASIC_INFO_T      &stRec = vDelRec[i];

        if (m_pPeerDao->SyncOverNEBasicInfo(stRec, SYNC_DEL))
        {
            m_pDao->SyncOverNEBasicInfo(stRec, SYNC_DEL);

            TaskLog(LOG_DETAIL, "sync ne_basic_info succ: %s", stRec.acDevMac);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync ne_basic_info failed: %s", stRec.acDevMac);
            goto end;
        }
    }

end:
    if (m_pPeerDao->TransCommit())
    {
        m_pDao->TransCommit();
    }
    else
    {
        m_pDao->TransRollback();
    }
}

VOID TaskDBSync::SyncNEStatus()
{
    VECTOR<OMA_TX_STATUS_REC_T>      vAddRec;
    VECTOR<OMA_TX_STATUS_REC_T>      vDelRec;

    if (!m_pDao->GetSyncingTxStatus(g_ulSyncRecNum, vAddRec, vDelRec) ||
        (vAddRec.size() == 0 && vDelRec.size() == 0) )
    {
        return;
    }

    m_pPeerDao->TransBegin();
    m_pDao->TransBegin();

    for (UINT32 i=0; i<vAddRec.size(); i++)
    {
        OMA_TX_STATUS_REC_T  &stRec = vAddRec[i];

        if (m_pPeerDao->SetNEStatus(stRec, SYNC_NULL))
        {
            m_pDao->SyncOverTxStatus(stRec, SYNC_ADD);

            TaskLog(LOG_DETAIL, "sync ne_basic_status succ: %s %u", stRec.acNeID, stRec.ulTime);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync ne_basic_status failed: %s %u", stRec.acNeID, stRec.ulTime);
            goto end;
        }
    }

    for (UINT32 i=0; i<vDelRec.size(); i++)
    {
        OMA_TX_STATUS_REC_T      &stRec = vDelRec[i];

        if (m_pPeerDao->SyncOverTxStatus(stRec, SYNC_DEL))
        {
            m_pDao->SyncOverTxStatus(stRec, SYNC_DEL);

            TaskLog(LOG_DETAIL, "sync ne_basic_status succ: %s %u", stRec.acNeID, stRec.ulTime);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync ne_basic_status failed: %s %u", stRec.acNeID, stRec.ulTime);
            goto end;
        }
    }

end:
    if (m_pPeerDao->TransCommit())
    {
        m_pDao->TransCommit();
    }
    else
    {
        m_pDao->TransRollback();
    }
}
VOID TaskDBSync::SyncDevOperLog()
{

    VECTOR<OMA_OPERLOG_T>      vAddRec;
    VECTOR<OMA_OPERLOG_T>      vDelRec;

    if (!m_pDao->GetSyncingDevOperLog(g_ulSyncRecNum, vAddRec, vDelRec) ||
        (vAddRec.size() == 0 && vDelRec.size() == 0) )
    {
        return;
    }

    m_pPeerDao->TransBegin();
    m_pDao->TransBegin();

    for (UINT32 i=0; i<vAddRec.size(); i++)
    {
        OMA_OPERLOG_T  &stRec = vAddRec[i];

        if (m_pPeerDao->SetDevOperLog(stRec, SYNC_NULL))
        {
            m_pDao->SyncOverDevOperLog(stRec, SYNC_ADD);

            TaskLog(LOG_DETAIL, "sync dev_operlog succ: %s %s", stRec.acNeID, stRec.acLogInfo);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync dev_operlog failed: %s %s", stRec.acNeID, stRec.acLogInfo);
            goto end;
        }
    }

    for (UINT32 i=0; i<vDelRec.size(); i++)
    {
        OMA_OPERLOG_T      &stRec = vDelRec[i];

        if (m_pPeerDao->SyncOverDevOperLog(stRec, SYNC_DEL))
        {
            m_pDao->SyncOverDevOperLog(stRec, SYNC_DEL);

            TaskLog(LOG_DETAIL, "sync dev_operlog succ: %s %s", stRec.acNeID, stRec.acLogInfo);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync dev_operlog failed: %s %s", stRec.acNeID, stRec.acLogInfo);
            goto end;
        }
    }

end:
    if (m_pPeerDao->TransCommit())
    {
        m_pDao->TransCommit();
    }
    else
    {
        m_pDao->TransRollback();
    }
}

VOID TaskDBSync::SyncSwInfo()
{

    VECTOR<OMC_SW_INFO_T>      vAddRec;
    VECTOR<OMC_SW_INFO_T>      vDelRec;

    if (!m_pDao->GetSyncingSwInfo(g_ulSyncRecNum, vAddRec, vDelRec) ||
        (vAddRec.size() == 0 && vDelRec.size() == 0))
    {
        return;
    }

    m_pPeerDao->TransBegin();
    m_pDao->TransBegin();

    for (UINT32 i=0; i<vAddRec.size(); i++)
    {
        OMC_SW_INFO_T  &stRec = vAddRec[i];
		// TODO �����ļ������ݿ�ͬ������Ҫ��ͬ���ļ�Ȼ������Ӽ�¼���Զ����ݿ�
        if (m_pPeerDao->SetSwInfo(stRec, SYNC_NULL) )
        {
            m_pDao->SyncOverSwInfo(stRec, SYNC_ADD);
            TaskLog(LOG_DETAIL, "sync sw_info succ: %s", stRec.acSwVer);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync sw_info failed: %s", stRec.acSwVer);
            goto end;
        }
    }

    for (UINT32 i=0; i<vDelRec.size(); i++)
    {
        OMC_SW_INFO_T      &stRec = vDelRec[i];

        if (m_pPeerDao->SyncOverSwInfo(stRec, SYNC_DEL) )
        {
            m_pDao->SyncOverSwInfo(stRec, SYNC_DEL);

            TaskLog(LOG_DETAIL, "sync sw_info succ: %s", stRec.acSwVer);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync sw_info failed: %s", stRec.acSwVer);
            goto end;
        }
    }

end:
    if (m_pPeerDao->TransCommit())
    {
        m_pDao->TransCommit();
    }
    else
    {
        m_pDao->TransRollback();
    }
}

VOID TaskDBSync::SyncUpdateSwInfo()
{

    VECTOR<OMC_SW_UPDATE_INFO_T>      vAddRec;
    VECTOR<OMC_SW_UPDATE_INFO_T>      vDelRec;

    if (!m_pDao->GetSyncingUpdateSwInfo(g_ulSyncRecNum, vAddRec, vDelRec) ||
        (vAddRec.size() == 0 && vDelRec.size() == 0))
    {
        return;
    }

    m_pPeerDao->TransBegin();
    m_pDao->TransBegin();

    for (UINT32 i=0; i<vAddRec.size(); i++)
    {
        OMC_SW_UPDATE_INFO_T  &stRec = vAddRec[i];

        if (m_pPeerDao->SetUpdateSwInfo(stRec, SYNC_NULL))
        {
            m_pDao->SyncOverUpdateSwInfo(stRec, SYNC_ADD);

            TaskLog(LOG_DETAIL, "sync sw_update succ: %s %s %u", stRec.acNeID, stRec.acNewSwVer,stRec.bSetFlag);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync sw_update succ: %s %s %u", stRec.acNeID, stRec.acNewSwVer,stRec.bSetFlag);
            goto end;
        }
    }

    for (UINT32 i=0; i<vDelRec.size(); i++)
    {
        OMC_SW_UPDATE_INFO_T      &stRec = vDelRec[i];

        if (m_pPeerDao->SyncOverUpdateSwInfo(stRec, SYNC_DEL))
        {
            m_pDao->SyncOverUpdateSwInfo(stRec, SYNC_DEL);

            TaskLog(LOG_DETAIL, "sync sw_update succ: %s %s %u", stRec.acNeID, stRec.acNewSwVer,stRec.bSetFlag);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync sw_update succ: %s %s %u", stRec.acNeID, stRec.acNewSwVer,stRec.bSetFlag);
            goto end;
        }
    }

end:
    if (m_pPeerDao->TransCommit())
    {
        m_pDao->TransCommit();
    }
    else
    {
        m_pDao->TransRollback();
    }
}

VOID TaskDBSync::SyncDevFileName()
{

    VECTOR<OMA_FILE_INFO_T>      vAddRec;
    VECTOR<OMA_FILE_INFO_T>      vDelRec;

    if (!m_pDao->GetSyncingDevFile(g_ulSyncRecNum, vAddRec, vDelRec) ||
        (vAddRec.size() == 0 && vDelRec.size() == 0))
    {
        return;
    }

    m_pPeerDao->TransBegin();
    m_pDao->TransBegin();

    for (UINT32 i=0; i<vAddRec.size(); i++)
    {
        OMA_FILE_INFO_T  &stRec = vAddRec[i];
        CHAR    acFileName[256] = {0};
        CHAR    *szRawFile;
/*
        szRawFile = gos_right_strchr(stRec.acFileName, '/');
        if (!szRawFile)
        {
            szRawFile = stRec.acFileName;
        }
        else
        {
            szRawFile ++;
        }

        sprintf(acFileName, "%s/%s", g_acFtpLogFileDir, szRawFile);
        gos_format_path(acFileName);

        if (!gos_file_exist(acFileName))
        {

            if (!TransFile(g_stClusterCfg.aucPeerAddr,g_stLocalCfg.usTranFilePort,acFileName, acFileName))
            {
                vDelRec.push_back(stRec);
                TaskLog(LOG_ERROR, "sync rec_file failed: %s this File is not exist on both MRS servers", stRec.acFile);
            }
            else
            {
                if (m_pPeerDao->SyncOverRecFile(stRec, SYNC_ADD))
                {
                    m_pDao->SyncOverRecFile(stRec, SYNC_ADD);

                    TaskLog(LOG_DETAIL, "sync rec_file succ: %s", stRec.acFile);
                }
                else
                {
                    TaskLog(LOG_ERROR, "sync rec_file failed: %s", stRec.acFile);
                    goto end;
                }
            }
        }
        else
        {
            if (m_pPeerDao->SetRecFile(stRec, SYNC_ADD))
            {
                 m_pDao->SyncOverRecFile(stRec, SYNC_ADD);

                 TaskLog(LOG_DETAIL, "sync rec_file succ: %s", stRec.acFile);
            }
            else
            {
                 m_pDao->SyncOverRecFile(stRec, SYNC_ADD);
                 TaskLog(LOG_DETAIL, "sync rec_file faild: %s", stRec.acFile);
            }

        }
*/
        //����ļ��Ѿ����ص�������������ͬ���ļ���Ȼ�����޸����ݿ�
        if (stRec.bFileExist)
        {
            szRawFile = gos_right_strchr(stRec.acFileName, '/');
            if (!szRawFile)
            {
                szRawFile = stRec.acFileName;
            }
            else
            {
                szRawFile ++;
            }

            sprintf(acFileName, "%s/%s", g_stLocalCfg.acFtpLogFileDir, szRawFile);
            gos_format_path(acFileName);
            //�ж��ļ��ڱ����Ƿ����?
            //���������һ����¼����Զ����ݿ�SyncFlag����ΪSYNC_ADD
            //����������ӶԶ˻�ȡ�ļ�
            if (gos_file_exist(acFileName))
            {
                if (m_pPeerDao->SetDevFile(stRec, SYNC_ADD))
                {
                    m_pDao->SyncOverDevFile(stRec, SYNC_ADD);

                    TaskLog(LOG_DETAIL, "sync dev_file succ: %s %s", stRec.acNeID, stRec.acFileName);
                }
                else
                {
                    TaskLog(LOG_ERROR, "sync dev_file failed: %s %s", stRec.acNeID, stRec.acFileName);
                    goto end;
                }
            }
            else
            {
                // TransFile ������ͨ���ļ���������ȡ�ļ�������
                if (TransFile(g_aucClusterPeerAddr, g_stLocalCfg.usTransFilePort, acFileName, acFileName))
                {
                    if (m_pPeerDao->SetDevFile(stRec, SYNC_NULL))
                    {
                        m_pDao->SyncOverDevFile(stRec, SYNC_ADD);

                        TaskLog(LOG_DETAIL, "sync dev_file succ: %s %s", stRec.acNeID, stRec.acFileName);
                    }
                    else
                    {
                        TaskLog(LOG_ERROR, "sync dev_file failed: %s %s", stRec.acNeID, stRec.acFileName);
                        goto end;
                    }

                    TaskLog(LOG_INFO, "sync dev_file from perr_server succ: %s %s", stRec.acNeID, stRec.acFileName);
                }
                else
                {
                    TaskLog(LOG_ERROR, "sync dev_file from perr_server failed: %s %s", stRec.acNeID, stRec.acFileName);
                }
            }

        }
        else
        {
            if (m_pPeerDao->SetDevFile(stRec, SYNC_NULL))
            {
                m_pDao->SyncOverDevFile(stRec, SYNC_ADD);
                TaskLog(LOG_DETAIL, "sync dev_file_naem succ: %s %s", stRec.acNeID, stRec.acFileName);
            }
            else
            {
                TaskLog(LOG_ERROR, "sync dev_file failed: %s %s", stRec.acNeID, stRec.acFileName);
                goto end;
            }
        }

    }

    for (UINT32 i=0; i<vDelRec.size(); i++)
    {
        OMA_FILE_INFO_T      &stRec = vDelRec[i];

        if (m_pPeerDao->SyncOverDevFile(stRec, SYNC_DEL) )
        {
            m_pDao->SyncOverDevFile(stRec, SYNC_DEL);

            TaskLog(LOG_DETAIL, "sync dev_file succ: %s %s", stRec.acNeID, stRec.acFileName);
        }
        else
        {
            TaskLog(LOG_ERROR, "sync dev_file failed: %s %s", stRec.acNeID, stRec.acFileName);
            goto end;
        }
    }

end:
    if (m_pPeerDao->TransCommit())
    {
        m_pDao->TransCommit();
    }
    else
    {
        m_pDao->TransRollback();
    }
}

VOID TaskDBSync::SyncDevFile()
{

    VECTOR<OMA_FILE_INFO_T>      vPeerAddRec;
    VECTOR<OMA_FILE_INFO_T>      vPeerDelRec;

    if (!m_pPeerDao->GetSyncingDevFile(g_ulSyncRecNum, vPeerAddRec, vPeerDelRec) ||
        (vPeerAddRec.size() == 0 && vPeerDelRec.size() == 0))
    {
        return;
    }

    for (UINT32 i=0; i<vPeerAddRec.size(); i++)
    {
        OMA_FILE_INFO_T  &stRec = vPeerAddRec[i];
        CHAR    acFileName[256] = {0};
        CHAR    *szRawFile;

        //����ļ��Ѿ����ص�������������ͬ���ļ���Ȼ�����޸����ݿ�
        if (stRec.bFileExist)
        {
            szRawFile = gos_right_strchr(stRec.acFileName, '/');
            if (!szRawFile)
            {
                szRawFile = stRec.acFileName;
            }
            else
            {
                szRawFile ++;
            }

            sprintf(acFileName, "%s/%s", g_stLocalCfg.acFtpLogFileDir, szRawFile);
            gos_format_path(acFileName);

            if (!gos_file_exist(acFileName))
            {
                // TransFile ������ͨ���ļ���������ȡ�ļ�������
                if (TransFile(g_aucClusterPeerAddr, g_stLocalCfg.usTransFilePort, acFileName, acFileName))
                {
                    TaskLog(LOG_INFO, "sync dev_file from perr_server succ: %s %s", stRec.acNeID, stRec.acFileName);
                }
                else
                {
                    TaskLog(LOG_ERROR, "sync dev_file from perr_server failed: %s %s", stRec.acNeID, stRec.acFileName);
                }
            }

        }
    }

}

VOID TaskDBSync::OnTimerCheck(VOID *pvMsg, UINT32 ulMsgLen)
{
    if (!m_pDao->CheckConn())
    {
        TaskLog(LOG_FATAL, "check db failed");
        return;
    }

	/* 
	ͬ���澯��Ϣ�����ݿ�
	Ϊ����ͬʱ�����������������澯�������������ĸ澯�ڽ���֮����д���ļ���֮�������ݿ�ͬ���߳�д�����ݿ�	
	*/
    SyncAlarmRaise();
	SyncAlarmClear();

	ClearTable();
    // ÿ����������ı����ö����������һ�����������ݿ��ʱ��
    ClearNeBasicStatusTable();
    ClearTrafficStatusTable();

    if (!m_pPeerDao->CheckConn())
    {
        TaskLog(LOG_FATAL, "check peer db failed");
        return;
    }

    SyncOMTUser();
    SyncOperLog();
    SyncActiveAlarm();
    SyncHistoryAlarm();
	SyncHistoryAlarmStatistic();
    SyncStation();
    SyncAlarmLevel();
    SyncNEBasicInfo();
    SyncNEStatus();
    SyncDevOperLog();
    SyncSwInfo();
    SyncUpdateSwInfo();
    SyncDevFile();
	SyncDevFileName();
}

VOID TaskDBSync::TaskEntry(UINT16 usMsgID, VOID *pvMsg, UINT32 ulMsgLen)
{
    UINT32  ulTaskStatus = GetTaskStatus();

    switch(ulTaskStatus)
    {
    case TASK_STATUS_IDLE:
        switch(usMsgID)
        {
        case EV_TASK_INIT_REQ:
            SendRsp();
            SetTaskStatus(TASK_STATUS_INIT);
            SetTimer(TIMER_INIT, 0);
            break;

        default:
            break;
        }

        break;

    case TASK_STATUS_INIT:
        switch(usMsgID)
        {
        case EV_INIT_IND:
            if (!Init())
            {
                SetTimer(TIMER_INIT, 1000);
                TaskLog(LOG_FATAL, "Init failed!");
            }

            break;

        default:
            break;
        }

        break;

    case TASK_STATUS_WORK:
        switch(usMsgID)
        {
        case EV_CHECK_STATUS_TIMER:
            OnTimerCheck(pvMsg, ulMsgLen);
            break;

        default:
            TaskLog(LOG_ERROR, "unknown msg %u", usMsgID);
            break;
        }

        break;

    default:
        break;
    }
}
