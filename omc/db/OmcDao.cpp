#include "omc_public.h"

#define SYNC_ADD        1
#define SYNC_DEL        (-1)
#define SYNC_NULL       0
#define SYNC_UPDATE     SYNC_ADD

extern PDBEnvBase   *g_PDBEnv;
extern CHAR         g_acFtpLogFileDir[256];

VOID FormatSyncFlag(INT32 &iSyncFlag)
{
    if (iSyncFlag > 0)
    {
        iSyncFlag = gos_get_current_time();
    }
    else if (iSyncFlag < 0)
    {
        iSyncFlag = 0 - gos_get_current_time();
    }
}

OmcDao* GetOmcDao()
{
    return GetOmcDao(g_PDBEnv);
}

OmcDao* GetOmcDao(PDBEnvBase *pPDBEnv)
{
    OmcDao  *pInstance = NULL;

    pInstance = new OmcDao();

    if (!pInstance->Init(pPDBEnv))
    {
        delete pInstance;
        pInstance = NULL;
    }

    return pInstance;
}

BOOL OmcDao::Init(PDBEnvBase *pPDBEnv)
{
    if (!pPDBEnv)
    {
        return FALSE;
    }

    m_pPDBApp = new PDBApp(pPDBEnv);
    if (!m_pPDBApp)
    {
        return FALSE;
    }

    if (!m_pPDBApp->AllocConn())
    {
        DaoLog(LOG_ERROR, "alloc Conn fail");

        return FALSE;
    }

    return TRUE;
}

OmcDao::OmcDao()
{
    m_pPDBApp = NULL;
}

OmcDao::~OmcDao()
{
    if (m_pPDBApp)
    {
        delete m_pPDBApp;
    }
}

BOOL OmcDao::TransBegin()
{
    return m_pPDBApp->TransBegin();
}

BOOL OmcDao::TransRollback()
{
    return m_pPDBApp->TransRollback();
}

BOOL OmcDao::TransCommit()
{
    if (m_pPDBApp->TransCommit())
    {
        return TRUE;
    }

    m_pPDBApp->TransRollback();
    return FALSE;
}

BOOL OmcDao::CheckConn()
{
    INT32                   iRow = 0;
//  PDBRET                  enRet = PDB_ERROR;
    PDBRsltParam            Params;
    UINT32                  ulCount;

    Params.Bind(&ulCount);

    iRow = m_pPDBApp->Query("SELECT 0 FROM dual", Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query database failed");
        return FALSE;
    }

    m_pPDBApp->CloseQuery();

    return TRUE;
}

BOOL OmcDao::Update(const CHAR *szSQL)
{
    return m_pPDBApp->Update(szSQL) >= 0;
}

BOOL OmcDao::IsRecordExist(const CHAR *szTable, const CHAR *szKey, UINT32 ulKey)
{
    INT32                   iRow = 0;
    UINT32                  ulNum = 0;
    PDBRsltParam            Params;
    PDBRET                  enRet;
    CHAR                    acSQL[512];

    Params.Bind(&ulNum);
    sprintf(acSQL, "SELECT COUNT(*) FROM %s WHERE %s=%u", szTable, szKey, ulKey);
    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query %s failed", szTable);
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
    }

    m_pPDBApp->CloseQuery();

    return ulNum>0;
}

BOOL OmcDao::AddOperLog(UINT32 ulTime, CHAR *szUserName, CHAR *szLogInfo)
{
    OMT_OPER_LOG_INFO_T     stRec = {0};

    stRec.ulTime = ulTime;

    strncpy(stRec.acUserID, szUserName, sizeof(stRec.acUserID)-1);
    strncpy(stRec.acLogInfo, szLogInfo, sizeof(stRec.acLogInfo)-1);

    return SetOperLog(stRec, SYNC_ADD);
}

BOOL OmcDao::ClearOperLog(UINT32 ulTime)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&ulTime);

    iAffectedRow = m_pPDBApp->Update("DELETE FROM OperLog WHERE Time<?", Params);
    return iAffectedRow >= 0;
}

BOOL OmcDao::SetOperLog(OMT_OPER_LOG_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulTime);
    Params.BindString(stRec.acUserID);
    Params.BindString(stRec.acLogInfo);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO OperLog(SyncFlag, Time, UserID, LogInfo) VALUES(?,?,?,?) ", Params);
    return iAffectedRow > 0;
}

BOOL OmcDao::GetSyncingOperLog(UINT32 ulMaxRecNum, VECTOR<OMT_OPER_LOG_INFO_T> &vAddRec, VECTOR<OMT_OPER_LOG_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    OMT_OPER_LOG_INFO_T         stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[256];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulTime);
    Params.Bind(stRec.acUserID, sizeof(stRec.acUserID));
    Params.Bind(stRec.acLogInfo, sizeof(stRec.acLogInfo));

    sprintf(acSQL, "SELECT SyncFlag, Time, UserID, LogInfo FROM OperLog WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query OperLog failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        if (iSyncFlag > 0)
        {
            vAddRec.push_back(stRec);
        }
        else
        {
            vDelRec.push_back(stRec);
        }

        if ((vAddRec.size()+vDelRec.size()) >= ulMaxRecNum)
        {
            break;
        }

        memset(&stRec, 0, sizeof(stRec));
    }

    m_pPDBApp->CloseQuery();

    return PDB_ERROR != enRet;
}

BOOL OmcDao::SyncOverOperLog(OMT_OPER_LOG_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stRec.ulTime);
    Params.BindString(stRec.acUserID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE OperLog SET SyncFlag=0 WHERE Time=? AND UserID=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM OperLog WHERE Time=? AND UserID=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

INT32 OmcDao::GetRecNum(CHAR *szTable)
{
    INT32                   iRow = 0;
    UINT32                  ulNum = 0;
    PDBRsltParam            Params;
    PDBRET                  enRet;
    CHAR                    acSQL[512];

    Params.Bind(&ulNum);

    sprintf(acSQL, "SELECT COUNT(*) FROM %s", szTable);
    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query %s failed", szTable);
        return -1;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
    }

    m_pPDBApp->CloseQuery();

    return ulNum;
}

BOOL OmcDao::ClearTable(CHAR *szTable, CHAR *szFieldName, UINT32 ulMaxResvNum)
{
    CHAR        acSQL[1024];
    INT32       iDeleteNum = 0;
    INT32       iAffectedRow;

    iDeleteNum =  GetRecNum(szTable);
    if (iDeleteNum < 0)
    {
        DaoLog(LOG_ERROR, "get rec number of %s failed", szTable);
        return FALSE;
    }

    iDeleteNum -= ulMaxResvNum;
    if (iDeleteNum <= 0)
    {
        return TRUE;
    }

    sprintf(acSQL, "DELETE FROM %s ORDER BY %s LIMIT %d", szTable, szFieldName, iDeleteNum);

    iAffectedRow = m_pPDBApp->Update(acSQL);
    if (iAffectedRow < 0)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL OmcDao::AddOmtUserInfo(OMT_USER_INFO_T &stInfo)
{
    return SetOMTUser(stInfo, SYNC_ADD);
}

BOOL OmcDao::SetOMTUser(OMT_USER_INFO_T &stInfo, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.BindString(stInfo.acUser);
    Params.BindString(stInfo.acPwd);
    Params.Bind(&stInfo.ulUserType);

    Params.Bind(&iSyncFlag);
    Params.BindString(stInfo.acPwd);
    Params.Bind(&stInfo.ulUserType);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO omt_user(SyncFlag, User, Pwd, Type) VALUES(?,?,?,?) ON DUPLICATE KEY UPDATE SyncFlag=?, Pwd=?, Type=? ", Params);
    return iAffectedRow > 0;
}

BOOL OmcDao::DelOmtUserInfo(CHAR *pstInfo)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;
    INT32           iSyncFlag = SYNC_DEL;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.BindString(pstInfo);

    iAffectedRow = m_pPDBApp->Update("UPDATE omt_user SET SyncFlag=? WHERE User=?", Params);

    return iAffectedRow >= 0;
}

BOOL OmcDao::AlterOmtUserInfo (OMT_USER_INFO_T *pstInfo)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.BindString(pstInfo->acUser );
    Params.BindString(pstInfo->acPwd );
    Params.Bind(&pstInfo->ulUserType );

    Params.BindString(pstInfo->acPwd );
    Params.Bind(&pstInfo->ulUserType );

    iAffectedRow = m_pPDBApp->Update("INSERT INTO omt_user(User, Pwd, Type) VALUES(?,?,?) "
                                        "ON DUPLICATE KEY UPDATE Pwd=?, Type=?", Params);
    return iAffectedRow > 0;
}

BOOL OmcDao::GetUserInfo(VECTOR<OMT_USER_INFO_T> &vInfo)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    OMT_USER_INFO_T         stInfo = {0};

    Params.Bind(stInfo.acUser, sizeof(stInfo.acUser));
    Params.Bind(stInfo.acPwd, sizeof(stInfo.acPwd));
    Params.Bind(&stInfo.ulUserType);

    iRow = m_pPDBApp->Query("SELECT user, pwd, type FROM omt_user WHERE SyncFlag>=0", Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query omt_user failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        vInfo.push_back(stInfo);
        memset(&stInfo, 0, sizeof(stInfo));
    }

    m_pPDBApp->CloseQuery();

    return enRet != PDB_ERROR;
}

BOOL OmcDao::GetSyncingOMTUser(UINT32 ulMaxRecNum, VECTOR<OMT_USER_INFO_T> &vAddRec, VECTOR<OMT_USER_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    OMT_USER_INFO_T          stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];


    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(stRec.acUser, sizeof(stRec.acUser));
    Params.Bind(stRec.acPwd, sizeof(stRec.acPwd));
    Params.Bind(&stRec.ulUserType);

    sprintf(acSQL, "SELECT SyncFlag, User, Pwd, Type FROM omt_user WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query DCUser failed");
        return FALSE;
    }

    memset(&stRec, 0, sizeof(stRec));
    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        if (iSyncFlag > 0)
        {
            vAddRec.push_back(stRec);
        }
        else
        {
            vDelRec.push_back(stRec);
        }

        if ((vAddRec.size()+vDelRec.size()) >= ulMaxRecNum)
        {
            break;
        }

        memset(&stRec, 0, sizeof(stRec));
    }

    m_pPDBApp->CloseQuery();

    return PDB_ERROR != enRet;
}

BOOL OmcDao::SyncOverOMTUser(OMT_USER_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.BindString(stRec.acUser);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE omt_user SET SyncFlag=0 WHERE User=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM omt_user WHERE User=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

// 将网元的基本信息写入数据库
BOOL OmcDao::AddNeBasicInfo(NE_BASIC_INFO_T &stInfo)
{
    return SetNeBasicInfo(stInfo, SYNC_ADD);
}

BOOL OmcDao::SetNeBasicInfo(NE_BASIC_INFO_T &stInfo, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    // 参数绑定
    Params.BindString(stInfo.acNeID);
    Params.BindString(stInfo.acDevMac);
    Params.BindString(stInfo.acDevIp);
    Params.Bind(&stInfo.ulDevType);
    Params.BindString(stInfo.acDevName);
    Params.BindString(stInfo.acDevAlias);
    Params.BindString(stInfo.acSoftwareVer);
    Params.BindString(stInfo.acHardwareVer);
    Params.BindString(stInfo.acModelName);
    Params.Bind(&stInfo.ulLastOnlineTime);
    Params.BindString(stInfo.acRadioVer);
    Params.BindString(stInfo.acAndroidVer);
    Params.Bind(&stInfo.ulDevTC);
    Params.BindString(stInfo.acSIMIMSI);
    Params.Bind(&iSyncFlag);

    Params.BindString(stInfo.acDevMac);
    Params.BindString(stInfo.acDevIp);
    Params.Bind(&stInfo.ulDevType);
    Params.BindString(stInfo.acDevName);
    Params.BindString(stInfo.acDevAlias);
    Params.BindString(stInfo.acSoftwareVer);
    Params.BindString(stInfo.acHardwareVer);
    Params.BindString(stInfo.acModelName);
    Params.Bind(&stInfo.ulLastOnlineTime);
    Params.BindString(stInfo.acRadioVer);
    Params.BindString(stInfo.acAndroidVer);
    Params.Bind(&stInfo.ulDevTC);
    Params.BindString(stInfo.acSIMIMSI);
    Params.Bind(&iSyncFlag);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO ne_basic_info(ne_id, mac, ip, type, name, alias, sw_ver, hw_ver, model, online_time, radio_ver, android_ver, tc, sim_imsi, SyncFlag) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
                                     "ON DUPLICATE KEY UPDATE mac=?, ip=?, type=?, name=?, alias=?, sw_ver=?, hw_ver=?, model=?, online_time=?, radio_ver=?, android_ver=?, tc=?, sim_imsi=?, SyncFlag=?", Params);
    return iAffectedRow > 0;
}

BOOL OmcDao::GetSyncingNEBasicInfo(UINT32 ulMaxRecNum, VECTOR<NE_BASIC_INFO_T> &vAddRec, VECTOR<NE_BASIC_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    NE_BASIC_INFO_T          stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(stRec.acNeID, sizeof(stRec.acNeID));
    Params.Bind(stRec.acDevMac, sizeof(stRec.acDevMac));
    Params.Bind(stRec.acDevIp, sizeof(stRec.acDevIp));
    Params.Bind(&stRec.ulDevType);
    Params.Bind(stRec.acDevName, sizeof(stRec.acDevName));
    Params.Bind(stRec.acDevAlias, sizeof(stRec.acDevAlias));
    Params.Bind(stRec.acSoftwareVer,sizeof(stRec.acSoftwareVer));
    Params.Bind(stRec.acHardwareVer, sizeof(stRec.acHardwareVer));
    Params.Bind(stRec.acModelName, sizeof(stRec.acModelName));
    Params.Bind(stRec.acRadioVer, sizeof(stRec.acRadioVer));
    Params.Bind(stRec.acAndroidVer, sizeof(stRec.acAndroidVer));


    sprintf(acSQL, "SELECT SyncFlag, ne_id, mac, ip, type, name, alias, sw_ver, hw_ver, model, radio_ver, android_ver FROM ne_basic_info WHERE SyncFlag != 0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query ne_basic_info failed");
        return FALSE;
    }

    memset(&stRec, 0, sizeof(stRec));
    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        if (iSyncFlag > 0)
        {
            vAddRec.push_back(stRec);
        }
        else
        {
            vDelRec.push_back(stRec);
        }

        if ((vAddRec.size()+vDelRec.size()) >= ulMaxRecNum)
        {
            break;
        }

        memset(&stRec, 0, sizeof(stRec));
    }

    m_pPDBApp->CloseQuery();

    return PDB_ERROR != enRet;
}

BOOL OmcDao::SyncOverNEBasicInfo(NE_BASIC_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(stRec.acNeID, sizeof(stRec.acNeID));

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE ne_basic_info SET SyncFlag=0 WHERE ne_id=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM ne_basic_info WHERE ne_id=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL OmcDao::AddNe(NE_BASIC_INFO_T &stNEBasicInfo)
{
    return SetNe(stNEBasicInfo, SYNC_ADD);
}

BOOL OmcDao::SetNe(NE_BASIC_INFO_T &stNEBasicInfo, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.BindString(stNEBasicInfo.acNeID);
    Params.BindString(stNEBasicInfo.acDevMac);
    Params.Bind(&stNEBasicInfo.ulDevType);
    Params.BindString(stNEBasicInfo.acDevName);
    Params.Bind(&iSyncFlag);

    Params.BindString(stNEBasicInfo.acDevMac);
    Params.Bind(&stNEBasicInfo.ulDevType);
    Params.BindString(stNEBasicInfo.acDevName);
    Params.Bind(&iSyncFlag);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO ne_basic_info(ne_id, mac, type, name, SyncFlag) VALUES(?,?,?,?,?) "
        "ON DUPLICATE KEY UPDATE mac=?, type=?, name=?, SyncFlag=?", Params);
    return iAffectedRow > 0;
}

BOOL OmcDao::DelNe(CHAR *szNeID)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;
    INT32           iSyncFlag = SYNC_DEL;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.BindString(szNeID);

    iAffectedRow = m_pPDBApp->Update("UPDATE ne_basic_info SET SyncFlag=? WHERE ne_id=?", Params);

    return iAffectedRow >= 0;
}

BOOL OmcDao::GetSyncingNE(UINT32 ulMaxRecNum, VECTOR<NE_BASIC_INFO_T> &vAddRec, VECTOR<NE_BASIC_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    NE_BASIC_INFO_T          stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(stRec.acNeID, sizeof(stRec.acNeID));
    Params.Bind(stRec.acDevMac, sizeof(stRec.acDevMac));
    Params.Bind(&stRec.ulDevType);
    Params.Bind(stRec.acDevName, sizeof(stRec.acDevName));
    Params.Bind(&iSyncFlag);

    sprintf(acSQL, "SELECT ne_id, mac, ip, type, name, alias, sw_ver, hw_ver, model, radio_ver,android_ver, SyncFlag FROM ne_basic_info WHERE SyncFlag != 0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query Station failed");
        return FALSE;
    }

    memset(&stRec, 0, sizeof(stRec));
    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        if (iSyncFlag > 0)
        {
            vAddRec.push_back(stRec);
        }
        else
        {
            vDelRec.push_back(stRec);
        }

        if ((vAddRec.size()+vDelRec.size()) >= ulMaxRecNum)
        {
            break;
        }

        memset(&stRec, 0, sizeof(stRec));
    }

    m_pPDBApp->CloseQuery();

    return PDB_ERROR != enRet;
}

BOOL OmcDao::SyncOverNE(NE_BASIC_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(stRec.acNeID, sizeof(stRec.acNeID));

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE ne_basic_info SET SyncFlag=0 WHERE ne_id=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM ne_basic_info WHERE ne_id=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL OmcDao::GetNEBasicInfo(VECTOR<NE_BASIC_INFO_T> &vInfo)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    NE_BASIC_INFO_T         stInfo = {0};

    Params.Bind(stInfo.acNeID, sizeof(stInfo.acNeID));
    Params.Bind(stInfo.acDevMac, sizeof(stInfo.acDevMac));
    Params.Bind(stInfo.acDevIp, sizeof(stInfo.acDevIp));
    Params.Bind(&stInfo.ulDevType);
    Params.Bind(stInfo.acDevName, sizeof(stInfo.acDevName));
    Params.Bind(stInfo.acDevAlias, sizeof(stInfo.acDevAlias));
    Params.Bind(stInfo.acSoftwareVer, sizeof(stInfo.acSoftwareVer));
    Params.Bind(stInfo.acHardwareVer, sizeof(stInfo.acHardwareVer));
    Params.Bind(stInfo.acModelName, sizeof(stInfo.acModelName));
    Params.Bind(stInfo.acRadioVer,sizeof(stInfo.acRadioVer));
    Params.Bind(stInfo.acAndroidVer, sizeof(stInfo.acAndroidVer));

    iRow = m_pPDBApp->Query("SELECT ne_id, mac, ip, type, name, alias, sw_ver, hw_ver, model, radio_ver,android_ver FROM ne_basic_info WHERE SyncFlag>=0 ORDER BY type", Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query ne_basic_info failed");
        return FALSE;
    }

    memset(&stInfo, 0, sizeof(stInfo));
    UINT32 ulCount = 0;
    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        ulCount++;
         // 将获取到的网元基本信息插入容器
        vInfo.push_back(stInfo);
        memset(&stInfo, 0, sizeof(stInfo));
    }

    m_pPDBApp->CloseQuery();

    return enRet != PDB_ERROR;
}

BOOL OmcDao::GetActiveAlarm(VECTOR<ALARM_INFO_T> &vInfo)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    ALARM_INFO_T            stInfo = {0};

    Params.Bind(stInfo.acNeID, sizeof(stInfo.acNeID));
    Params.Bind(&stInfo.ulAlarmID);
    Params.Bind(&stInfo.ulAlarmRaiseTime);
    Params.Bind(&stInfo.ulAlarmType);
    Params.Bind(&stInfo.ulAlarmLevel);
    Params.Bind(stInfo.acAlarmTitle, sizeof(stInfo.acAlarmTitle));
    Params.Bind(stInfo.acAlarmInfo, sizeof(stInfo.acAlarmInfo));
    Params.Bind(stInfo.acAlarmReason, sizeof(stInfo.acAlarmReason));

    iRow = m_pPDBApp->Query("SELECT ne_id, alarm_id, alarm_time, alarm_type, alarm_level, alarm_title, alarm_info, alarm_reason FROM active_alarm WHERE SyncFlag >= 0", Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query active_alarm failed");
        return FALSE;
    }

    memset(&stInfo, 0, sizeof(stInfo));

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        if (stInfo.acAlarmInfo[0] == ' ')
        {
            gos_right_trim_string(stInfo.acAlarmInfo);
        }

        vInfo.push_back(stInfo);
        memset(&stInfo, 0, sizeof(stInfo));
    }

    m_pPDBApp->CloseQuery();

    return enRet != PDB_ERROR;
}

BOOL OmcDao::GetHistoryAlarm(UINT32 ulDevType, CHAR *szNeID, UINT32 ulStartTime, UINT32 ulEndTime, CHAR *szAlarmInfoKey, GJsonTupleParam &JsonTupleParam)
{
    //PROFILER();
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    ALARM_INFO_T            stInfo = {0};
    CHAR                    acSQL[1024];
    UINT32                  ulLen = 0;
    BOOL                    bWhere = FALSE;

    Params.Bind(stInfo.acNeID, sizeof(stInfo.acNeID));
    Params.Bind(&stInfo.ulAlarmID);
    Params.Bind(&stInfo.ulAlarmRaiseTime);
    Params.Bind(&stInfo.ulAlarmType);
    Params.Bind(&stInfo.ulAlarmLevel);
    Params.Bind(stInfo.acAlarmTitle, sizeof(stInfo.acAlarmTitle));
    Params.Bind(stInfo.acAlarmInfo, sizeof(stInfo.acAlarmInfo));
    Params.Bind(stInfo.acAlarmReason, sizeof(stInfo.acAlarmReason));
    Params.Bind(&stInfo.ulAlarmClearTime);
    Params.Bind(stInfo.acAlarmClearInfo, sizeof(stInfo.acAlarmClearInfo));

    if (*szNeID == '\0')
    {
        ulLen = sprintf(acSQL, "SELECT ne_id, alarm_id, alarm_time, alarm_type, alarm_level, alarm_title, alarm_info, alarm_reason, alarm_clear_time, alarm_clear_info FROM history_alarm");
        if (ulDevType != OMC_DEV_TYPE_ALL)
        {
            ulLen = sprintf(acSQL, "SELECT a.ne_id, a.alarm_id, a.alarm_time, a.alarm_type, a.alarm_level, a.alarm_title, a.alarm_info, a.alarm_reason, a.alarm_clear_time, a.alarm_clear_info FROM history_alarm a, ne_basic_info b "
                                "WHERE a.ne_id=b.ne_id AND b.type=%u", ulDevType);
            bWhere = TRUE;
        }
    }
    else
    {
        ulLen = sprintf(acSQL, "SELECT ne_id, alarm_id, alarm_time, alarm_type, alarm_level, alarm_title, alarm_info, alarm_reason, alarm_clear_time, alarm_clear_info FROM history_alarm "
                            "WHERE ne_id='%s'", szNeID);
        bWhere = TRUE;
    }

    ulLen += sprintf(acSQL+ulLen, " %s alarm_time>=%u AND alarm_time<=%u", bWhere?"AND":"WHERE", ulStartTime, ulEndTime);
    if (*szAlarmInfoKey != '\0')
    {
        ulLen += sprintf(acSQL+ulLen, " AND alarm_title LIKE '%c%s%c'", '%', szAlarmInfoKey, '%');
    }

    ulLen += sprintf(acSQL+ulLen, " ORDER BY alarm_time LIMIT 400");

    iRow = m_pPDBApp->Query(acSQL, Params);
    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query history_alarm failed");
        return FALSE;
    }

    memset(&stInfo, 0, sizeof(stInfo));


    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        if (stInfo.acAlarmInfo[0] == ' ')
        {
            gos_right_trim_string(stInfo.acAlarmInfo);
        }

        GJsonParam      JsonParam;

        JsonParam.Add("ne_id", stInfo.acNeID);
        JsonParam.Add("alarm_id", stInfo.ulAlarmID);
        JsonParam.Add("level", stInfo.ulAlarmLevel);
        JsonParam.Add("type", stInfo.ulAlarmType);
        JsonParam.Add("reason", stInfo.acAlarmReason);
        JsonParam.Add("time", stInfo.ulAlarmRaiseTime);
        JsonParam.Add("info", stInfo.acAlarmInfo[0]?stInfo.acAlarmInfo:stInfo.acAlarmTitle);
        JsonParam.Add("clear_time", stInfo.ulAlarmClearTime);
        JsonParam.Add("clear_info", stInfo.acAlarmClearInfo);

        JsonTupleParam.Add(JsonParam);

        if (JsonTupleParam.GetStringSize() >= 60*1024)
        {
            break;
        }

        memset(&stInfo, 0, sizeof(stInfo));
    }

    m_pPDBApp->CloseQuery();


    return enRet != PDB_ERROR;
}

// 更新历史告警统计表
BOOL OmcDao::UpdateHistoryStatistics(UINT32 ulAlarmRaiseTime, UINT32 ulAlarmLevel)
{
    CHAR                    acSQL[1024] = {0};
    INT32                   iRow = 0;
    INT32                   iSyncFlag = SYNC_ADD;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBParam                Params;
    OMC_ALARM_STAT_T        stAlarmStat = {0};

    FormatSyncFlag(iSyncFlag);
    // 判断更新的告警等级，只对相应告警等级的列进行增加操作
    // 先进行插入操作，如果主键冲突则说明已存在记录，则进行更新某一列数量操作。
    if(ulAlarmLevel == ALARM_LEVEL_CRITICAL)
    {
        stAlarmStat.ulCriticalAlarmNum = 1;
        sprintf(acSQL, "INSERT INTO history_alarm_statistic(time, critical_alarm, main_alarm, minor_alarm, normal_alarm, SyncFlag) VALUES(?,?,?,?,?,?)"
                       "ON DUPLICATE KEY UPDATE  critical_alarm = critical_alarm + 1, SyncFlag=?");
    }
    else if(ulAlarmLevel == ALARM_LEVEL_MAIN)
    {
        stAlarmStat.ulMainAlarmNum = 1;
        sprintf(acSQL, "INSERT INTO history_alarm_statistic(time, critical_alarm, main_alarm, minor_alarm, normal_alarm, SyncFlag) VALUES(?,?,?,?,?,?)"
                        "ON DUPLICATE KEY UPDATE  main_alarm = main_alarm + 1, SyncFlag=?");
    }
    else if(ulAlarmLevel == ALARM_LEVEL_MINOR)
    {
        stAlarmStat.ulMinorAlarmNum = 1;
        sprintf(acSQL, "INSERT INTO history_alarm_statistic(time, critical_alarm, main_alarm, minor_alarm, normal_alarm, SyncFlag) VALUES(?,?,?,?,?,?)"
                       "ON DUPLICATE KEY UPDATE  minor_alarm = minor_alarm + 1, SyncFlag=?");
    }
    else if(ulAlarmLevel == ALARM_LEVEL_NORMAL)
    {
        stAlarmStat.ulNormalAlarmNum = 1;
        sprintf(acSQL, "INSERT INTO history_alarm_statistic(time, critical_alarm, main_alarm, minor_alarm, normal_alarm, SyncFlag) VALUES(?,?,?,?,?,?)"
                       "ON DUPLICATE KEY UPDATE  normal_alarm = normal_alarm + 1, SyncFlag=?");
    }
    else 
    {
        GosLog(LOG_ERROR, "unknow alarm_level(%u)", ulAlarmLevel);
        return FALSE;
    }

    // 获取到告警产生时间的零点时间戳
    stAlarmStat.ulTime = gos_get_day_from_time(ulAlarmRaiseTime);

    Params.Bind(&stAlarmStat.ulTime);
    Params.Bind(&stAlarmStat.ulCriticalAlarmNum);
    Params.Bind(&stAlarmStat.ulMainAlarmNum);
    Params.Bind(&stAlarmStat.ulMinorAlarmNum);
    Params.Bind(&stAlarmStat.ulNormalAlarmNum);
    Params.Bind(&iSyncFlag);

    Params.Bind(&iSyncFlag);

    iRow = m_pPDBApp->Update(acSQL, Params);
    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query history_alarm_statistic failed");
        return FALSE;
    }

    return enRet != PDB_ERROR;
}

BOOL OmcDao::CheckHistoryAlarmStatistic(UINT32 ulHistoryAlarmStatisticDay)
{
    PDBRsltParam        RsltParams;
    PDBParam            Params;
    PDBRET              enRet = PDB_CONTINUE;
    INT32               iRow = -1;
    INT32               iSyncFlag = SYNC_ADD;
    CHAR                acSQL[1024] = {0};
    UINT32              ulCurrTimeStamp = 0;
    UINT32              ulStartTime = 0;
    UINT32              ulAlarmSum = 0;
    UINT32              ulAlarmLevel = 0;
    OMC_ALARM_STAT_T    stAlarmStat = {0};

    FormatSyncFlag(iSyncFlag);

    // 历史告警统计开始日期的零点时间戳
    ulCurrTimeStamp = gos_get_day_from_time(gos_get_current_time());    // 当天零点时间戳
    ulStartTime = ulCurrTimeStamp - ulHistoryAlarmStatisticDay*86400;     // 检查历史告警统计的开始时间
    while(stAlarmStat.ulTime < ulCurrTimeStamp)
    {
        memset(&stAlarmStat, 0, sizeof(stAlarmStat));
        stAlarmStat.ulTime = ulStartTime;
        sprintf(acSQL, " SELECT time FROM history_alarm_statistic WHERE time=%u", stAlarmStat.ulTime);

        UINT32      ulAlarmDay=0;   // 查询当天是否已有历史告警统计记录
        RsltParams.Clear();
        RsltParams.Bind(&ulAlarmDay);

        iRow = m_pPDBApp->Query(acSQL,RsltParams);
        if (INVALID_ROWCOUNT == iRow)   // 查询失败
        {
            DaoLog(LOG_ERROR, "CheckHistoryAlarmStatistic: query history_alarm_statistic failed");
            return FALSE;
        }
        // 获取查询结果
        FOR_ROW_FETCH(enRet, m_pPDBApp)
        {   
        }
        m_pPDBApp->CloseQuery();

        //判断查询结果，如果不存在则进行插入操作，若存在则不做处理
        if (ulAlarmDay == 0)
        {
            RsltParams.Clear();
            RsltParams.Bind(&ulAlarmLevel);
            RsltParams.Bind(&ulAlarmSum);
            // 从历史告警表统计一天内不同告警等级的数量
            sprintf(acSQL, "SELECT alarm_level, COUNT(*) FROM history_alarm WHERE alarm_time>=%u  AND alarm_time < %u GROUP BY alarm_level", stAlarmStat.ulTime, stAlarmStat.ulTime + 86400);
            iRow = m_pPDBApp->Query(acSQL, RsltParams);
            if (INVALID_ROWCOUNT == iRow)
            {
                DaoLog(LOG_ERROR, "query history_alarm_statistic failed");
                return FALSE;
            }
            // 处理查询结果
            FOR_ROW_FETCH(enRet, m_pPDBApp)
            {
                if (ulAlarmLevel == ALARM_LEVEL_CRITICAL)
                {
                    stAlarmStat.ulCriticalAlarmNum = ulAlarmSum;
                }
                else if (ulAlarmLevel == ALARM_LEVEL_MAIN)
                {
                    stAlarmStat.ulMainAlarmNum = ulAlarmSum;
                }
                else if (ulAlarmLevel == ALARM_LEVEL_MINOR)
                {
                    stAlarmStat.ulMinorAlarmNum = ulAlarmSum;
                }
                else if (ulAlarmLevel == ALARM_LEVEL_NORMAL)
                {
                    stAlarmStat.ulNormalAlarmNum = ulAlarmSum;
                }
                else
                {
                    DaoLog(LOG_ERROR, "CheckHistoryStatistic error alarm_level(%u)", ulAlarmLevel);
                }
            }
            m_pPDBApp->CloseQuery();
            // 将结果插入历史告警统计表，若存在则进行更新操作
            Params.Clear();
            Params.Bind(&stAlarmStat.ulTime);
            Params.Bind(&stAlarmStat.ulCriticalAlarmNum);
            Params.Bind(&stAlarmStat.ulMainAlarmNum);
            Params.Bind(&stAlarmStat.ulMinorAlarmNum);
            Params.Bind(&stAlarmStat.ulNormalAlarmNum);
            Params.Bind(&iSyncFlag);

            Params.Bind(&stAlarmStat.ulCriticalAlarmNum);
            Params.Bind(&stAlarmStat.ulMainAlarmNum);
            Params.Bind(&stAlarmStat.ulMinorAlarmNum);
            Params.Bind(&stAlarmStat.ulNormalAlarmNum);
            Params.Bind(&iSyncFlag);

            iRow = m_pPDBApp->Update(" INSERT INTO history_alarm_statistic(time, critical_alarm, main_alarm, minor_alarm, normal_alarm, SyncFlag) VALUES (?,?,?,?,?,?) " 
                                     " ON DUPLICATE KEY UPDATE critical_alarm=?,main_alarm=?,minor_alarm=?,normal_alarm=?, SyncFlag=?", Params);
            if (INVALID_ROWCOUNT == iRow)
            {
                DaoLog(LOG_ERROR, "update history_alarm_statistic failed");
                return FALSE;
            }
        }

        ulStartTime += 86400; //增加一天
    }

    return TRUE;
}

BOOL OmcDao::GetHistoryAlarmTotalCount(UINT32 ulBeginTime, UINT32 ulEndTime, OMC_ALARM_STAT_T &stHistoryAlarmStat)
{
    PDBRsltParam        Params;
    PDBRET              enRet = PDB_CONTINUE;
    INT32               iRow = -1;
    CHAR                acSQL[1024] = {0};
    UINT32              ulCriticalAlarmSum = 0;
    UINT32              ulMainAlarmSum = 0;
    UINT32              ulMinorAlarmSum = 0;
    UINT32              ulNormalAlarmSum = 0;

    Params.Bind(&ulCriticalAlarmSum);
    Params.Bind(&ulMainAlarmSum);
    Params.Bind(&ulMinorAlarmSum);
    Params.Bind(&ulNormalAlarmSum);

    sprintf(acSQL, " SELECT SUM(critical_alarm) AS critical_alarm_sum, SUM(main_alarm) AS main_alarm_sum, SUM(minor_alarm) AS minor_alarm_sum, SUM(normal_alarm) AS normal_alarm_sum "
                   " FROM history_alarm_statistic WHERE time <= %u AND time > %u ", ulEndTime, ulBeginTime);
    iRow = m_pPDBApp->Query(acSQL, Params);
    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query history_alarm_statistic failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        stHistoryAlarmStat.ulCriticalAlarmNum = ulCriticalAlarmSum;
        stHistoryAlarmStat.ulMainAlarmNum = ulMainAlarmSum;
        stHistoryAlarmStat.ulMinorAlarmNum = ulMinorAlarmSum;
        stHistoryAlarmStat.ulNormalAlarmNum = ulNormalAlarmSum;      
    }

    m_pPDBApp->CloseQuery();

    return enRet != PDB_ERROR;
}

BOOL OmcDao::GetHistoryAlarmDayCount(UINT32 ulBegainTime, UINT32 ulEndTime, VECTOR<OMC_ALARM_STAT_T> &vec)
{
    PDBRsltParam        Params;
    CHAR                acSQL[1024] = {0};
    PDBRET              enRet = PDB_CONTINUE;
    OMC_ALARM_STAT_T    stAlarmStat = {0};

    Params.Bind(&stAlarmStat.ulTime);
    Params.Bind(&stAlarmStat.ulCriticalAlarmNum);
    Params.Bind(&stAlarmStat.ulMainAlarmNum);
    Params.Bind(&stAlarmStat.ulMinorAlarmNum);
    Params.Bind(&stAlarmStat.ulNormalAlarmNum);

    sprintf(acSQL, "SELECT time, critical_alarm, main_alarm, minor_alarm, normal_alarm FROM history_alarm_statistic WHERE time > " 
                   " %u  AND time <= %u ORDER BY time",ulBegainTime, ulEndTime);

    INT32 iRow = m_pPDBApp->Query(acSQL, Params);
    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query history_alarm failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        vec.push_back(stAlarmStat);
    }

    m_pPDBApp->CloseQuery();

    return enRet != PDB_ERROR;
}

BOOL OmcDao::GetHistoryAlarmStat(UINT32 ulHistoryAlarmDay, OMC_ALARM_STAT_T &stAlarmStat, OMC_ALARM_TREND_T &stAlarmTrend)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    CHAR                    acSQL[1024];
    INT32                   iTimeZone = gos_get_timezone();
    UINT32                  ulAlarmDay;
    UINT32                  ulAlarmNum;
    UINT32                  ulAlarmLevel;
    UINT32                  ulCurrTime = gos_get_current_time();
    UINT32                  ulToday;
    UINT32                  ulStartTime;
    INT32                   iIndex;

    // 统计90天内的告警数据
    if (ulHistoryAlarmDay > ARRAY_SIZE(stAlarmTrend.astAlarmTrend))
    {
        ulHistoryAlarmDay = ARRAY_SIZE(stAlarmTrend.astAlarmTrend);
    }

    ulToday = (ulCurrTime - iTimeZone)/86400;
    ulStartTime = ulCurrTime - ulHistoryAlarmDay*86400;

    memset(&stAlarmStat, 0, sizeof(stAlarmStat));
    memset(&stAlarmTrend, 0, sizeof(stAlarmTrend));

    Params.Bind(&ulAlarmDay);
    Params.Bind(&ulAlarmLevel);
    Params.Bind(&ulAlarmNum);

    sprintf(acSQL, " SELECT FLOOR((alarm_time-%d)/86400) AS AlarmDay, alarm_level, COUNT(*) FROM history_alarm "
                   " WHERE alarm_time>=%u GROUP BY AlarmDay, alarm_level ORDER BY AlarmDay ", iTimeZone, ulStartTime);

    iRow = m_pPDBApp->Query(acSQL, Params);
    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query history_alarm failed");
        return FALSE;
    }
    // 统计每天不同等级告警的数量，所有不同等级告警的数量
    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        iIndex = ulToday - ulAlarmDay;
        if (iIndex >= 0 &&
            iIndex < ulHistoryAlarmDay)
        {
            if (ulAlarmLevel == ALARM_LEVEL_CRITICAL)
            {
                stAlarmTrend.astAlarmTrend[iIndex].ulCriticalAlarmNum = ulAlarmNum;
                stAlarmStat.ulCriticalAlarmNum += ulAlarmNum;
            }
            else if (ulAlarmLevel == ALARM_LEVEL_MAIN)
            {
                stAlarmTrend.astAlarmTrend[iIndex].ulMainAlarmNum = ulAlarmNum;
                stAlarmStat.ulMainAlarmNum += ulAlarmNum;
            }
            else if (ulAlarmLevel == ALARM_LEVEL_MINOR)
            {
                stAlarmTrend.astAlarmTrend[iIndex].ulMinorAlarmNum = ulAlarmNum;
                stAlarmStat.ulMinorAlarmNum += ulAlarmNum;
            }
            else if (ulAlarmLevel == ALARM_LEVEL_NORMAL)
            {
                stAlarmTrend.astAlarmTrend[iIndex].ulNormalAlarmNum = ulAlarmNum;
                stAlarmStat.ulNormalAlarmNum += ulAlarmNum;
            }
            
        }
    }

    stAlarmTrend.ulNum = ulHistoryAlarmDay;
    for (UINT32 i=0; i<ulHistoryAlarmDay; i++)
    {
        stAlarmTrend.astAlarmTrend[i].ulTime = ulCurrTime - 86400*(ulHistoryAlarmDay-1-i);
    }

    m_pPDBApp->CloseQuery();

    return enRet != PDB_ERROR;
}

BOOL OmcDao::GetSyncingHistoryAlarmStatistic(UINT32 ulMaxRecNum, VECTOR<OMC_ALARM_STAT_T> &vAddRec, VECTOR<OMC_ALARM_STAT_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    OMC_ALARM_STAT_T        stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[256];

    vAddRec.clear();
    vDelRec.clear();

	Params.Bind(&stRec.ulTime);
	Params.Bind(&stRec.ulCriticalAlarmNum);
	Params.Bind(&stRec.ulMainAlarmNum);
	Params.Bind(&stRec.ulMinorAlarmNum);
	Params.Bind(&stRec.ulNormalAlarmNum);
    Params.Bind(&iSyncFlag);

    sprintf(acSQL, "SELECT time, critical_alarm, main_alarm, minor_alarm, normal_alarm, SyncFlag FROM history_alarm_statistic WHERE SyncFlag != 0");

    iRow = m_pPDBApp->Query(acSQL, Params);
    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query history_alarm_statistic failed");
        return FALSE;
    }

    memset(&stRec, 0, sizeof(stRec));
    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        if (iSyncFlag > 0)
        {
            vAddRec.push_back(stRec);
        }
        else
        {
            vDelRec.push_back(stRec);
        }

        if ((vAddRec.size()+vDelRec.size()) >= ulMaxRecNum)
        {
            break;
        }

        memset(&stRec, 0, sizeof(stRec));
    }

    m_pPDBApp->CloseQuery();

    return PDB_ERROR != enRet;
}

BOOL OmcDao::SetHistoryAlarmStatistic(OMC_ALARM_STAT_T &stInfo, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

	Params.Bind(&stInfo.ulTime);
	Params.Bind(&stInfo.ulCriticalAlarmNum);
	Params.Bind(&stInfo.ulMainAlarmNum);
	Params.Bind(&stInfo.ulMinorAlarmNum);
	Params.Bind(&stInfo.ulNormalAlarmNum);
    Params.Bind(&iSyncFlag);

	Params.Bind(&stInfo.ulCriticalAlarmNum);
	Params.Bind(&stInfo.ulMainAlarmNum);
	Params.Bind(&stInfo.ulMinorAlarmNum);
	Params.Bind(&stInfo.ulNormalAlarmNum);
    Params.Bind(&iSyncFlag);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO history_alarm_statistic(time, critical_alarm, main_alarm, minor_alarm, normal_alarm, SyncFlag) VALUES(?,?,?,?,?,?) "
									 "ON DUPLICATE KEY UPDATE critical_alarm=?, main_alarm=?, minor_alarm=?, normal_alarm=?, SyncFlag=?", Params);
    return iAffectedRow > 0;
}

BOOL OmcDao::SyncOverHistoryAlarmStatistic(OMC_ALARM_STAT_T &stInfo, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

	Params.Bind(&stInfo.ulTime);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE history_alarm_statistic SET SyncFlag=0 WHERE time=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {

        iAffectedRow = m_pPDBApp->Update("DELETE FROM history_alarm_statistic WHERE time=?", Params);
        if(iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL OmcDao::AddActiveAlarm(ALARM_INFO_T &stInfo)
{
    return SetActiveAlarm(stInfo, SYNC_ADD);
}

BOOL OmcDao::SetActiveAlarm(ALARM_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.BindString(stRec.acNeID);
    Params.Bind(&stRec.ulAlarmID);
    Params.Bind(&stRec.ulAlarmRaiseTime);
    Params.Bind(&stRec.ulAlarmType);
    Params.Bind(&stRec.ulAlarmLevel);
    Params.BindString(stRec.acAlarmTitle);
    Params.BindString(stRec.acAlarmInfo);
    Params.BindString(stRec.acAlarmReason);
    Params.Bind(&iSyncFlag);

    Params.Bind(&stRec.ulAlarmRaiseTime);
    Params.Bind(&stRec.ulAlarmType);
    Params.Bind(&stRec.ulAlarmLevel);
    Params.BindString(stRec.acAlarmTitle);
    Params.BindString(stRec.acAlarmInfo);
    Params.BindString(stRec.acAlarmReason);
    Params.Bind(&iSyncFlag);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO active_alarm(ne_id, alarm_id, alarm_time, alarm_type, alarm_level, alarm_title, alarm_info, alarm_reason, SyncFlag) VALUES(?,?,?,?,?,?,?,?,?) "
        "ON DUPLICATE KEY UPDATE alarm_time=?, alarm_type=?, alarm_level=?, alarm_title=?, alarm_info=?, alarm_reason=?, SyncFlag=?", Params);
    return iAffectedRow > 0;
}

BOOL OmcDao::GetSyncingActiveAlarm(UINT32 ulMaxRecNum, VECTOR<ALARM_INFO_T> &vAddRec, VECTOR<ALARM_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    ALARM_INFO_T            stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[256];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(stRec.acNeID, sizeof(stRec.acNeID));
    Params.Bind(&stRec.ulAlarmID);
    Params.Bind(&stRec.ulAlarmRaiseTime);
    Params.Bind(&stRec.ulAlarmType);
    Params.Bind(&stRec.ulAlarmLevel);
    Params.Bind(stRec.acAlarmTitle, sizeof(stRec.acAlarmTitle));
    Params.Bind(stRec.acAlarmInfo, sizeof(stRec.acAlarmInfo));
    Params.Bind(stRec.acAlarmReason, sizeof(stRec.acAlarmReason));
    Params.Bind(&iSyncFlag);

    sprintf(acSQL, "SELECT ne_id, alarm_id, alarm_time, alarm_type, alarm_level, alarm_title, alarm_info, alarm_reason, SyncFlag FROM active_alarm WHERE SyncFlag != 0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query active_alarm failed");
        return FALSE;
    }

    memset(&stRec, 0, sizeof(stRec));
    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        if (iSyncFlag > 0)
        {
            vAddRec.push_back(stRec);
        }
        else
        {
            vDelRec.push_back(stRec);
        }

        if ((vAddRec.size()+vDelRec.size()) >= ulMaxRecNum)
        {
            break;
        }

        memset(&stRec, 0, sizeof(stRec));
    }

    m_pPDBApp->CloseQuery();

    return PDB_ERROR != enRet;
}


BOOL OmcDao::SyncOverActiveAlarm(ALARM_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.BindString(stRec.acNeID);
    Params.Bind(&stRec.ulAlarmID);


    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE active_alarm SET SyncFlag=0 WHERE ne_id=? AND alarm_id=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM active_alarm where ne_id=? AND alarm_id=?", Params);
        if(iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL OmcDao::AddHistoryAlarm(ALARM_INFO_T &stInfo)
{
    return SetHistoryAlarm(stInfo, SYNC_ADD);
}

BOOL OmcDao::SetHistoryAlarm(ALARM_INFO_T &stInfo, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.BindString(stInfo.acNeID);
    Params.Bind(&stInfo.ulAlarmID);
    Params.Bind(&stInfo.ulAlarmRaiseTime);
    Params.Bind(&stInfo.ulAlarmType);
    Params.Bind(&stInfo.ulAlarmLevel);
    Params.BindString(stInfo.acAlarmTitle);
    Params.BindString(stInfo.acAlarmInfo);
    Params.BindString(stInfo.acAlarmReason);
    Params.Bind(&stInfo.ulAlarmClearTime);
    Params.Bind(&iSyncFlag);

    Params.Bind(&stInfo.ulAlarmRaiseTime);
    Params.Bind(&stInfo.ulAlarmType);
    Params.Bind(&stInfo.ulAlarmLevel);
    Params.BindString(stInfo.acAlarmTitle);
    Params.BindString(stInfo.acAlarmInfo);
    Params.BindString(stInfo.acAlarmReason);
    Params.Bind(&stInfo.ulAlarmClearTime);
    Params.Bind(&iSyncFlag);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO history_alarm(ne_id, alarm_id, alarm_time, alarm_type, alarm_level, alarm_title, alarm_info, alarm_reason, alarm_clear_time, SyncFlag) VALUES(?,?,?,?,?,?,?,?,?,?) "
        "ON DUPLICATE KEY UPDATE alarm_time=?, alarm_type=?, alarm_level=?, alarm_title=?, alarm_info=?, alarm_reason=?,alarm_clear_time=?, SyncFlag=?", Params);
    return iAffectedRow > 0;
}

BOOL OmcDao::GetSyncingHistoryAlarm(UINT32 ulMaxRecNum, VECTOR<ALARM_INFO_T> &vAddRec, VECTOR<ALARM_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    ALARM_INFO_T            stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[256];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(stRec.acNeID, sizeof(stRec.acNeID));
    Params.Bind(&stRec.ulAlarmID);
    Params.Bind(&stRec.ulAlarmRaiseTime);
    Params.Bind(&stRec.ulAlarmType);
    Params.Bind(&stRec.ulAlarmLevel);
    Params.Bind(stRec.acAlarmTitle, sizeof(stRec.acAlarmTitle));
    Params.Bind(stRec.acAlarmInfo, sizeof(stRec.acAlarmInfo));
    Params.Bind(stRec.acAlarmReason, sizeof(stRec.acAlarmReason));
    Params.Bind(&stRec.ulAlarmReasonID);
    Params.Bind(&stRec.ulAlarmClearTime);
    Params.Bind(&iSyncFlag);

    sprintf(acSQL, "SELECT ne_id, alarm_id, alarm_time, alarm_type, alarm_level, alarm_title, alarm_info, alarm_reason, alarm_reason_id, alarm_clear_time, SyncFlag FROM history_alarm WHERE SyncFlag != 0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query active_alarm failed");
        return FALSE;
    }

    memset(&stRec, 0, sizeof(stRec));
    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        if (iSyncFlag > 0)
        {
            vAddRec.push_back(stRec);
        }
        else
        {
            vDelRec.push_back(stRec);
        }

        if ((vAddRec.size()+vDelRec.size()) >= ulMaxRecNum)
        {
            break;
        }

        memset(&stRec, 0, sizeof(stRec));
    }

    m_pPDBApp->CloseQuery();

    return PDB_ERROR != enRet;
}

BOOL OmcDao::SyncOverHistoryAlarm(ALARM_INFO_T &stInfo, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.BindString(stInfo.acNeID);
    Params.Bind(&stInfo.ulAlarmID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE history_alarm SET SyncFlag=0 WHERE ne_id=? AND alarm_id=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {

        iAffectedRow = m_pPDBApp->Update("DELETE FROM history_alarm where ne_id=? AND alarm_id=?", Params);
        if(iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}
// 批量添加活跃告警
BOOL OmcDao::AddActiveAlarm(VECTOR<ALARM_INFO_T> &vInfo)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;
    INT32           iSyncFlag = SYNC_ADD;

    FormatSyncFlag(iSyncFlag);
    m_pPDBApp->TransBegin();

    for (UINT32 i=0; i<vInfo.size(); i++)
    {
        ALARM_INFO_T &stInfo = vInfo[i];
        Params.Clear();

        Params.BindString(stInfo.acNeID);
        Params.Bind(&stInfo.ulAlarmID);
        Params.Bind(&stInfo.ulAlarmRaiseTime);
        Params.Bind(&stInfo.ulAlarmType);
        Params.Bind(&stInfo.ulAlarmLevel);
        Params.BindString(stInfo.acAlarmTitle);
        Params.BindString(stInfo.acAlarmInfo);
        Params.BindString(stInfo.acAlarmReason);
        Params.Bind(&iSyncFlag);

        Params.Bind(&stInfo.ulAlarmRaiseTime);
        Params.Bind(&stInfo.ulAlarmType);
        Params.Bind(&stInfo.ulAlarmLevel);
        Params.BindString(stInfo.acAlarmTitle);
        Params.BindString(stInfo.acAlarmInfo);
        Params.BindString(stInfo.acAlarmReason);
        Params.Bind(&iSyncFlag);

        iAffectedRow = m_pPDBApp->Update(" INSERT INTO active_alarm(ne_id, alarm_id, alarm_time, alarm_type, alarm_level, alarm_title, alarm_info, alarm_reason, SyncFlag) VALUES(?,?,?,?,?,?,?,?,?) "
                                         " ON DUPLICATE KEY UPDATE alarm_time=?, alarm_type=?, alarm_level=?, alarm_title=?, alarm_info=?, alarm_reason=?, SyncFlag=?", Params);
        if (iAffectedRow < 0)
        {
            m_pPDBApp->TransRollback();
            return FALSE;
        }
    }

    m_pPDBApp->TransCommit();

    return TRUE;
}

//批量消除活跃告警
BOOL OmcDao::ClearActiveAlarm(VECTOR<ALARM_INFO_T> &vInfo, CHAR *szClearInfo)
{
    INT32           iAffectedRow = 0;
    GString         strSQL(32*1024);
    PDBParam        Params;
    UINT32          ulCurrTime = gos_get_current_time();

    if (vInfo.size() == 0)
    {
        return TRUE;
    }

    m_pPDBApp->TransBegin();
    for (UINT32 i=0; i<vInfo.size(); i++)
    {
        INT32           iSyncFlag = SYNC_ADD;
        ALARM_INFO_T &stInfo = vInfo[i];

        FormatSyncFlag(iSyncFlag);
        vInfo[i].ulAlarmClearTime = ulCurrTime;
        Params.Clear();

        Params.Bind(&ulCurrTime);
        Params.BindString(szClearInfo);
        Params.Bind(&iSyncFlag);
        Params.BindString(stInfo.acNeID);
        Params.Bind(&stInfo.ulAlarmID);

        Params.Bind(&ulCurrTime);
        Params.BindString(szClearInfo);

        iAffectedRow = m_pPDBApp->Update(" INSERT INTO history_alarm(ne_id, alarm_id, alarm_time, alarm_type, alarm_level, alarm_title, alarm_info, alarm_reason, alarm_clear_time, alarm_clear_info, SyncFlag) "
            " SELECT ne_id, alarm_id, alarm_time, alarm_type, alarm_level, alarm_title, alarm_info, alarm_reason, ?, ?, ? FROM active_alarm WHERE ne_id=? AND alarm_id=? "
            " ON DUPLICATE KEY UPDATE alarm_clear_time = ?, alarm_clear_info = ? ", Params);

        if(iAffectedRow < 0)
        {
            m_pPDBApp->TransRollback();
            return FALSE;
        }

        if (!UpdateHistoryStatistics(stInfo.ulAlarmRaiseTime, stInfo.ulAlarmLevel))
        {
            m_pPDBApp->TransRollback();
            return FALSE;
        }

        iSyncFlag = SYNC_DEL;
        FormatSyncFlag(iSyncFlag);
        Params.Clear();

        Params.Bind(&iSyncFlag);
        Params.BindString(stInfo.acNeID);
        Params.Bind(&stInfo.ulAlarmID);

        iAffectedRow = m_pPDBApp->Update(" UPDATE active_alarm SET SyncFlag=? WHERE ne_id=? AND alarm_id=? ", Params);
        if(iAffectedRow < 0)
        {
            m_pPDBApp->TransRollback();
            return FALSE;
        }

    }

    return m_pPDBApp->TransCommit();
}

BOOL OmcDao::ClearActiveAlarm(CHAR *szNeID, UINT32 ulAlarmID, UINT32 ulAlarmRaiseTime, UINT32 ulAlarmLevel, CHAR *szClearInfo)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;
    INT32           iSyncFlag = SYNC_ADD;
    UINT32          ulCurrTime = gos_get_current_time();
    
    FormatSyncFlag(iSyncFlag);

    Params.Bind(&ulCurrTime);
    Params.BindString(szClearInfo);
    Params.Bind(&iSyncFlag);
    Params.BindString(szNeID);
    Params.Bind(&ulAlarmID);

    Params.Bind(&ulCurrTime);
    Params.BindString(szClearInfo);

    m_pPDBApp->TransBegin();
    iAffectedRow = m_pPDBApp->Update( " INSERT INTO history_alarm(ne_id, alarm_id, alarm_time, alarm_type, alarm_level, alarm_title, alarm_info, alarm_reason, alarm_clear_time, alarm_clear_info, SyncFlag) "
                                      " SELECT ne_id, alarm_id, alarm_time, alarm_type, alarm_level, alarm_title, alarm_info, alarm_reason, ?, ?, ? "
                                      " FROM active_alarm WHERE ne_id=? AND alarm_id=? "
                                      " ON DUPLICATE KEY UPDATE alarm_clear_time = ?, alarm_clear_info = ? ", Params);
    if(iAffectedRow < 0)
    {
        m_pPDBApp->TransRollback();
        return FALSE;
    }
    // 同步历史告警统计表
    if (!UpdateHistoryStatistics(ulAlarmRaiseTime, ulAlarmLevel))
    {
        m_pPDBApp->TransRollback();
        return FALSE;
    }

    iSyncFlag = SYNC_DEL;
    FormatSyncFlag(iSyncFlag);

    Params.Clear();
    Params.Bind(&iSyncFlag);
    Params.BindString(szNeID);
    Params.Bind(&ulAlarmID);

    iAffectedRow = m_pPDBApp->Update(" UPDATE active_alarm SET SyncFlag=? WHERE ne_id=? AND alarm_id=? ", Params);
    if (iAffectedRow < 0)
    {
        m_pPDBApp->TransRollback();
        return FALSE;
    }

    return m_pPDBApp->TransCommit();;
}

BOOL OmcDao::GetSyncingTxStatus(UINT32 ulMaxRecNum, VECTOR<OMA_TX_STATUS_REC_T> &vAddRec, VECTOR<OMA_TX_STATUS_REC_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    OMA_TX_STATUS_REC_T       stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&stRec.ulTime);
    Params.Bind(stRec.acNeID, sizeof(stRec.acNeID));
    Params.Bind(&stRec.stStatus.stLteStatus.ulStationID);
    Params.Bind(&stRec.stStatus.stResStatus.ulCpuUsage);
    Params.Bind(&stRec.stStatus.stResStatus.ulMemUsage);
    Params.Bind(&stRec.stStatus.stResStatus.ulDiskUsage);
    Params.Bind(&stRec.stStatus.stResStatus.iDevTemp);
    Params.Bind(&stRec.stStatus.stLteStatus.ulPCI);
    Params.Bind(&stRec.stStatus.stLteStatus.iRSRP);
    Params.Bind(&stRec.stStatus.stLteStatus.iRSRQ);
    Params.Bind(&stRec.stStatus.stLteStatus.iRSSI);
    Params.Bind(&stRec.stStatus.stLteStatus.iSINR);
    Params.Bind(&stRec.stStatus.ulRunStatus);
    Params.Bind(&iSyncFlag);

    sprintf(acSQL, "SELECT time, ne_id, station_id, cpu_usage, mem_usage,disk_usage, dev_temp, pci, rsrp, rsrq, rssi, sinr, status, SyncFlag FROM ne_basic_status WHERE SyncFlag!=0");
    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query ne_basic_status failed");
        return FALSE;
    }

    memset(&stRec, 0, sizeof(stRec));
    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        if (iSyncFlag > 0)
        {
            vAddRec.push_back(stRec);
        }
        else
        {
            vDelRec.push_back(stRec);
        }

        if ((vAddRec.size()+vDelRec.size()) >= ulMaxRecNum)
        {
            break;
        }

        memset(&stRec, 0, sizeof(stRec));
    }

    m_pPDBApp->CloseQuery();

    return PDB_ERROR != enRet;
}

BOOL OmcDao::SetNEStatus(OMA_TX_STATUS_REC_T &stInfo, INT32 iSyncFlag)
{
    INT32               iAffectedRow = 0;
    PDBParam            Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&stInfo.ulTime);
    Params.Bind(stInfo.acNeID, sizeof(stInfo.acNeID));
    Params.Bind(&stInfo.stStatus.stLteStatus.ulStationID);
    Params.Bind(&stInfo.stStatus.bOnline);
    Params.Bind(&stInfo.stStatus.stResStatus.ulCpuUsage);
    Params.Bind(&stInfo.stStatus.stResStatus.ulMemUsage);
    Params.Bind(&stInfo.stStatus.stResStatus.ulDiskUsage);
    Params.Bind(&stInfo.stStatus.stResStatus.iDevTemp);
    Params.Bind(&stInfo.stStatus.stLteStatus.ulPCI);
    Params.Bind(&stInfo.stStatus.stLteStatus.iRSRP);
    Params.Bind(&stInfo.stStatus.stLteStatus.iRSRQ);
    Params.Bind(&stInfo.stStatus.stLteStatus.iRSSI);
    Params.Bind(&stInfo.stStatus.stLteStatus.iSINR);
    Params.Bind(&stInfo.stStatus.ulRunStatus);
    Params.Bind(&iSyncFlag);

    Params.Bind(&stInfo.ulTime);
    Params.Bind(&iSyncFlag);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO ne_basic_status(time, ne_id, station_id, online, cpu_usage, mem_usage, disk_usage, dev_temp, pci, rsrp, rsrq, rssi, sinr, status, SyncFlag) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"
        "ON DUPLICATE KEY UPDATE time=?, SyncFlag=?", Params);

    if (INVALID_ROWCOUNT == iAffectedRow)
    {
        DaoLog(LOG_ERROR, "query ne_basic_status failed");
        return FALSE;
    }

    return TRUE;
}

BOOL OmcDao::SyncOverTxStatus(OMA_TX_STATUS_REC_T &stInfo, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stInfo.ulTime);
    Params.BindString(stInfo.acNeID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE ne_basic_status SET SyncFlag=0 WHERE time=? AND ne_id=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM ne_basic_status WHERE time=? AND ne_id=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL OmcDao::AddFxStatus(VECTOR<OMA_FX_STATUS_REC_T> &vInfo)
{
    INT32           iAffectedRow = 0;
    CHAR            acTmp[1024];
    GString         strSQL;
    INT32           iSyncFlag = SYNC_ADD;

    FormatSyncFlag(iSyncFlag);

    if (vInfo.size() == 0)
    {
        return TRUE;
    }

    strSQL.Append("INSERT INTO ne_basic_status(time, ne_id, station_id, online, cpu_usage, mem_usage, disk_usage, dev_temp, pci, rsrp, rsrq, rssi, sinr, status, SyncFlag) VALUES ");
    for (UINT32 i=0; i<vInfo.size(); i++)
    {
        OMA_FX_STATUS_REC_T &stInfo = vInfo[i];

        sprintf(acTmp, "%s(%u,'%s', %u, %u, %u, %u, %u, %d, %u, %d, %d, %d, %d, %u, %d)", 
                i==0?"":",",
                stInfo.ulTime,
                stInfo.acNeID,
                stInfo.stStatus.stLteStatus.ulStationID,
                stInfo.stStatus.bOnline,
                stInfo.stStatus.stResStatus.ulCpuUsage,
                stInfo.stStatus.stResStatus.ulMemUsage,
                stInfo.stStatus.stResStatus.ulDiskUsage,
                stInfo.stStatus.stResStatus.iDevTemp,
                stInfo.stStatus.stLteStatus.ulPCI,
                stInfo.stStatus.stLteStatus.iRSRP,
                stInfo.stStatus.stLteStatus.iRSRQ,
                stInfo.stStatus.stLteStatus.iRSSI,
                stInfo.stStatus.stLteStatus.iSINR,
                stInfo.stStatus.bOnline,
                iSyncFlag);

        strSQL.Append(acTmp);
    }

    iAffectedRow = m_pPDBApp->Update(strSQL.GetString());
    if (iAffectedRow <= 0)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL OmcDao::GetTxStatus(UINT32 ulStartTime, UINT32 ulEndTime, VECTOR<OMA_TX_STATUS_REC_T> &vInfo)
{
    INT32                   iRow = 0;
    CHAR                    acSQL[1024];
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    OMA_TX_STATUS_REC_T     stInfo = {0};

    Params.Bind(&stInfo.ulTime);
    Params.Bind(stInfo.acNeID, sizeof(stInfo.acNeID));
    Params.Bind(&stInfo.stStatus.stLteStatus.ulStationID);
    Params.Bind(&stInfo.stStatus.stResStatus.ulCpuUsage);
    Params.Bind(&stInfo.stStatus.stResStatus.ulMemUsage);
    Params.Bind(&stInfo.stStatus.stResStatus.ulDiskUsage);
    Params.Bind(&stInfo.stStatus.stResStatus.iDevTemp);
    Params.Bind(&stInfo.stStatus.stLteStatus.ulPCI);
    Params.Bind(&stInfo.stStatus.stLteStatus.iRSRP);
    Params.Bind(&stInfo.stStatus.stLteStatus.iRSRQ);
    Params.Bind(&stInfo.stStatus.stLteStatus.iRSSI);
    Params.Bind(&stInfo.stStatus.stLteStatus.iSINR);
    Params.Bind(&stInfo.stStatus.ulRunStatus);

    sprintf(acSQL, "SELECT time, ne_id, station_id, cpu_usage, mem_usage,disk_usage, dev_temp, pci, rsrp, rsrq, rssi, sinr, status FROM ne_basic_status WHERE time>=%u AND time<=%u", ulStartTime, ulEndTime);
    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query ne_basic_status failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        vInfo.push_back(stInfo);
        memset(&stInfo, 0, sizeof(stInfo));
    }

    m_pPDBApp->CloseQuery();

    return enRet != PDB_ERROR;
}

BOOL OmcDao::GetTxStatus(UINT32 ulStartTime, UINT32 ulEndTime, CHAR *szNeID, OMT_GET_TXSTATUS_RSP_T *pstRsp)
{
    INT32                   iRow = 0;
    CHAR                    acSQL[1024];
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    OMA_TX_STATUS_REC_T     stInfo = {0};

    pstRsp->ulNum = 0;

    Params.Bind(&stInfo.ulTime);
    Params.Bind(stInfo.acNeID, sizeof(stInfo.acNeID));
    Params.Bind(&stInfo.stStatus.stLteStatus.ulStationID);
    Params.Bind(&stInfo.stStatus.stResStatus.ulCpuUsage);
    Params.Bind(&stInfo.stStatus.stResStatus.ulMemUsage);
    Params.Bind(&stInfo.stStatus.stResStatus.ulDiskUsage);
    Params.Bind(&stInfo.stStatus.stResStatus.iDevTemp);
    Params.Bind(&stInfo.stStatus.stLteStatus.ulPCI);
    Params.Bind(&stInfo.stStatus.stLteStatus.iRSRP);
    Params.Bind(&stInfo.stStatus.stLteStatus.iRSRQ);
    Params.Bind(&stInfo.stStatus.stLteStatus.iRSSI);
    Params.Bind(&stInfo.stStatus.stLteStatus.iSINR);
    Params.Bind(&stInfo.stStatus.ulRunStatus);

    sprintf(acSQL, "SELECT time, ne_id, station_id, cpu_usage, mem_usage,disk_usage, dev_temp, pci, rsrp, rsrq, rssi, sinr, status "
                "FROM ne_basic_status WHERE time>=%u AND time<=%u AND ne_id='%s' ORDER BY time", ulStartTime, ulEndTime, szNeID);
    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query ne_basic_status failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        memcpy(&pstRsp->astRec[pstRsp->ulNum++], &stInfo, sizeof(stInfo));
        if (pstRsp->ulNum >= ARRAY_SIZE(pstRsp->astRec))
        {
            break;
        }

        memset(&stInfo, 0, sizeof(stInfo));
    }

    m_pPDBApp->CloseQuery();

    return enRet != PDB_ERROR;
}

BOOL OmcDao::GetAllTxStatus(UINT32 ulTime, OMT_GET_ALLTXSTATUS_RSP_T *pstRsp)
{
    INT32                   iRow = 0;
    CHAR                    acSQL[1024];
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    OMA_TX_STATUS_REC_T     stInfo = {0};

    pstRsp->ulNum = 0;

    Params.Bind(&stInfo.ulTime);
    Params.Bind(stInfo.acNeID, sizeof(stInfo.acNeID));
    Params.Bind(&stInfo.stStatus.stLteStatus.ulStationID);
    Params.Bind(&stInfo.stStatus.stResStatus.ulCpuUsage);
    Params.Bind(&stInfo.stStatus.stResStatus.ulMemUsage);
    Params.Bind(&stInfo.stStatus.stResStatus.ulDiskUsage);
    Params.Bind(&stInfo.stStatus.stResStatus.iDevTemp);
    Params.Bind(&stInfo.stStatus.stLteStatus.ulPCI);
    Params.Bind(&stInfo.stStatus.stLteStatus.iRSRP);
    Params.Bind(&stInfo.stStatus.stLteStatus.iRSRQ);
    Params.Bind(&stInfo.stStatus.stLteStatus.iRSSI);
    Params.Bind(&stInfo.stStatus.stLteStatus.iSINR);
    Params.Bind(&stInfo.stStatus.ulRunStatus);

    sprintf(acSQL, "SELECT time, ne_id, station_id, cpu_usage, mem_usage,disk_usage, dev_temp, pci, rsrp, rsrq, rssi, sinr, status "
        "FROM ne_basic_status WHERE time=(SELECT MAX(time) FROM ne_basic_status WHERE time<=%u)", ulTime);
    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query ne_basic_status failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        memcpy(&pstRsp->astRec[pstRsp->ulNum++], &stInfo, sizeof(stInfo));
        if (pstRsp->ulNum >= ARRAY_SIZE(pstRsp->astRec))
        {
            break;
        }

        memset(&stInfo, 0, sizeof(stInfo));
    }

    m_pPDBApp->CloseQuery();

    return enRet != PDB_ERROR;
}
// 获取所有设备的状态信息
BOOL OmcDao::GetAllDevStatus(UINT32 ulTime, GJsonTupleParam &TupleParam)
{
    INT32                   iRow = 0;
    CHAR                    acSQL[1024];
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    GJsonParam              JsonParam;
    CHAR                    acNeID[NE_ID_LEN];
    BOOL                    bOnline;
    UINT32                  ulCpuUsage = 0;
    UINT32                  ulMemUsage = 0;
    UINT32                  ulDiskUsage = 0;
    UINT32                  ulStation = 0;
    INT32                   iPCI;
    INT32                   iRSRP;
    INT32                   iRSRQ;
    INT32                   iRSSI;
    INT32                   iSINR;

    TupleParam.Clear();

    Params.Bind(&ulTime);
    Params.Bind(acNeID, sizeof(acNeID));
    Params.Bind(&bOnline);
    Params.Bind(&ulCpuUsage);
    Params.Bind(&ulMemUsage);
    Params.Bind(&ulDiskUsage);
    
    Params.Bind(&ulStation);
    Params.Bind(&iPCI);
    Params.Bind(&iRSRP);
    Params.Bind(&iRSRQ);
    Params.Bind(&iRSSI);
    Params.Bind(&iSINR);

    sprintf(acSQL, " SELECT time, ne_id, online, cpu_usage, mem_usage, disk_usage, station_id, pci, rsrp, rsrq, rssi, sinr "
                   " FROM ne_basic_status WHERE time=(SELECT MAX(time) FROM ne_basic_status WHERE time<=%u) AND SyncFlag>=0 ", ulTime);
    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query ne_basic_status failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        JsonParam.Clear();

        JsonParam.Add("Time", ulTime);
        JsonParam.Add("NeID", acNeID);
        JsonParam.Add("Online", bOnline);
        JsonParam.Add("Cpu", ulCpuUsage);
        JsonParam.Add("Mem", ulMemUsage);
        JsonParam.Add("Disk", ulDiskUsage);
        
        JsonParam.Add("Station", ulStation);
        JsonParam.Add("PCI", iPCI);
        JsonParam.Add("RSRP", iRSRP);
        JsonParam.Add("RSRQ", iRSRQ);
        JsonParam.Add("RSSI", iRSSI);
        JsonParam.Add("SINR", iSINR);

        TupleParam.Add(JsonParam);
        // TupleParam 绑定的数据最大为 60KB
        if (TupleParam.GetStringSize() > GosGetSafeMsgSize())
        {
            break;
        }
    }

    m_pPDBApp->CloseQuery();

    return enRet != PDB_ERROR;
}
// 获取指定设备的状态消息
BOOL OmcDao::GetDevStatus(UINT32 ulStartTime, UINT32 ulEndTime, CHAR *szNeID, GJsonTupleParam &TupleParam)
{
    INT32                   iRow = 0;
    CHAR                    acSQL[1024] = {0};
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    GJsonParam              JsonParam;
    CHAR                    acNeID[NE_ID_LEN] = {0};
    UINT32                  ulTime;
    BOOL                    bOnline = FALSE;
    UINT32                  ulCpuUsage = 0;
    UINT32                  ulMemUsage = 0;
    UINT32                  ulDiskUsage = 0;

    TupleParam.Clear();

    Params.Bind(&ulTime);
    Params.Bind(acNeID, sizeof(acNeID));
    Params.Bind(&bOnline);

    Params.Bind(&ulCpuUsage);
    Params.Bind(&ulMemUsage);
    Params.Bind(&ulDiskUsage);

    sprintf(acSQL, " SELECT time, ne_id, online, cpu_usage, mem_usage, disk_usage "
                   " FROM ne_basic_status WHERE time>=%u AND time<%u AND ne_id='%s' ORDER BY time ", ulStartTime, ulEndTime, szNeID);

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query ne_basic_status failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        JsonParam.Clear();

        JsonParam.Add("Time", ulTime);
        JsonParam.Add("NeID", acNeID);
        JsonParam.Add("Online", bOnline);
        JsonParam.Add("Cpu", ulCpuUsage);
        JsonParam.Add("Mem", ulMemUsage);
        JsonParam.Add("Disk", ulDiskUsage);

        TupleParam.Add(JsonParam);

        if (TupleParam.GetStringSize() > GosGetSafeMsgSize())
        {
            break;
        }

        bOnline = FALSE;
    }

    m_pPDBApp->CloseQuery();

    return enRet != PDB_ERROR;
}

BOOL OmcDao::GetLteStatus(UINT32 ulStartTime, UINT32 ulEndTime, CHAR *szNeID, GJsonTupleParam &TupleParam)
{
    INT32                   iRow = 0;
    CHAR                    acSQL[1024];
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    GJsonParam              JsonParam;
    CHAR                    acNeID[NE_ID_LEN];
    UINT32                  ulTime;
    BOOL                    bOnline = FALSE;
    UINT32                  ulCpuUsage = 0;
    UINT32                  ulMemUsage = 0;
    UINT32                  ulDiskUsage = 0;

    INT32                   iPCI;
    INT32                   iRSRP;
    INT32                   iRSRQ;
    INT32                   iRSSI;
    INT32                   iSINR;

    TupleParam.Clear();

    Params.Bind(&ulTime);
    Params.Bind(acNeID, sizeof(acNeID));
    Params.Bind(&bOnline);

    Params.Bind(&ulCpuUsage);
    Params.Bind(&ulMemUsage);
    Params.Bind(&ulDiskUsage);
    Params.Bind(&iPCI);
    Params.Bind(&iRSRP);
    Params.Bind(&iRSRQ);
    Params.Bind(&iRSSI);
    Params.Bind(&iSINR);

    sprintf(acSQL, "SELECT time, ne_id, online, cpu_usage, mem_usage, disk_usage, pci, rsrp, rsrq, rssi, sinr "
        "FROM ne_basic_status WHERE time>=%u AND time<=%u AND ne_id='%s' ORDER BY time", ulStartTime, ulEndTime, szNeID);

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query ne_basic_status failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        JsonParam.Clear();

        JsonParam.Add("Time", ulTime);
        JsonParam.Add("NeID", acNeID);
        JsonParam.Add("Online", bOnline);
        JsonParam.Add("Cpu", ulCpuUsage);
        JsonParam.Add("Mem", ulMemUsage);
        JsonParam.Add("Disk", ulDiskUsage);

        JsonParam.Add("PCI", iPCI);
        JsonParam.Add("RSRP", iRSRP);
        JsonParam.Add("RSRQ", iRSRQ);
        JsonParam.Add("RSSI", iRSSI);
        JsonParam.Add("SINR", iSINR);

        TupleParam.Add(JsonParam);

        if (TupleParam.GetStringSize() > GosGetSafeMsgSize())
        {
            break;
        }

        bOnline = FALSE;
    }

    m_pPDBApp->CloseQuery();

    return enRet != PDB_ERROR;
}

BOOL OmcDao::GetTrafficStatus(UINT32 ulStartTime, UINT32 ulEndTime, CHAR *szNeID, GJsonTupleParam &TupleParam)
{
    INT32                   iRow = 0;
    CHAR                    acSQL[1024];
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    CHAR                    acNeID[NE_ID_LEN];
    UINT32                  ulTime;
    UINT64                  u64WanTx = 0;
    UINT64                  u64WanRx = 0;
    UINT64                  u64Lan1Tx = 0;
    UINT64                  u64Lan1Rx = 0;
    UINT64                  u64Lan2Tx = 0;
    UINT64                  u64Lan2Rx = 0;
    UINT64                  u64Lan3Tx = 0;
    UINT64                  u64Lan3Rx = 0;
    UINT64                  u64Lan4Tx = 0;
    UINT64                  u64Lan4Rx = 0;

    TupleParam.Clear();
    Params.Bind(&ulTime);
    Params.Bind(acNeID, sizeof(acNeID));
    Params.Bind(&u64WanTx);
    Params.Bind(&u64WanRx);
    Params.Bind(&u64Lan1Tx);
    Params.Bind(&u64Lan1Rx);
    Params.Bind(&u64Lan2Tx);
    Params.Bind(&u64Lan2Rx);
    Params.Bind(&u64Lan3Tx);
    Params.Bind(&u64Lan3Rx);
    Params.Bind(&u64Lan4Tx);
    Params.Bind(&u64Lan4Rx);

    sprintf(acSQL, " SELECT time, ne_id, wan_tx, wan_rx, lan1_tx, lan1_rx, lan2_tx, lan2_rx, lan3_tx, lan3_rx, lan4_tx, lan4_rx "
                   " FROM   traffic_status WHERE time>=%u AND time<%u AND ne_id='%s' ORDER BY time", ulStartTime, ulEndTime, szNeID);

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query ne_basic_status failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        GJsonParam              JsonParam;

        JsonParam.Add("Time",   ulTime);
        JsonParam.Add("NeID",   acNeID);
        JsonParam.Add("WanTx",  u64WanTx);
        JsonParam.Add("WanRx",  u64WanRx);
        JsonParam.Add("Lan1Tx", u64Lan1Tx);
        JsonParam.Add("Lan1Rx", u64Lan1Rx);
        JsonParam.Add("Lan2Tx", u64Lan2Tx);
        JsonParam.Add("Lan2Rx", u64Lan2Rx);
        JsonParam.Add("Lan3Tx", u64Lan3Tx);
        JsonParam.Add("Lan3Rx", u64Lan3Rx);
        JsonParam.Add("Lan4Tx", u64Lan4Tx);
        JsonParam.Add("Lan4Rx", u64Lan4Rx);

        TupleParam.Add(JsonParam);
        if (TupleParam.GetStringSize() > GosGetSafeMsgSize())
        {
            break;
        }
    }

    m_pPDBApp->CloseQuery();

    return enRet != PDB_ERROR;
}

BOOL OmcDao::AddDevOperLog(OMA_OPERLOG_T &stLog)
{
    return SetDevOperLog(stLog, SYNC_ADD);
}

BOOL OmcDao::SetDevOperLog(OMA_OPERLOG_T &stLog, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stLog.ulTime);
    Params.BindString(stLog.acNeID);
    Params.BindString(stLog.acLogInfo);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO dev_operlog(SyncFlag, oper_time, ne_id, log_info) VALUES(?,?,?,?) "
                                "ON DUPLICATE KEY UPDATE oper_time=oper_time", Params);;
    if (iAffectedRow <= 0)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL OmcDao::GetDevOperLog(CHAR *szNeID, UINT32 ulStartTime, UINT32 ulEndTime, GJsonTupleParam &JsonTupleParam)
{
    INT32                   iRow = 0;
    CHAR                    acSQL[1024];
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    OMA_OPERLOG_T           stInfo = {0};
    GJsonParam              JsonParam;

    JsonTupleParam.Clear();

    Params.Bind(&stInfo.ulTime);
    Params.Bind(stInfo.acLogInfo, sizeof(stInfo.acLogInfo));

    sprintf(acSQL, "SELECT oper_time, log_info FROM dev_operlog WHERE oper_time>=%u AND oper_time<=%u AND ne_id='%s' ORDER BY oper_time",
            ulStartTime, ulEndTime, szNeID);
    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query dev_operlog failed");
        return FALSE;
    }

    memset(&stInfo, 0, sizeof(stInfo));
    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        JsonParam.Clear();
        JsonParam.Add("time", stInfo.ulTime);
        JsonParam.Add("log", stInfo.acLogInfo);
        JsonTupleParam.Add(JsonParam);

        if (JsonTupleParam.GetStringSize() > 60*1024)
        {
            break;
        }

        memset(&stInfo, 0, sizeof(stInfo));
    }

    m_pPDBApp->CloseQuery();

    return enRet != PDB_ERROR;
}

BOOL OmcDao::GetSyncingDevOperLog(UINT32 ulMaxRecNum, VECTOR<OMA_OPERLOG_T> &vAddRec, VECTOR<OMA_OPERLOG_T> &vDelRec)
{
    INT32                   iRow = 0;
    CHAR                    acSQL[1024];
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    OMA_OPERLOG_T           stRec = {0};
    INT32                   iSyncFlag;
    GJsonParam              JsonParam;

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulTime);
    Params.Bind(stRec.acNeID, sizeof(stRec.acNeID));
    Params.Bind(stRec.acLogInfo, sizeof(stRec.acLogInfo));

    sprintf(acSQL, "SELECT SyncFlag, oper_time, ne_id, log_info FROM dev_operlog WHERE SyncFlag!=0");
    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query dev_operlog failed");
        return FALSE;
    }

    memset(&stRec, 0, sizeof(stRec));
    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        if (iSyncFlag > 0)
        {
            vAddRec.push_back(stRec);
        }
        else
        {
            vDelRec.push_back(stRec);
        }

        if ((vAddRec.size()+vDelRec.size()) >= ulMaxRecNum)
        {
            break;
        }

        memset(&stRec, 0, sizeof(stRec));
    }

    m_pPDBApp->CloseQuery();

    return PDB_ERROR != enRet;
}

BOOL OmcDao::SyncOverDevOperLog(OMA_OPERLOG_T &stInfo, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stInfo.ulTime);
    Params.BindString(stInfo.acNeID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE dev_operlog SET SyncFlag=0 WHERE oper_time=? AND ne_id=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM dev_operlog WHERE oper_time=?, ne_id=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL OmcDao::AddDevFile(OMA_FILE_INFO_T &stFile)
{
    return SetDevFile(stFile, SYNC_ADD);
}

BOOL OmcDao::SetDevFile(OMA_FILE_INFO_T &stFile, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.BindString(stFile.acNeID);
    Params.Bind(&stFile.ulFileTime);
    Params.Bind(&stFile.ulFileType);
    Params.BindString(stFile.acFileName);
    Params.BindString(stFile.acFileSize);
    Params.Bind(&stFile.bFileExist);
    Params.Bind(&iSyncFlag);

    Params.Bind(&stFile.ulFileTime);
    Params.Bind(&iSyncFlag);

    iAffectedRow = m_pPDBApp->Update(" INSERT INTO dev_file(ne_id, file_time, file_type, file_name, file_size, file_exist, SyncFlag) VALUES(?,?,?,?,?,?,?) "
        "ON DUPLICATE KEY UPDATE file_time=?, SyncFlag=?", Params);
    if (iAffectedRow <= 0)
    {
        return FALSE;
    }

    return TRUE;
}


BOOL OmcDao::SetDevFileName(OMA_FILE_INFO_T &stFile, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.BindString(stFile.acNeID);
    Params.Bind(&stFile.ulFileTime);
    Params.Bind(&stFile.ulFileType);
    Params.BindString(stFile.acFileName);
    Params.BindString(stFile.acFileSize);
    Params.Bind(&stFile.bFileExist);
    Params.Bind(&iSyncFlag);

    Params.Bind(&stFile.ulFileTime);
    Params.Bind(&iSyncFlag);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO dev_file(ne_id, file_time, file_type, file_name, file_size, file_exist, SyncFlag) VALUES(?,?,?,?,?,?,?) "
        "ON DUPLICATE KEY UPDATE file_time=?, SyncFlag=?", Params);
    if (iAffectedRow <= 0)
    {
        return FALSE;
    }

    return TRUE;
}
BOOL OmcDao::UpdateDevFileExistFlag(CHAR *szNeID, CHAR *szFileName)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;
    INT32           iSyncFlag = SYNC_ADD;


    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.BindString(szNeID);
    Params.BindString(szFileName);

    iAffectedRow = m_pPDBApp->Update("UPDATE dev_file SET SyncFlag=?, file_exist=1 WHERE ne_id=? AND file_name=?", Params);
    if (iAffectedRow < 0)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL OmcDao::GetSyncingDevFile(UINT32 ulMaxRecNum, VECTOR<OMA_FILE_INFO_T> &vAddRec, VECTOR<OMA_FILE_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    CHAR                    acSQL[1024];
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    OMA_FILE_INFO_T           stRec = {0};
    INT32                   iSyncFlag;
    GJsonParam              JsonParam;

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(stRec.acNeID, sizeof(stRec.acNeID));
    Params.Bind(&stRec.ulFileTime);
    Params.Bind(stRec.acFileName, sizeof(stRec.acFileName));
    Params.Bind(&stRec.ulFileType);
    Params.Bind(stRec.acFileSize, sizeof(stRec.acFileSize));
    Params.Bind(&stRec.bFileExist);
    Params.Bind(&iSyncFlag);

    sprintf(acSQL, "SELECT ne_id, file_time, file_name, file_type, file_size, file_exist, SyncFlag FROM dev_file WHERE SyncFlag!=0");
    iRow = m_pPDBApp->Query(acSQL, Params);

     if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query dev_file failed");
        return FALSE;
    }

    memset(&stRec, 0, sizeof(stRec));
    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        if (iSyncFlag > 0)
        {
            vAddRec.push_back(stRec);
        }
        else
        {
            vDelRec.push_back(stRec);
        }

        if ((vAddRec.size()+vDelRec.size()) >= ulMaxRecNum)
        {
            break;
        }

        memset(&stRec, 0, sizeof(stRec));
    }

    m_pPDBApp->CloseQuery();

    return PDB_ERROR != enRet;
}

BOOL OmcDao::SyncOverDevFile(OMA_FILE_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.BindString(stRec.acNeID);
    Params.Bind(&stRec.ulFileTime);
    Params.Bind(&stRec.ulFileType);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE dev_file SET SyncFlag=0 WHERE ne_id=? AND file_time=? AND file_type=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM dev_file WHERE ne_id=? AND file_time=? AND file_type=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL OmcDao::GetDevFile(UINT32 ulDevFileType, CHAR *szNeID, UINT32 ulStartTime, UINT32 ulEndTime, GJsonTupleParam &JsonTupleParam)
{
    INT32                   iRow = 0;
    CHAR                    acSQL[1024];
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    OMA_FILE_INFO_T         stInfo = {0};
    GJsonParam              JsonParam;

    JsonTupleParam.Clear();

    Params.Bind(&stInfo.ulFileTime);
    Params.Bind(stInfo.acFileName, sizeof(stInfo.acFileName));
    Params.Bind(stInfo.acFileSize, sizeof(stInfo.acFileSize));
    Params.Bind(&stInfo.bFileExist);

    sprintf(acSQL, "SELECT file_time, file_name, file_size, file_exist  FROM dev_file WHERE ne_id='%s' AND file_time>=%u AND file_time<=%u AND file_type=%u ORDER BY file_time",
            szNeID, ulStartTime, ulEndTime, ulDevFileType);
    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query dev_file failed");
        return FALSE;
    }

    memset(&stInfo, 0, sizeof(stInfo));
    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
/*        szRawFile = gos_right_strchr(stInfo.acFileName, '/');
        if (!szRawFile)
        {
            szRawFile = stInfo.acFileName;
        }
        else
        {
            szRawFile ++;
        }

        sprintf(acFileName, "%s/%s", g_acFtpLogFileDir, szRawFile);
*/
        JsonParam.Clear();
        JsonParam.Add("time", stInfo.ulFileTime);
        JsonParam.Add("name", stInfo.acFileName);
        JsonParam.Add("size", stInfo.acFileSize);
        JsonParam.Add("exist",stInfo.bFileExist);
        //JsonParam.Add("exist", gos_file_exist(acFileName));

        JsonTupleParam.Add(JsonParam);

        if (JsonTupleParam.GetStringSize() > 60*1024)
        {
            break;
        }

        memset(&stInfo, 0, sizeof(stInfo));
    }

    m_pPDBApp->CloseQuery();

    return enRet != PDB_ERROR;
}

BOOL OmcDao::GetSwInfo(VECTOR<OMC_SW_INFO_T> &vInfo)//GJsonTupleParam &JsonTupleParam)
{
    INT32                   iRow = 0;
    CHAR                    acSQL[1024];
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    OMC_SW_INFO_T           stInfo = {0};
    GJsonParam              JsonParam;

    vInfo.clear();

    Params.Bind(stInfo.acSwVer, sizeof(stInfo.acSwVer));
    Params.Bind(stInfo.acSwFile, sizeof(stInfo.acSwFile));
    Params.Bind(&stInfo.ulSwType);
    Params.Bind(&stInfo.ulSwTime);

    sprintf(acSQL, "SELECT sw_ver, sw_file, sw_type, sw_time FROM sw_info WHERE SyncFlag>=0 ORDER BY sw_time DESC");
    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query sw_info failed");
        return FALSE;
    }

    memset(&stInfo, 0, sizeof(stInfo));
    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        vInfo.push_back(stInfo);
        memset(&stInfo, 0, sizeof(stInfo));
    }

    m_pPDBApp->CloseQuery();

    return enRet != PDB_ERROR;
}

BOOL OmcDao::AddSwInfo(OMC_SW_INFO_T &stInfo)
{
    return SetSwInfo(stInfo, SYNC_ADD);
}

BOOL OmcDao::SetSwInfo(OMC_SW_INFO_T &stInfo, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.BindString(stInfo.acSwVer);
    Params.BindString(stInfo.acSwFile);
    Params.Bind(&stInfo.ulSwType);
    Params.Bind(&stInfo.ulSwTime);
    Params.Bind(&iSyncFlag);

    Params.BindString(stInfo.acSwFile);
    Params.Bind(&stInfo.ulSwType);
    Params.Bind(&stInfo.ulSwTime);
    Params.Bind(&iSyncFlag);

    iAffectedRow = m_pPDBApp->Update("INSERT IGNORE INTO sw_info(sw_ver, sw_file, sw_type, sw_time, SyncFlag) VALUES(?,?,?,?,?)"
        "ON DUPLICATE KEY UPDATE sw_file=?, sw_type=?, sw_time=?, SyncFlag=?", Params);
    if (iAffectedRow <= 0)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL OmcDao::DelSwInfo(CHAR *szSwVer)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;
    INT32           iSyncFlag = SYNC_DEL;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.BindString(szSwVer);

    m_pPDBApp->TransBegin();
    iAffectedRow = m_pPDBApp->Update("UPDATE sw_info SET SyncFlag=? WHERE sw_ver=?", Params);
    if (iAffectedRow < 0)
    {
        m_pPDBApp->TransRollback();
        return FALSE;
    }

    iAffectedRow = m_pPDBApp->Update("UPDATE sw_update SET SyncFlag=? WHERE sw_ver=?", Params);
    if (iAffectedRow < 0)
    {
        m_pPDBApp->TransRollback();
        return FALSE;
    }

    m_pPDBApp->TransCommit();

    return TRUE;
}

BOOL OmcDao::GetSyncingSwInfo(UINT32 ulMaxRecNum, VECTOR<OMC_SW_INFO_T> &vAddRec, VECTOR<OMC_SW_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    CHAR                    acSQL[1024];
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    OMC_SW_INFO_T           stRec = {0};
    INT32                   iSyncFlag;
    GJsonParam              JsonParam;

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(stRec.acSwVer, sizeof(stRec.acSwVer));
    Params.Bind(stRec.acSwFile, sizeof(stRec.acSwFile));
    Params.Bind(&stRec.ulSwType);
    Params.Bind(&stRec.ulSwTime);
    Params.Bind(&iSyncFlag);

    sprintf(acSQL, "SELECT sw_ver, sw_file, sw_type, sw_time, SyncFlag FROM sw_info WHERE SyncFlag!=0");
    iRow = m_pPDBApp->Query(acSQL, Params);

     if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query sw_info failed");
        return FALSE;
    }

    memset(&stRec, 0, sizeof(stRec));
    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        if (iSyncFlag > 0)
        {
            vAddRec.push_back(stRec);
        }
        else
        {
            vDelRec.push_back(stRec);
        }

        if ((vAddRec.size()+vDelRec.size()) >= ulMaxRecNum)
        {
            break;
        }

        memset(&stRec, 0, sizeof(stRec));
    }

    m_pPDBApp->CloseQuery();

    return PDB_ERROR != enRet;
}

BOOL OmcDao::SyncOverSwInfo(OMC_SW_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.BindString(stRec.acSwVer);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE sw_info SET SyncFlag=0 WHERE sw_ver=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM sw_info WHERE sw_ver=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }

        iAffectedRow = m_pPDBApp->Update("UPDATE sw_update SET sw_ver='', SyncFlag=1 WHERE sw_ver=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL OmcDao::GetSwUpdateInfo(VECTOR<OMC_SW_UPDATE_INFO_T> &vInfo)//GJsonTupleParam &JsonTupleParam)
{
    INT32                   iRow = 0;
    CHAR                    acSQL[1024];
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    OMC_SW_UPDATE_INFO_T    stInfo = {0};

    vInfo.clear();

    Params.Bind(stInfo.acNeID, sizeof(stInfo.acNeID));
//  Params.Bind(stInfo.acCurrSwVer, sizeof(stInfo.acCurrSwVer));
    Params.Bind(stInfo.acNewSwVer, sizeof(stInfo.acNewSwVer));
    Params.Bind(&stInfo.bSetFlag);

    sprintf(acSQL, "SELECT ne_id, sw_ver, set_flag FROM sw_update WHERE SyncFlag>=0");
    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query sw_update failed");
        return FALSE;
    }

    memset(&stInfo, 0, sizeof(stInfo));
    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        vInfo.push_back(stInfo);
        memset(&stInfo, 0, sizeof(stInfo));
    }

    m_pPDBApp->CloseQuery();

    return enRet != PDB_ERROR;
}

BOOL OmcDao::UpdateSw(CHAR *szNewVer, VECTOR<CHAR*> &vNeID)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;
    BOOL            bSetFalg = TRUE;
    INT32           iSyncFlag = SYNC_ADD;

    FormatSyncFlag(iSyncFlag);

    TransBegin();
    for (UINT32 i=0; i<vNeID.size(); i++)
    {
        Params.Clear();

        Params.BindString(vNeID[i]);
        Params.BindString(szNewVer);
        Params.Bind(&bSetFalg);
        Params.Bind(&iSyncFlag);

        Params.BindString(szNewVer);
        Params.Bind(&bSetFalg);
        Params.Bind(&iSyncFlag);

        iAffectedRow = m_pPDBApp->Update("INSERT INTO sw_update(ne_id, sw_ver, set_flag, SyncFlag) VALUES(?,?,?,?) "
                    "ON DUPLICATE KEY UPDATE sw_ver=?, set_flag=?, SyncFlag=?", Params);
        if (iAffectedRow <= 0)
        {
            TransRollback();
            return FALSE;
        }
    }

    if (!TransCommit())
    {
        TransRollback();
        return FALSE;
    }

    return TRUE;
}

BOOL OmcDao::SetUpdateSwInfo(OMC_SW_UPDATE_INFO_T &stInfo, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.BindString(stInfo.acNeID);
    Params.BindString(stInfo.acNewSwVer);
    Params.Bind(&stInfo.bSetFlag);
    Params.Bind(&iSyncFlag);

    Params.BindString(stInfo.acNewSwVer);
    Params.Bind(&stInfo.bSetFlag);
    Params.Bind(&iSyncFlag);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO sw_update(ne_id, sw_ver, set_flag, SyncFlag) VALUES(?,?,?,?)"
        "ON DUPLICATE KEY UPDATE sw_ver=?, set_flag=?, SyncFlag=?", Params);
    if (iAffectedRow <= 0)
    {
        return FALSE;
    }

    return TRUE;

}

BOOL OmcDao::GetSyncingUpdateSwInfo(UINT32 ulMaxRecNum, VECTOR<OMC_SW_UPDATE_INFO_T> &vAddRec, VECTOR<OMC_SW_UPDATE_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    CHAR                    acSQL[1024];
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    OMC_SW_UPDATE_INFO_T    stRec = {0};
    INT32                   iSyncFlag;
    GJsonParam              JsonParam;

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(stRec.acNeID, sizeof(stRec.acNeID));
    Params.Bind(stRec.acNewSwVer, sizeof(stRec.acNewSwVer));
    Params.Bind(&stRec.bSetFlag);
    Params.Bind(&iSyncFlag);

    sprintf(acSQL, "SELECT ne_id, sw_ver, set_flag, SyncFlag FROM sw_update WHERE SyncFlag!=0");
    iRow = m_pPDBApp->Query(acSQL, Params);

     if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query sw_update failed");
        return FALSE;
    }

    memset(&stRec, 0, sizeof(stRec));
    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        if (iSyncFlag > 0)
        {
            vAddRec.push_back(stRec);
        }
        else
        {
            vDelRec.push_back(stRec);
        }

        if ((vAddRec.size()+vDelRec.size()) >= ulMaxRecNum)
        {
            break;
        }

        memset(&stRec, 0, sizeof(stRec));
    }

    m_pPDBApp->CloseQuery();

    return PDB_ERROR != enRet;
}

BOOL OmcDao::SyncOverUpdateSwInfo(OMC_SW_UPDATE_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.BindString(stRec.acNeID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE sw_update SET SyncFlag=0 WHERE ne_id=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM sw_update WHERE ne_id=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL OmcDao::UpdateSwSetFlag(CHAR *szNeID, BOOL bSetFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;
    INT32           iSyncFlag = SYNC_ADD;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&bSetFlag);
    Params.BindString(szNeID);

    iAffectedRow = m_pPDBApp->Update("UPDATE sw_update SET SyncFlag=?,set_flag=? WHERE ne_id=?", Params);
    if (iAffectedRow < 0)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL OmcDao::GetAlarmLevelCfg(VECTOR<ALARM_LEVEL_CFG_T> &vCfg)
{
    INT32           iRow = 0;
    PDBRET          enRet = PDB_CONTINUE;
    PDBRsltParam        Params;
    ALARM_LEVEL_CFG_T   stCfg = {0};

	Params.Bind(&stCfg.ulDevType);
    Params.Bind(&stCfg.ulAlarmID);
    Params.Bind(&stCfg.ulAlarmLevel);
    Params.Bind(&stCfg.bIsIgnore);

    iRow = m_pPDBApp->Query("SELECT dev_type, alarm_id, alarm_level, alarm_status FROM alarm_level_cfg WHERE SyncFlag>=0", Params);
    if (INVALID_ROWCOUNT == iRow )
    {
        DaoLog(LOG_ERROR,"query alarm_level failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        vCfg.push_back(stCfg);
        memset(&stCfg, 0, sizeof(stCfg));
    }

    m_pPDBApp->CloseQuery();

    return TRUE;
}

BOOL OmcDao::AddAlarmLevelCfg(ALARM_LEVEL_CFG_T &stCfg)
{
    return SetAlarmLevelCfg(stCfg, SYNC_ADD);
}

BOOL OmcDao::SetAlarmLevelCfg(ALARM_LEVEL_CFG_T &stCfg, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

	Params.Bind(&stCfg.ulDevType);
    Params.Bind(&stCfg.ulAlarmID);
    Params.Bind(&stCfg.ulAlarmLevel);
    Params.Bind(&stCfg.bIsIgnore);
    Params.Bind(&iSyncFlag);

    Params.Bind(&stCfg.ulAlarmLevel);
    Params.Bind(&stCfg.bIsIgnore);
    Params.Bind(&iSyncFlag);

    iAffectedRow = m_pPDBApp->Update( "INSERT INTO alarm_level_cfg (dev_type, alarm_id, alarm_level, alarm_status, SyncFlag) "
                                      "VALUES(?,?,?,?,?) ON DUPLICATE KEY UPDATE alarm_level=?, alarm_status=?, SyncFlag=?", Params);

    if (iAffectedRow < 0)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL OmcDao::DelAlarmLevelCfg(ALARM_LEVEL_CFG_T &stCfg)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;
    INT32           iSyncFlag = SYNC_DEL;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
	Params.Bind(&stCfg.ulDevType);
	Params.Bind(&stCfg.ulAlarmID);

    iAffectedRow = m_pPDBApp->Update("UPDATE alarm_level_cfg SET SyncFlag=? WHERE dev_type=? AND alarm_id=?", Params);

    if (INVALID_ROWCOUNT == iAffectedRow)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL OmcDao::SetAlarmLevelCfg(ALARM_LEVEL_CFG_T &stCfg)
{
    return SetAlarmLevelCfg(stCfg, SYNC_ADD);
}

BOOL OmcDao::GetSyncingAlarmLevel(UINT32 ulMaxRecNum, VECTOR<ALARM_LEVEL_CFG_T> &vAddRec, VECTOR<ALARM_LEVEL_CFG_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    ALARM_LEVEL_CFG_T       stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

	Params.Bind(&stRec.ulDevType);
    Params.Bind(&stRec.ulAlarmID);
    Params.Bind(&stRec.ulAlarmLevel);
    Params.Bind(&stRec.bIsIgnore);
    Params.Bind(&iSyncFlag);

    sprintf(acSQL, "SELECT dev_type, alarm_id, alarm_level, alarm_status, SyncFlag FROM alarm_level_cfg WHERE SyncFlag !=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query alarm_level_cfg failed");
        return FALSE;
    }

    memset(&stRec, 0, sizeof(stRec));
    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        if (iSyncFlag > 0)
        {
            vAddRec.push_back(stRec);
        }
        else
        {
            vDelRec.push_back(stRec);
        }

        if ((vAddRec.size()+vDelRec.size()) >= ulMaxRecNum)
        {
            break;
        }

        memset(&stRec, 0, sizeof(stRec));
    }

    m_pPDBApp->CloseQuery();

    return PDB_ERROR != enRet;
}

BOOL OmcDao::SyncOverAlarmLevel(ALARM_LEVEL_CFG_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

	Params.Bind(&stRec.ulDevType);
    Params.Bind(&stRec.ulAlarmID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE alarm_level_cfg SET SyncFlag=0 WHERE dev_type=? AND alarm_id=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM alarm_level_cfg WHERE dev_type=? AND alarm_id=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL OmcDao::GetStationInfo(VECTOR<STATION_INFO_T> &vInfo)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    STATION_INFO_T          stInfo = {0};
    GJsonParam              JsonParam;

    vInfo.clear();

    Params.Bind(&stInfo.ulStationID);
    Params.Bind(stInfo.acStationName,sizeof(stInfo.acStationName));
    Params.Bind(&stInfo.ulStationType);

    iRow = m_pPDBApp->Query("SELECT StationID, StationName, StationType FROM station WHERE SyncFlag>=0 ORDER BY StationID", Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query sw_info failed");
        return FALSE;
    }

    memset(&stInfo, 0, sizeof(stInfo));
    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        vInfo.push_back(stInfo);
        memset(&stInfo, 0, sizeof(stInfo));
    }

    m_pPDBApp->CloseQuery();

    return enRet != PDB_ERROR;
}

BOOL OmcDao::AddStationInfo(STATION_INFO_T &stInfo)
{
    return SetStationInfo(stInfo, SYNC_ADD);
}

BOOL OmcDao::SetStationInfo(STATION_INFO_T &stInfo, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&stInfo.ulStationID);
    Params.BindString(stInfo.acStationName);
    Params.Bind(&stInfo.ulStationType);
    Params.Bind(&iSyncFlag);

    Params.BindString(stInfo.acStationName);
    Params.Bind(&stInfo.ulStationType);
    Params.Bind(&iSyncFlag);

    iAffectedRow = m_pPDBApp->Update(" INSERT INTO station(StationID, StationName, StationType, SyncFlag) "
                                     " VALUES(?,?,?,?) ON DUPLICATE KEY UPDATE StationName=?, StationType=?, SyncFlag=? ", Params);

    return iAffectedRow > 0;
}

BOOL OmcDao::DelStationInfo(UINT32 ulStationID)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;
    INT32           iSyncFlag = SYNC_DEL;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&ulStationID);

    iAffectedRow = m_pPDBApp->Update("UPDATE station SET SyncFlag=? WHERE StationID=?", Params);
    return TRUE;
}

BOOL OmcDao::GetSyncingStationInfo(UINT32 ulMaxRecNum, VECTOR<STATION_INFO_T> &vAddRec, VECTOR<STATION_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    STATION_INFO_T          stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&stRec.ulStationID);
    Params.Bind(stRec.acStationName, sizeof(stRec.acStationName));
    Params.Bind(&stRec.ulStationType);
    Params.Bind(&iSyncFlag);


    sprintf(acSQL, "SELECT StationID, StationName, StationType, SyncFlag FROM station WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query Station failed");
        return FALSE;
    }

    memset(&stRec, 0, sizeof(stRec));
    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        if (iSyncFlag > 0)
        {
            vAddRec.push_back(stRec);
        }
        else
        {
            vDelRec.push_back(stRec);
        }

        if ((vAddRec.size()+vDelRec.size()) >= ulMaxRecNum)
        {
            break;
        }

        memset(&stRec, 0, sizeof(stRec));
    }

    m_pPDBApp->CloseQuery();

    return PDB_ERROR != enRet;
}

BOOL OmcDao::SyncOverStationInfo(STATION_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stRec.ulStationID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE station SET SyncFlag=0 WHERE StationID=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM station WHERE StationID=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL OmcDao::ClearDevFile(UINT32 ulTime)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&ulTime);

    iAffectedRow = m_pPDBApp->Update("DELETE FROM dev_file WHERE file_time<?", Params);
    return iAffectedRow >= 0;
}

BOOL OmcDao::ClearDevOperLog(UINT32 ulTime)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&ulTime);

    iAffectedRow = m_pPDBApp->Update("DELETE FROM dev_operlog WHERE oper_time<?", Params);
    return iAffectedRow >= 0;
}

BOOL OmcDao::ClearHistoryAlarm(UINT32 ulTime)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&ulTime);

    iAffectedRow = m_pPDBApp->Update("DELETE FROM history_alarm WHERE alarm_time<?", Params);
    return iAffectedRow >= 0;
}

BOOL OmcDao::ClearTrafficStatus(UINT32 ulTime)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&ulTime);

    iAffectedRow = m_pPDBApp->Update("DELETE FROM traffic_status WHERE time<?", Params);
    return iAffectedRow >= 0;
}

BOOL OmcDao::ClearNeBasicStatus(UINT32 ulTime)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&ulTime);

    iAffectedRow = m_pPDBApp->Update("DELETE FROM ne_basic_status WHERE time<?", Params);
    return iAffectedRow >= 0;
}

BOOL OmcDao::ClearHistoryAlarmStatistic(UINT32 ulTime)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&ulTime);

    iAffectedRow = m_pPDBApp->Update("DELETE FROM history_alarm_statistic WHERE time<?", Params);
    return iAffectedRow >= 0;
}

BOOL OmcDao::BackupTxStatus(UINT32 ulStartTime, UINT32 ulEndTime, CHAR *szFile)
{
    INT32                   iRow = 0;
    CHAR                    acSQL[1024];
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    OMA_TX_STATUS_REC_T     stInfo = {0};
    CHAR                    acRec[1024];
    UINT32                  ulLen;
    BOOL                    bFirst = TRUE;
    FILE                    *fp = fopen(szFile, "wb");

    if (!fp)
    {
        return FALSE;
    }

    Params.Bind(&stInfo.ulTime);
    Params.Bind(stInfo.acNeID, sizeof(stInfo.acNeID));
    Params.Bind(&stInfo.stStatus.stLteStatus.ulStationID);
    Params.Bind(&stInfo.stStatus.bOnline);
    Params.Bind(&stInfo.stStatus.stResStatus.ulCpuUsage);
    Params.Bind(&stInfo.stStatus.stResStatus.ulMemUsage);
    Params.Bind(&stInfo.stStatus.stResStatus.ulDiskUsage);
    Params.Bind(&stInfo.stStatus.stResStatus.iDevTemp);
    Params.Bind(&stInfo.stStatus.stLteStatus.ulPCI);
    Params.Bind(&stInfo.stStatus.stLteStatus.iRSRP);
    Params.Bind(&stInfo.stStatus.stLteStatus.iRSRQ);
    Params.Bind(&stInfo.stStatus.stLteStatus.iRSSI);
    Params.Bind(&stInfo.stStatus.stLteStatus.iSINR);
    Params.Bind(&stInfo.stStatus.ulRunStatus);

    sprintf(acSQL, "SELECT time, ne_id, station_id, online, cpu_usage, mem_usage, disk_usage, dev_temp, pci, rsrp, rsrq, rssi, sinr, status FROM ne_basic_status WHERE time>=%u AND time<%u ORDER BY time", ulStartTime, ulEndTime);
    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query ne_basic_status failed");
        fclose(fp);
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        ulLen = sprintf(acRec, "%c{\"Time\":%u, \"NeID\":\"%s\", \"StationID\":%u, \"Online\":%u, \"Cpu\":%u, \"Mem\":%u, \"Disk\":%u,"
                "\"Temp\":%d, \"PCI\":%u, \"RSRP\":%d, \"RSRQ\":%d, \"RSSI\":%d, \"SINR\":%d, \"Status\":%u}",
                bFirst?'[':',',
                stInfo.ulTime,
                stInfo.acNeID,
                stInfo.stStatus.stLteStatus.ulStationID,
                stInfo.stStatus.bOnline,
                stInfo.stStatus.stResStatus.ulCpuUsage,
                stInfo.stStatus.stResStatus.ulMemUsage,
                stInfo.stStatus.stResStatus.ulDiskUsage,
                stInfo.stStatus.stResStatus.iDevTemp,
                stInfo.stStatus.stLteStatus.ulPCI,
                stInfo.stStatus.stLteStatus.iRSRP,
                stInfo.stStatus.stLteStatus.iRSRQ,
                stInfo.stStatus.stLteStatus.iRSSI,
                stInfo.stStatus.stLteStatus.iSINR,
                stInfo.stStatus.ulRunStatus);

        if (bFirst)
        {
            bFirst = FALSE;
        }

        if (ulLen != fwrite(acRec, 1, ulLen, fp))
        {
            enRet = PDB_ERROR;
            break;
        }

        memset(&stInfo, 0, sizeof(stInfo));
    }

    m_pPDBApp->CloseQuery();

    fwrite("]", 1, 1, fp);
    fclose(fp);

    return enRet != PDB_ERROR;
}

BOOL OmcDao::BackupTrafficStatus(UINT32 ulStartTime, UINT32 ulEndTime, CHAR *szFile)
{
    INT32                   iRow = 0;
    CHAR                    acSQL[1024];
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    OMA_TAU_STATUS_REC_T     stInfo = {0};
    CHAR                    acRec[1024];
    UINT32                  ulLen;
    BOOL                    bFirst = TRUE;
    FILE                    *fp = fopen(szFile, "wb");

    if (!fp)
    {
        return FALSE;
    }

    Params.Bind(&stInfo.ulTime);
    Params.Bind(stInfo.acNeID, sizeof(stInfo.acNeID));
    Params.Bind(&stInfo.stStatus.stTrafficStatus.u64WanTxBytes);
    Params.Bind(&stInfo.stStatus.stTrafficStatus.u64WanRxBytes);
    Params.Bind(&stInfo.stStatus.stTrafficStatus.u64Lan1TxBytes);
    Params.Bind(&stInfo.stStatus.stTrafficStatus.u64Lan1RxBytes);
    Params.Bind(&stInfo.stStatus.stTrafficStatus.u64Lan2TxBytes);
    Params.Bind(&stInfo.stStatus.stTrafficStatus.u64Lan2RxBytes);
    Params.Bind(&stInfo.stStatus.stTrafficStatus.u64Lan3TxBytes);
    Params.Bind(&stInfo.stStatus.stTrafficStatus.u64Lan3RxBytes);
    Params.Bind(&stInfo.stStatus.stTrafficStatus.u64Lan4TxBytes);
    Params.Bind(&stInfo.stStatus.stTrafficStatus.u64Lan4RxBytes);

    sprintf(acSQL, " SELECT time, ne_id, wan_tx, wan_rx, lan1_tx, lan1_rx, lan2_tx, lan3_tx, lan3_tx, lan3_rx, lan4_tx, lan4_rx  FROM traffic_status WHERE time>=%u AND time<%u  ORDER BY time", ulStartTime, ulEndTime);
    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query ne_basic_status failed");
        fclose(fp);
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        ulLen = sprintf(acRec, "%c{\"Time\":%u, \"NeID\":\"%s\", \"WanTx\":%llu, \"WanRx\":%llu, \"Lan1Tx\":%llu, \"Lan1Rx\":%llu, "
                                  "\"Lan2Tx\":%llu, \"Lan2Rx\":%llu, \"Lan3Tx\":%llu, \"Lan3Rx\":%llu, \"Lan4Tx\":%llu, \"Lan4Rx\":%llu}",
                bFirst?'[':',',
                stInfo.ulTime,
                stInfo.acNeID,
                stInfo.stStatus.stTrafficStatus.u64WanTxBytes,
                stInfo.stStatus.stTrafficStatus.u64WanRxBytes,
                stInfo.stStatus.stTrafficStatus.u64Lan1TxBytes,
                stInfo.stStatus.stTrafficStatus.u64Lan1RxBytes,
                stInfo.stStatus.stTrafficStatus.u64Lan2TxBytes,
                stInfo.stStatus.stTrafficStatus.u64Lan2RxBytes,
                stInfo.stStatus.stTrafficStatus.u64Lan3TxBytes,
                stInfo.stStatus.stTrafficStatus.u64Lan3RxBytes,
                stInfo.stStatus.stTrafficStatus.u64Lan4TxBytes,
                stInfo.stStatus.stTrafficStatus.u64Lan4RxBytes);

        if (bFirst)
        {
            bFirst = FALSE;
        }

        if (ulLen != fwrite(acRec, 1, ulLen, fp))
        {
            enRet = PDB_ERROR;
            break;
        }

        memset(&stInfo, 0, sizeof(stInfo));
    }

    m_pPDBApp->CloseQuery();

    fwrite("]", 1, 1, fp);
    fclose(fp);

    return enRet != PDB_ERROR;
}

BOOL OmcDao::GetOperLogInfo(UINT32 ulStartTime, UINT32 ulEndTime, CHAR *szOperatorID, UINT32 ulMaxNum, GJsonTupleParam &JsonTupleParam)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    UINT32                  ulTime;
    CHAR                    acOperatorID[32];
    CHAR                    acLog[1024];
    GJsonParam              JsonParam;
    CHAR                    acSQL[256];

    JsonTupleParam.Clear();

    Params.Bind(&ulTime);
    Params.Bind(acOperatorID, sizeof(acOperatorID));
    Params.Bind(acLog, sizeof(acLog));

    if (szOperatorID[0] == '\0')
    {
        sprintf(acSQL, "SELECT Time, UserID, LogInfo FROM operLog WHERE Time>=%u AND Time<=%u AND SyncFlag>=0 ORDER BY Time %s",
                ulStartTime, ulEndTime, ulStartTime==0?"DESC":"");
    }
    else
    {
        sprintf(acSQL, "SELECT Time, UserID, LogInfo FROM operLog WHERE Time>=%u AND Time<=%u AND UserID=%s AND SyncFlag>=0 ORDER BY Time %s",
                ulStartTime, ulEndTime, szOperatorID, ulStartTime==0?"DESC":"");
    }

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        DaoLog(LOG_ERROR, "query OperLog failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        JsonParam.Clear();
        JsonParam.Add("Time", ulTime);
        JsonParam.Add("OperatorID", acOperatorID);
        JsonParam.Add("LogInfo", acLog);

        JsonTupleParam.Add(JsonParam);

        if (JsonTupleParam.GetStringSize() > 30*1024)
        {
            break;
        }
    }

    m_pPDBApp->CloseQuery();

    return PDB_ERROR != enRet;
}

BOOL OmcDao::AddTauStatus(VECTOR<OMA_TAU_STATUS_REC_T> &vInfo)
{
    INT32           iAffectedRow = 0;
    CHAR            acTmp[1024];
    GString         strSQL;
    INT32           iSyncFlag = SYNC_ADD;

    FormatSyncFlag(iSyncFlag);
    if (vInfo.size() == 0)
    {
        return TRUE;
    }

    strSQL.Append(" INSERT INTO ne_basic_status(time, ne_id, online, cpu_usage, mem_usage, disk_usage, disk_logsave_usage, disk_package_usage, disk_model_usage, dev_temp, pci, rsrp, rsrq, rssi, sinr, status, SyncFlag) VALUES ");
    for (UINT32 i=0; i<vInfo.size(); i++)
    {
        OMA_TAU_STATUS_REC_T &stInfo = vInfo[i];

        sprintf(acTmp, "%s(%u, '%s', %u, %u, %u, %u, %u, %u, %u, %d, %u, %d, %d, %d, %d, %u, %d)",
            i==0?"":",",
            stInfo.ulTime,
            stInfo.acNeID,
            stInfo.stStatus.bOnline,
            stInfo.stStatus.stResStatus.ulCpuUsage,
            stInfo.stStatus.stResStatus.ulMemUsage,
            stInfo.stStatus.stResStatus.ulDiskUsage,
            stInfo.stStatus.stResStatus.ulDiskLogsaveUsage,
            stInfo.stStatus.stResStatus.ulDiskPackageUsage,
            stInfo.stStatus.stResStatus.ulDiskModuleUsage,
            stInfo.stStatus.stResStatus.iDevTemp,
            stInfo.stStatus.stLteStatus.ulPCI,
            stInfo.stStatus.stLteStatus.iRSRP,
            stInfo.stStatus.stLteStatus.iRSRQ,
            stInfo.stStatus.stLteStatus.iRSSI,
            stInfo.stStatus.stLteStatus.iSINR,
            stInfo.stStatus.bOnline,
            iSyncFlag);

        strSQL.Append(acTmp);
    }

    iAffectedRow = m_pPDBApp->Update(strSQL.GetString());
    if (iAffectedRow <= 0)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL OmcDao::AddTrafficStatus(VECTOR<OMA_TAU_STATUS_REC_T> &vInfo)
{
    INT32           iAffectedRow = 0;
    CHAR            acTmp[1024]= {0};
    GString         strSQL;
    INT32           iSyncFlag = SYNC_ADD;

    FormatSyncFlag(iSyncFlag);

    if (vInfo.size() == 0)
    {
        return TRUE;
    }

    strSQL.Append(" INSERT INTO traffic_status(time, ne_id, wan_tx, wan_rx, lan1_tx, lan1_rx, lan2_tx, lan2_rx, lan3_tx, lan3_rx, lan4_tx, lan4_rx, SyncFlag) VALUES ");
    for (UINT32 i=0; i<vInfo.size(); i++)
    {
        OMA_TAU_STATUS_REC_T    &stInfo = vInfo[i];

        sprintf(acTmp, "%s(%u, '%s', %llu, %llu, %llu, %llu, %llu, %llu, %llu, %llu, %llu, %llu, %d)",
            i==0?"":",",
            stInfo.ulTime,
            stInfo.acNeID,
            stInfo.stStatus.stTrafficStatus.u64WanTxBytes,
            stInfo.stStatus.stTrafficStatus.u64WanRxBytes,
            stInfo.stStatus.stTrafficStatus.u64Lan1TxBytes,
            stInfo.stStatus.stTrafficStatus.u64Lan1RxBytes,
            stInfo.stStatus.stTrafficStatus.u64Lan2TxBytes,
            stInfo.stStatus.stTrafficStatus.u64Lan2RxBytes,
            stInfo.stStatus.stTrafficStatus.u64Lan3TxBytes,
            stInfo.stStatus.stTrafficStatus.u64Lan3RxBytes,
            stInfo.stStatus.stTrafficStatus.u64Lan4TxBytes,
            stInfo.stStatus.stTrafficStatus.u64Lan4RxBytes,
            iSyncFlag);

        strSQL.Append(acTmp);
    }

    iAffectedRow = m_pPDBApp->Update(strSQL.GetString());
    if (iAffectedRow <= 0)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL OmcDao::AddTxStatus(VECTOR<OMA_TX_STATUS_REC_T> &vInfo)
{
    INT32           iAffectedRow = 0;
    CHAR            acTmp[1024];
    GString         strSQL;
	INT32			iSyncFlag =	SYNC_ADD;

    FormatSyncFlag(iSyncFlag);

    if (vInfo.size() == 0)
    {
        return TRUE;
    }

    strSQL.Append( " INSERT INTO ne_basic_status(time, ne_id, station_id, online, cpu_usage, mem_usage, disk_usage, dev_temp, pci, rsrp, rsrq, rssi, sinr, status, SyncFlag) VALUES ");
    for (UINT32 i=0; i<vInfo.size(); i++)
    {
        OMA_TX_STATUS_REC_T &stInfo = vInfo[i];

        sprintf(acTmp, "%s(%u,'%s', %u, %u, %u, %u, %u, %d, %u, %d, %d, %d, %d, %u, %d)",
                        i==0 ? "":",",
                        stInfo.ulTime,
                        stInfo.acNeID,
                        stInfo.stStatus.stLteStatus.ulStationID,
                        stInfo.stStatus.bOnline,
                        stInfo.stStatus.stResStatus.ulCpuUsage,
                        stInfo.stStatus.stResStatus.ulMemUsage,
                        stInfo.stStatus.stResStatus.ulDiskUsage,
                        stInfo.stStatus.stResStatus.iDevTemp,
                        stInfo.stStatus.stLteStatus.ulPCI,
                        stInfo.stStatus.stLteStatus.iRSRP,
                        stInfo.stStatus.stLteStatus.iRSRQ,
                        stInfo.stStatus.stLteStatus.iRSSI,
                        stInfo.stStatus.stLteStatus.iSINR,
                        stInfo.stStatus.bOnline,
                        iSyncFlag);

        strSQL.Append(acTmp);
    }

    iAffectedRow = m_pPDBApp->Update(strSQL.GetString());
    if (iAffectedRow <= 0)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL OmcDao::AddDCStatus(VECTOR<OMA_DC_STATUS_REC_T> &vInfo)
{
    INT32           iAffectedRow = 0;
    CHAR            acTmp[1024];
    GString         strSQL;
	INT32			iSyncFlag =	SYNC_ADD;

    FormatSyncFlag(iSyncFlag);

    if (vInfo.size() == 0)
    {
        return TRUE;
    }

    strSQL.Append( " INSERT INTO ne_basic_status(time, ne_id, online, cpu_usage, mem_usage, disk_usage, status, SyncFlag) VALUES ");
    for (UINT32 i=0; i<vInfo.size(); i++)
    {
        OMA_DC_STATUS_REC_T &stInfo = vInfo[i];

        sprintf(acTmp, "%s(%u,'%s', %u, %u, %u, %u, %u, %d)",
                        i==0 ? "":",",
                        stInfo.ulTime,
                        stInfo.acNeID,
                        stInfo.stStatus.bOnline,
                        stInfo.stStatus.stResStatus.ulCpuUsage,
                        stInfo.stStatus.stResStatus.ulMemUsage,
                        stInfo.stStatus.stResStatus.ulDiskUsage,
                        stInfo.stStatus.bOnline,
                        iSyncFlag);

        strSQL.Append(acTmp);
    }

    iAffectedRow = m_pPDBApp->Update(strSQL.GetString());
    if (iAffectedRow <= 0)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL OmcDao::AddDISStatus(VECTOR<OMA_DIS_STATUS_REC_T> &vInfo)
{
    INT32           iAffectedRow = 0;
    CHAR            acTmp[1024];
    GString         strSQL;
	INT32			iSyncFlag =	SYNC_ADD;

    FormatSyncFlag(iSyncFlag);

    if (vInfo.size() == 0)
    {
        return TRUE;
    }

    strSQL.Append( " INSERT INTO ne_basic_status(time, ne_id, online, cluster_status, cpu_usage, mem_usage, disk_usage, SyncFlag) VALUES ");
    for (UINT32 i=0; i<vInfo.size(); i++)
    {
        OMA_DIS_STATUS_REC_T &stInfo = vInfo[i];

        sprintf(acTmp, "%s(%u,'%s', %u, %u, %u, %u, %u, %d)",
                        i==0 ? "":",",
                        stInfo.ulTime,
                        stInfo.acNeID,
                        stInfo.stStatus.bOnline,
						stInfo.stStatus.ulClusterStatus,
                        stInfo.stStatus.stResStatus.ulCpuUsage,
                        stInfo.stStatus.stResStatus.ulMemUsage,
                        stInfo.stStatus.stResStatus.ulDiskUsage,
                        iSyncFlag);

        strSQL.Append(acTmp);
    }

    iAffectedRow = m_pPDBApp->Update(strSQL.GetString());
    if (iAffectedRow <= 0)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL OmcDao::AddRECStatus(VECTOR<OMA_REC_STATUS_REC_T> &vInfo)
{
    INT32           iAffectedRow = 0;
    CHAR            acTmp[1024];
    GString         strSQL;
	INT32			iSyncFlag =	SYNC_ADD;

    FormatSyncFlag(iSyncFlag);

    if (vInfo.size() == 0)
    {
        return TRUE;
    }

    strSQL.Append( " INSERT INTO ne_basic_status(time, ne_id, online, cpu_usage, mem_usage, disk_usage, status, SyncFlag) VALUES ");
    for (UINT32 i=0; i<vInfo.size(); i++)
    {
        OMA_REC_STATUS_REC_T &stInfo = vInfo[i];

        sprintf(acTmp, "%s(%u,'%s', %u, %u, %u, %u, %u, %d)",
                        i==0 ? "":",",
                        stInfo.ulTime,
                        stInfo.acNeID,
                        stInfo.stStatus.bOnline,
                        stInfo.stStatus.stResStatus.ulCpuUsage,
                        stInfo.stStatus.stResStatus.ulMemUsage,
                        stInfo.stStatus.stResStatus.ulDiskUsage,
                        stInfo.stStatus.bOnline,
                        iSyncFlag);

        strSQL.Append(acTmp);
    }

    iAffectedRow = m_pPDBApp->Update(strSQL.GetString());
    if (iAffectedRow <= 0)
    {
        return FALSE;
    }

    return TRUE;
}