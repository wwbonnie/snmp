#include "omc_public.h"


extern VOID FormatSyncFlag(INT32 &iSyncFlag);

// group call info

BOOL CfgDao::GetGroupCallInfo(UINT32 ulStartTime, UINT32 ulEndTime, UINT32 ulCallType, UINT32 ulDCUserID, INT64 i64GroupID, UINT32 ulMaxNum, GJsonTupleParam &JsonTupleParam)
{
    INT32           iRow = 0;
    PDBRET          enRet = PDB_CONTINUE;
    PDBRsltParam    Params;
    UINT32          ulTime;
    UINT32          ulSpeaker;
    CHAR            acSQL[512];
    CHAR            acCallType[128] = {0};
    GJsonParam      JsonParam;

    Params.Bind(&ulDCUserID);
    Params.Bind(&i64GroupID);
    Params.Bind(&ulTime);
    Params.Bind(&ulCallType);
    Params.Bind(&ulSpeaker);

    if (ulCallType != CALL_TYPE_UNKNOWN)
    {
        sprintf(acCallType, " AND CallType=%u", ulCallType);
    }
    else
    {
        sprintf(acCallType, " AND CallType>0");
    }

    if (ulStartTime == 0)
    {
        sprintf(acSQL, "SELECT DCUserID, GroupID, Time, CallType, Speaker FROM GroupCallInfo "
            "WHERE Time<=%u AND DCUserID%s%u AND GroupID%s" FORMAT_I64
            "%s AND Speaker>0 AND SyncFlag>=0 ORDER BY Time DESC",
            ulEndTime, ulDCUserID==INVALID_USER_ID?">":"=", ulDCUserID,
            i64GroupID==INVALID_GROUP_ID?">":"=", i64GroupID, acCallType);
    }
    else
    {
        sprintf(acSQL, "SELECT DCUserID, GroupID, Time, CallType, Speaker FROM GroupCallInfo "
            "WHERE Time>=%u AND Time<=%u AND DCUserID%s%u AND GroupID%s" FORMAT_I64
            "%s AND Speaker>0 AND SyncFlag>=0 ORDER BY Time",
            ulStartTime, ulEndTime, ulDCUserID==INVALID_USER_ID?">":"=", ulDCUserID,
            i64GroupID==INVALID_GROUP_ID?">":"=", i64GroupID, acCallType);
    }

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query GroupCallInfo failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        JsonParam.Clear();
        JsonParam.Add("DCUserID", ulDCUserID);
        JsonParam.Add("GroupID", i64GroupID);
        JsonParam.Add("Time", ulTime);
        JsonParam.Add("CallType", ulCallType);
        JsonParam.Add("Speaker", ulSpeaker);

        JsonTupleParam.Add(JsonParam);
        if (JsonTupleParam.GetTupleNum() >= ulMaxNum ||
            JsonTupleParam.GetStringSize() >= 32*1024 )
        {
            break;
        }
    }

    m_pPDBApp->CloseQuery();

    if (PDB_ERROR != enRet)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL CfgDao::SaveGroupCallInfo(GROUP_CALL_INFO_T &stRec)
{
    return SetGroupCallInfo(stRec, SYNC_ADD);
}

BOOL CfgDao::SetGroupCallInfo(GROUP_CALL_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(&stRec.i64GroupID);
    Params.Bind(&stRec.ulTime);
    Params.Bind(&stRec.ulCallType);
    Params.Bind(&stRec.ulSpeaker);
    Params.Bind(&stRec.ulSeqID);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(&stRec.i64GroupID);
    Params.Bind(&stRec.ulTime);
    Params.Bind(&stRec.ulCallType);
    Params.Bind(&stRec.ulSpeaker);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO GroupCallInfo(SyncFlag, DCUserID, GroupID, Time, CallType, Speaker, SeqID) VALUES(?,?,?,?,?,?,?) "
                                     "ON DUPLICATE KEY UPDATE SyncFlag=?, DCUserID=?, GroupID=?, Time=?, CallType=?, Speaker=?", Params);
    return iAffectedRow > 0;
}

BOOL CfgDao::GetSyncingGroupCallInfo(UINT32 ulMaxRecNum, VECTOR<GROUP_CALL_INFO_T> &vAddRec, VECTOR<GROUP_CALL_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    GROUP_CALL_INFO_T       stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(&stRec.i64GroupID);
    Params.Bind(&stRec.ulTime);
    Params.Bind(&stRec.ulCallType);
    Params.Bind(&stRec.ulSpeaker);
    Params.Bind(&stRec.ulSeqID);

    sprintf(acSQL, "SELECT SyncFlag, DCUserID, GroupID, Time, CallType, Speaker, SeqID FROM GroupCallInfo WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query GroupCallInfo failed");
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

BOOL CfgDao::SyncOverGroupCallInfo(GROUP_CALL_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stRec.ulSeqID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE GroupCallInfo SET SyncFlag=0 WHERE SeqID=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM GroupCallInfo WHERE SeqID=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

// P2P call info

BOOL CfgDao::GetP2PCallInfo(UINT32 ulStartTime, UINT32 ulEndTime, UINT32 ulCallType, UINT32 ulUserID, UINT32 ulMaxNum, GJsonTupleParam &JsonTupleParam)
{
    INT32           iRow = 0;
    PDBRET          enRet = PDB_CONTINUE;
    PDBRsltParam    Params;
    UINT32          ulCaller;
    UINT32          ulCallee;
    UINT32          ulTime;
    UINT32          ulStatus;
    CHAR            acSQL[512];
    CHAR            acCallType[128] = {0};
    CHAR            acUser[128] = {0};
    GJsonParam      JsonParam;

    Params.Bind(&ulCaller);
    Params.Bind(&ulCallee);
    Params.Bind(&ulTime);
    Params.Bind(&ulCallType);
    Params.Bind(&ulStatus);

    if (ulCallType != CALL_TYPE_UNKNOWN)
    {
        sprintf(acCallType, " AND CallType=%u", ulCallType);
    }

    if (ulUserID != INVALID_USER_ID)
    {
        sprintf(acUser, " AND (Caller=%u OR Callee=%u)", ulUserID, ulUserID);
    }

    if (ulStartTime == 0)
    {
        sprintf(acSQL, "SELECT Caller, Callee, Time, CallType, Status FROM P2PCallInfo "
            "WHERE Time<=%u%s%s AND SyncFlag>=0 ORDER BY Time DESC",
            ulEndTime, acCallType, acUser);
    }
    else
    {
        sprintf(acSQL, "SELECT Caller, Callee, Time, CallType, Status FROM P2PCallInfo "
            "WHERE Time>=%u AND Time<=%u%s%s AND SyncFlag>=0 ORDER BY Time",
            ulStartTime, ulEndTime, acCallType, acUser);
    }

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query P2PCallInfo failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        JsonParam.Clear();
        JsonParam.Add("Caller", ulCaller);
        JsonParam.Add("Callee", ulCallee);
        JsonParam.Add("Time", ulTime);
        JsonParam.Add("CallType", ulCallType);
        JsonParam.Add("Status", ulStatus);

        JsonTupleParam.Add(JsonParam);
        if (JsonTupleParam.GetTupleNum() >= ulMaxNum ||
            JsonTupleParam.GetStringSize() >= 32*1024 )
        {
            break;
        }
    }

    m_pPDBApp->CloseQuery();

    if (PDB_ERROR != enRet)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL CfgDao::SaveP2PCallInfo(P2P_CALL_INFO_T &stRec)
{
    return SetP2PCallInfo(stRec, SYNC_ADD);
}

BOOL CfgDao::SetP2PCallInfo(P2P_CALL_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulCaller);
    Params.Bind(&stRec.ulCallee);
    Params.Bind(&stRec.ulTime);
    Params.Bind(&stRec.ulCallType);
    Params.Bind(&stRec.ulStatus);
    Params.Bind(&stRec.ulSeqID);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulCaller);
    Params.Bind(&stRec.ulCallee);
    Params.Bind(&stRec.ulTime);
    Params.Bind(&stRec.ulCallType);
    Params.Bind(&stRec.ulStatus);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO P2PCallInfo(SyncFlag, Caller, Callee, Time, CallType, Status, SeqID) VALUES(?,?,?,?,?,?,?) "
                                     "ON DUPLICATE KEY UPDATE SyncFlag=?, Caller=?, Callee=?, Time=?, CallType=?, Status=?", Params);
    return iAffectedRow > 0;
}

BOOL CfgDao::GetSyncingP2PCallInfo(UINT32 ulMaxRecNum, VECTOR<P2P_CALL_INFO_T> &vAddRec, VECTOR<P2P_CALL_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    P2P_CALL_INFO_T     stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulCaller);
    Params.Bind(&stRec.ulCallee);
    Params.Bind(&stRec.ulTime);
    Params.Bind(&stRec.ulCallType);
    Params.Bind(&stRec.ulStatus);
    Params.Bind(&stRec.ulSeqID);

    sprintf(acSQL, "SELECT SyncFlag, Caller, Callee, Time, CallType, Status, SeqID FROM P2PCallInfo WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query P2PCallInfo failed");
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

BOOL CfgDao::SyncOverP2PCallInfo(P2P_CALL_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stRec.ulSeqID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE P2PCallInfo SET SyncFlag=0 WHERE SeqID=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM P2PCallInfo WHERE SeqID=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

// IPH Talk Info
BOOL CfgDao::GetIPHTalkInfo(UINT32 ulStartTime, UINT32 ulEndTime, GJsonTupleParam &JsonTupleParam)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    CHAR                    acSQL[1024];
    UINT32                  ulLen;
    IPH_TALK_INFO_T         stInfo = {0};

    JsonTupleParam.Clear();

    Params.Bind(&stInfo.ulApplyTime);
    Params.Bind(&stInfo.ulTrainUnitID);
    Params.Bind(stInfo.acTrainID, sizeof(stInfo.acTrainID));
    Params.Bind(&stInfo.ulCarriageID);
    Params.Bind(&stInfo.ulIPHDevID);
    Params.Bind(&stInfo.ulAcceptDCUserID);
    Params.Bind(&stInfo.ulAcceptTime);
    Params.Bind(&stInfo.ulStopTime);

    ulLen = sprintf(acSQL, "SELECT ApplyTime, TrainUnitID, TrainID, CarriageID, IPHDevID, AcceptDCUserID, AcceptTime, StopTime "
        "FROM IPHTalkInfo WHERE ApplyTime>=%u AND ApplyTime<=%u AND SyncFlag>=0", ulStartTime, ulEndTime);

    sprintf(acSQL+ulLen, " ORDER BY ApplyTime, TrainUnitID, CarriageID, IPHDevID");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query IPHTalkInfo failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        GJsonParam          JsonParam;

        JsonParam.Add("ApplyTime", stInfo.ulApplyTime);
        JsonParam.Add("TrainUnitID", stInfo.ulTrainUnitID);
        JsonParam.Add("TrainID", stInfo.acTrainID);
        JsonParam.Add("CarriageID", stInfo.ulCarriageID);
        JsonParam.Add("IPHDevID", stInfo.ulIPHDevID);
        JsonParam.Add("AcceptDCUserID", stInfo.ulAcceptDCUserID);
        JsonParam.Add("AcceptTime", stInfo.ulAcceptTime);
        JsonParam.Add("StopTime", stInfo.ulStopTime);

        JsonTupleParam.Add(JsonParam);
        if (JsonTupleParam.GetStringSize() > 32*1024)
        {
            break;
        }

        memset(&stInfo, 0, sizeof(stInfo));
    }

    m_pPDBApp->CloseQuery();

    return PDB_ERROR != enRet;
}

BOOL CfgDao::NewIPHTalk(VECTOR<IPH_TALK_INFO_T> &vInfo)
{
/*  INT32           iAffectedRow = 0;
    PDBParam        Params;
    IPH_TALK_INFO_T stInfo;

    Params.Bind(&stInfo.ulApplyTime);
    Params.Bind(&stInfo.ulTrainUnitID);
    Params.Bind(stInfo.acTrainID, sizeof(stInfo.acTrainID));
    Params.Bind(&stInfo.ulCarriageID);
    Params.Bind(&stInfo.ulIPHDevID);
    Params.Bind(&stInfo.ulAcceptDCUserID);
    Params.Bind(&stInfo.ulAcceptTime);
    Params.Bind(&stInfo.ulStopTime);

    TransBegin();
    for (UINT32 i=0; i<vInfo.size(); i++)
    {
        memcpy(&stInfo, &vInfo[i], sizeof(stInfo));

        iAffectedRow = m_pPDBApp->Update("INSERT INTO IPHTalkInfo(ApplyTime, TrainUnitID, TrainID, CarriageID, IPHDevID, AcceptDCUserID, AcceptTime, StopTime)"
            " VALUES(?,?,?,?,?,?,?,?)", Params);
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

    return TRUE; */

    TransBegin();

    for (UINT32 i=0; i<vInfo.size(); i++)
    {
        if (!SetIPHTalkInfo(vInfo[i], SYNC_ADD))
        {
            TransRollback();
            return FALSE;
        }
    }

    return TransCommit();
}

BOOL CfgDao::UpdateIPHTalk(IPH_TALK_INFO_T &stInfo)
{
    return SetIPHTalkInfo(stInfo, SYNC_ADD);
}

BOOL CfgDao::SetIPHTalkInfo(IPH_TALK_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulApplyTime);
    Params.Bind(&stRec.ulTrainUnitID);
    Params.BindString(stRec.acTrainID);
    Params.Bind(&stRec.ulCarriageID);
    Params.Bind(&stRec.ulIPHDevID);
    Params.Bind(&stRec.ulAcceptDCUserID);
    Params.Bind(&stRec.ulAcceptTime);
    Params.Bind(&stRec.ulStopTime);

    Params.Bind(&iSyncFlag);
    Params.BindString(stRec.acTrainID);
    Params.Bind(&stRec.ulAcceptDCUserID);
    Params.Bind(&stRec.ulAcceptTime);
    Params.Bind(&stRec.ulStopTime);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO IPHTalkInfo(SyncFlag, ApplyTime, TrainUnitID, TrainID, CarriageID, IPHDevID, AcceptDCUserID, AcceptTime, StopTime) VALUES(?,?,?,?,?,?,?,?,?) "
        "ON DUPLICATE KEY UPDATE SyncFlag=?, TrainID=?, AcceptDCUserID=?, AcceptTime=?, StopTime=?", Params);
    return iAffectedRow > 0;
}

BOOL CfgDao::GetSyncingIPHTalkInfo(UINT32 ulMaxRecNum, VECTOR<IPH_TALK_INFO_T> &vAddRec, VECTOR<IPH_TALK_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    IPH_TALK_INFO_T         stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulApplyTime);
    Params.Bind(&stRec.ulTrainUnitID);
    Params.Bind(stRec.acTrainID, sizeof(stRec.acTrainID));
    Params.Bind(&stRec.ulCarriageID);
    Params.Bind(&stRec.ulIPHDevID);
    Params.Bind(&stRec.ulAcceptDCUserID);
    Params.Bind(&stRec.ulAcceptTime);
    Params.Bind(&stRec.ulStopTime);

    sprintf(acSQL, "SELECT SyncFlag, ApplyTime, TrainUnitID, TrainID, CarriageID, IPHDevID, AcceptDCUserID, AcceptTime, StopTime FROM IPHTalkInfo WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query IPHTalkInfo failed");
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

BOOL CfgDao::SyncOverIPHTalkInfo(IPH_TALK_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stRec.ulApplyTime);
    Params.Bind(&stRec.ulTrainUnitID);
    Params.Bind(&stRec.ulCarriageID);
    Params.Bind(&stRec.ulIPHDevID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE IPHTalkInfo SET SyncFlag=0 WHERE ApplyTime=? AND TrainUnitID=? AND CarriageID=? AND IPHDevID=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM IPHTalkInfo WHERE ApplyTime=? AND TrainUnitID=? AND CarriageID=? AND IPHDevID=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

// Req call info
BOOL CfgDao::GetReqCallInfo(UINT32 ulStartTime, UINT32 ulEndTime, UINT32 ulReqCallType, UINT32 ulMaxNum, GJsonTupleParam &JsonTupleParam)
{
    INT32           iRow = 0;
    PDBRET          enRet = PDB_CONTINUE;
    PDBRsltParam    Params;
    REQ_CALL_INFO_T stInfo = {0};
    CHAR            acSQL[512];
    CHAR            acReqCall[128] = {0};
    GJsonParam      JsonParam;

    Params.Bind(&stInfo.ulReqTime);
    Params.Bind(&stInfo.ulReqCallType);
    Params.Bind(&stInfo.ulReqUserID);
    Params.Bind(&stInfo.ulCaller);
    Params.Bind(&stInfo.ulDCUserID);
    Params.Bind(&stInfo.ulAcceptTime);

    if (ulReqCallType != INVALID_REQ_CALL)
    {
        sprintf(acReqCall, " AND ReqCallType=%u", ulReqCallType);
    }

    if (ulStartTime == 0)
    {
        sprintf(acSQL, "SELECT ReqTime, ReqCallType, ReqUserID, Caller, DCUserID, AcceptTime FROM ReqCallInfo "
            "WHERE ReqTime<=%u AND SyncFlag>=0 %s ORDER BY ReqTime DESC",
            ulEndTime, acReqCall);
    }
    else
    {
        sprintf(acSQL, "SELECT ReqTime, ReqCallType, ReqUserID, Caller, DCUserID, AcceptTime FROM ReqCallInfo "
            "WHERE ReqTime>=%u AND ReqTime<=%u AND SyncFlag>=0 %s ORDER BY ReqTime",
            ulStartTime, ulEndTime, acReqCall);
    }

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query ReqCallInfo failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        JsonParam.Clear();
        JsonParam.Add("ReqTime", stInfo.ulReqTime);
        JsonParam.Add("ReqCallType", stInfo.ulReqCallType);
        JsonParam.Add("ReqUserID", stInfo.ulReqUserID);
        JsonParam.Add("Caller", stInfo.ulCaller);
        JsonParam.Add("DCUserID", stInfo.ulDCUserID);
        JsonParam.Add("AcceptTime", stInfo.ulAcceptTime);

        JsonTupleParam.Add(JsonParam);
        if (JsonTupleParam.GetTupleNum() >= ulMaxNum ||
            JsonTupleParam.GetStringSize() >= 32*1024 )
        {
            break;
        }

        memset(&stInfo, 0, sizeof(stInfo));
    }

    m_pPDBApp->CloseQuery();

    if (PDB_ERROR != enRet)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL CfgDao::AddReqCallInfo(REQ_CALL_INFO_T &stInfo)
{
    return SetReqCallInfo(stInfo, SYNC_ADD);
}

BOOL CfgDao::UpdateReqCallInfo(UINT32 ulReqTime, UINT32 ulReqUserID, UINT32 ulDCUserID)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;
    UINT32          ulAcceptTime = gos_get_current_time();
    INT32           iSyncFlag = SYNC_ADD;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);;
    Params.Bind(&ulDCUserID);
    Params.Bind(&ulAcceptTime);
    Params.Bind(&ulReqTime);
    Params.Bind(&ulReqUserID);

    iAffectedRow = m_pPDBApp->Update("UPDATE ReqCallInfo SET SyncFlag=?, DCUserID=?, AcceptTime=? WHERE ReqTime=? AND ReqUserID=?", Params);
    if (iAffectedRow < 0)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL CfgDao::SetReqCallInfo(REQ_CALL_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulReqTime);
    Params.Bind(&stRec.ulReqCallType);
    Params.Bind(&stRec.ulReqUserID);
    Params.Bind(&stRec.ulCaller);
    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(&stRec.ulAcceptTime);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulReqCallType);
    Params.Bind(&stRec.ulCaller);
    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(&stRec.ulAcceptTime);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO ReqCallInfo(SyncFlag, ReqTime, ReqCallType, ReqUserID, Caller, DCUserID, AcceptTime) VALUES(?,?,?,?,?,?,?) "
                                     "ON DUPLICATE KEY UPDATE SyncFlag=?, ReqCallType=?, Caller=?, DCUserID=?, AcceptTime=?", Params);
    return iAffectedRow > 0;
}

BOOL CfgDao::GetSyncingReqCallInfo(UINT32 ulMaxRecNum, VECTOR<REQ_CALL_INFO_T> &vAddRec, VECTOR<REQ_CALL_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    REQ_CALL_INFO_T         stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulReqTime);
    Params.Bind(&stRec.ulReqCallType);
    Params.Bind(&stRec.ulReqUserID);
    Params.Bind(&stRec.ulCaller);
    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(&stRec.ulAcceptTime);

    sprintf(acSQL, "SELECT SyncFlag, ReqTime, ReqCallType, ReqUserID, Caller, DCUserID, AcceptTime FROM ReqCallInfo WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query ReqCallInfo failed");
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

BOOL CfgDao::SyncOverReqCallInfo(REQ_CALL_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stRec.ulReqTime);
    Params.Bind(&stRec.ulReqUserID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE ReqCallInfo SET SyncFlag=0 WHERE ReqTime=? AND ReqUserID=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM ReqCallInfo WHERE ReqTime=? AND ReqUserID=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

// real brd info
BOOL CfgDao::GetRealBrdInfo(UINT32 ulStartTime, UINT32 ulEndTime, UINT32 ulMaxNum, GJsonTupleParam &JsonTupleParam)
{
    INT32           iRow = 0;
    PDBRET          enRet = PDB_CONTINUE;
    PDBRsltParam    Params;
    REAL_BRD_INFO_T stInfo = {0};
    CHAR            acSQL[512];
    GJsonParam      JsonParam;

    Params.Bind(&stInfo.ulStartTime);
    Params.Bind(&stInfo.ulDCUserID);
    Params.Bind(&stInfo.ulEndTime);
    Params.Bind(stInfo.acTrainList, sizeof(stInfo.acTrainList));

    if (ulStartTime == 0)
    {
        sprintf(acSQL, "SELECT StartTime, DCUserID, EndTime, TrainList FROM RealBrdInfo "
            "WHERE StartTime<=%u AND SyncFlag>=0 ORDER BY StartTime DESC",
            ulEndTime);
    }
    else
    {
        sprintf(acSQL, "SELECT StartTime, DCUserID, EndTime, TrainList FROM RealBrdInfo "
            "WHERE StartTime>=%u AND StartTime<=%u AND SyncFlag>=0 ORDER BY StartTime",
            ulStartTime, ulEndTime);
    }

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query RealBrdInfo failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        JsonParam.Clear();
        JsonParam.Add("StartTime", stInfo.ulStartTime);
        JsonParam.Add("DCUserID", stInfo.ulDCUserID);
        JsonParam.Add("EndTime", stInfo.ulEndTime);
        JsonParam.Add("TrainList", stInfo.acTrainList);

        JsonTupleParam.Add(JsonParam);
        if (JsonTupleParam.GetTupleNum() >= ulMaxNum ||
            JsonTupleParam.GetStringSize() >= 32*1024 )
        {
            break;
        }

        memset(&stInfo, 0, sizeof(stInfo));
    }

    m_pPDBApp->CloseQuery();

    if (PDB_ERROR != enRet)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL CfgDao::SaveRealBrdInfo(REAL_BRD_INFO_T &stInfo)
{
    return SetRealBrdInfo(stInfo, SYNC_ADD);
}

BOOL CfgDao::SetRealBrdInfo(REAL_BRD_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulStartTime);
    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(&stRec.ulEndTime);
    Params.BindString(stRec.acTrainList);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulEndTime);
    Params.BindString(stRec.acTrainList);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO RealBrdInfo(SyncFlag, StartTime, DCUserID, EndTime, TrainList) VALUES(?,?,?,?,?) "
                                     "ON DUPLICATE KEY UPDATE SyncFlag=?, EndTime=?, TrainList=?", Params);
    return iAffectedRow > 0;
}

BOOL CfgDao::GetSyncingRealBrdInfo(UINT32 ulMaxRecNum, VECTOR<REAL_BRD_INFO_T> &vAddRec, VECTOR<REAL_BRD_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    REAL_BRD_INFO_T         stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulStartTime);
    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(&stRec.ulEndTime);
    Params.Bind(stRec.acTrainList, sizeof(stRec.acTrainList));

    sprintf(acSQL, "SELECT SyncFlag, StartTime, DCUserID, EndTime, TrainList FROM RealBrdInfo WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query RealBrdInfo failed");
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

BOOL CfgDao::SyncOverRealBrdInfo(REAL_BRD_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stRec.ulStartTime);
    Params.Bind(&stRec.ulDCUserID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE RealBrdInfo SET SyncFlag=0 WHERE StartTime=? AND DCUserID=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM RealBrdInfo WHERE StartTime=? AND DCUserID=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

// rec brd info

BOOL CfgDao::GetRecBrdInfo(UINT32 ulStartTime, UINT32 ulEndTime, UINT32 ulMaxNum, GJsonTupleParam &JsonTupleParam)
{
    INT32           iRow = 0;
    PDBRET          enRet = PDB_CONTINUE;
    PDBRsltParam    Params;
    REC_BRD_INFO_T  stInfo = {0};
    CHAR            acSQL[512];
    GJsonParam      JsonParam;

    Params.Bind(&stInfo.ulTime);
    Params.Bind(&stInfo.ulRecID);
    Params.Bind(stInfo.acRecInfo, sizeof(stInfo.acRecInfo));
    Params.Bind(&stInfo.ulBrdLoopTime);
    Params.Bind(&stInfo.ulDCUserID);
    Params.Bind(stInfo.acTrainList, sizeof(stInfo.acTrainList));

    if (ulStartTime == 0)
    {
        sprintf(acSQL, "SELECT Time, RecID, RecInfo, BrdLoopTime, DCUserID, TrainList FROM RecBrdInfo "
            "WHERE Time<=%u AND SyncFlag>=0 ORDER BY Time DESC",
            ulEndTime);
    }
    else
    {
        sprintf(acSQL, "SELECT Time, RecID, RecInfo, BrdLoopTime, DCUserID, TrainList FROM RecBrdInfo "
            "WHERE Time>=%u AND Time<=%u AND SyncFlag>=0 ORDER BY Time",
            ulStartTime, ulEndTime);
    }

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query RecBrdInfo failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        JsonParam.Clear();
        JsonParam.Add("Time", stInfo.ulTime);
        JsonParam.Add("RecID", stInfo.ulRecID);
        JsonParam.Add("RecInfo", stInfo.acRecInfo);
        JsonParam.Add("BrdLoopTime", stInfo.ulBrdLoopTime);
        JsonParam.Add("DCUserID", stInfo.ulDCUserID);
        JsonParam.Add("TrainList", stInfo.acTrainList);

        JsonTupleParam.Add(JsonParam);
        if (JsonTupleParam.GetTupleNum() >= ulMaxNum ||
            JsonTupleParam.GetStringSize() >= 32*1024 )
        {
            break;
        }

        memset(&stInfo, 0, sizeof(stInfo));
    }

    m_pPDBApp->CloseQuery();

    if (PDB_ERROR != enRet)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL CfgDao::AddRecBrdInfo(REC_BRD_INFO_T *pstInfo, VECTOR<UINT32> &vTrainUnitID)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;
    UINT32          ulTrainUnitID;
    UINT32          ulReplyTime = 0;
    INT32           iResult = REC_BRD_RESULT_NULL;
    INT32           iSyncFlag = SYNC_ADD;

    //  TransBegin();
    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&pstInfo->ulSeqID);
    Params.Bind(&pstInfo->ulTime);
    Params.Bind(&pstInfo->ulRecID);
    Params.BindString(pstInfo->acRecInfo);
    Params.Bind(&pstInfo->ulBrdLoopTime);
    Params.Bind(&pstInfo->ulDCUserID);
    Params.BindString(pstInfo->acTrainList);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO RecBrdInfo(SyncFlag, BrdSeqID, Time, RecID, RecInfo, BrdLoopTime, DCUserID, TrainList) VALUES(?,?,?,?,?,?,?,?)", Params);
    if (iAffectedRow <= 0)
    {
        goto fail;
    }

    // 优先保障记录插入
    TransBegin();

    for (UINT32 i=0; i<vTrainUnitID.size(); i++)
    {
        ulTrainUnitID = vTrainUnitID[i];

        Params.Clear();
        Params.Bind(&iSyncFlag);
        Params.Bind(&pstInfo->ulSeqID);
        Params.Bind(&ulTrainUnitID);
        Params.Bind(&ulReplyTime);
        Params.Bind(&iResult);

        iAffectedRow = m_pPDBApp->Update("INSERT INTO RecBrdReply(SyncFlag, BrdSeqID, TrainUnitID, ReplyTime, Result) VALUES(?,?,?,?,?)", Params);
        if (iAffectedRow <= 0)
        {
            goto fail;
        }
    }

    if (!TransCommit())
    {
        goto fail;
    }

    return TRUE;

fail:
    TransRollback();
    return FALSE;
}

BOOL CfgDao::UpdateRecBrdReply(UINT32 ulBrdSeqID, UINT32 ulTrainUnitID, UINT32 ulReplyTime, INT32 iResult)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;
    INT32           iSyncFlag = SYNC_ADD;

    //  TransBegin();
    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&ulReplyTime);
    Params.Bind(&iResult);
    Params.Bind(&ulBrdSeqID);
    Params.Bind(&ulTrainUnitID);

    iAffectedRow = m_pPDBApp->Update("UPDATE RecBrdReply SET SyncFlag=?, ReplyTime=?, Result=? WHERE BrdSeqID=? AND TrainUnitID=?", Params);
    if (iAffectedRow < 0)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL CfgDao::SetRecBrdInfo(REC_BRD_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulSeqID);
    Params.Bind(&stRec.ulTime);
    Params.Bind(&stRec.ulRecID);
    Params.BindString(stRec.acRecInfo);
    Params.Bind(&stRec.ulBrdLoopTime);
    Params.Bind(&stRec.ulDCUserID);
    Params.BindString(stRec.acTrainList);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulTime);
    Params.Bind(&stRec.ulRecID);
    Params.BindString(stRec.acRecInfo);
    Params.Bind(&stRec.ulBrdLoopTime);
    Params.Bind(&stRec.ulDCUserID);
    Params.BindString(stRec.acTrainList);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO RecBrdInfo(SyncFlag, BrdSeqID, Time, RecID, RecInfo, BrdLoopTime, DCUserID, TrainList) VALUES(?,?,?,?,?,?,?,?) "
                                     "ON DUPLICATE KEY UPDATE SyncFlag=?, Time=?, RecID=?, RecInfo=?, BrdLoopTime=?, DCUserID=?, TrainList=?", Params);
    return iAffectedRow > 0;
}

BOOL CfgDao::GetSyncingRecBrdInfo(UINT32 ulMaxRecNum, VECTOR<REC_BRD_INFO_T> &vAddRec, VECTOR<REC_BRD_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    REC_BRD_INFO_T          stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulSeqID);
    Params.Bind(&stRec.ulTime);
    Params.Bind(&stRec.ulRecID);
    Params.Bind(stRec.acRecInfo, sizeof(stRec.acRecInfo));
    Params.Bind(&stRec.ulBrdLoopTime);
    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(stRec.acTrainList, sizeof(stRec.acTrainList));

    sprintf(acSQL, "SELECT SyncFlag, BrdSeqID, Time, RecID, RecInfo, BrdLoopTime, DCUserID, TrainList FROM RecBrdInfo WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query RecBrdInfo failed");
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

BOOL CfgDao::SyncOverRecBrdInfo(REC_BRD_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stRec.ulSeqID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE RecBrdInfo SET SyncFlag=0 WHERE BrdSeqID=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM RecBrdInfo WHERE BrdSeqID=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

// sds info

BOOL CfgDao::GetSDSInfo(UINT32 ulStartTime, UINT32 ulEndTime, UINT32 ulSDSType,
                        CHAR *szSendUserName, CHAR *szKeyword, UINT32 ulMaxNum, VECTOR<SDS_INFO_T> &vInfo)
{
    INT32                   iRow = 0;
    PDBRET                  enRet;
    SDS_INFO_T              stInfo;
    PDBRsltParam            Params;
    CHAR                    acSQL[1024];
    UINT32                  ulLen;

    if (0 == ulEndTime)
    {
        ulEndTime = 0xffffffff;
    }

    Params.Bind(&stInfo.ulSDSID);
    Params.Bind(&stInfo.ulSendUserID);
    Params.Bind(stInfo.acSendUserName, sizeof(stInfo.acSendUserName));
    Params.Bind(&stInfo.ulSendTime);
    Params.Bind(&stInfo.ulSDSType);
//  Params.Bind(&stInfo.ulSDSFormat);
    Params.Bind(&stInfo.ulSDSIndex);
    Params.Bind(stInfo.acSDSInfo, sizeof(stInfo.acSDSInfo));
    Params.Bind(&stInfo.bNeedReply);
    Params.Bind(&stInfo.bSendFlag);
    Params.Bind(&stInfo.ulRecvUserNum);
    Params.Bind(&stInfo.ulRecvSuccUserNum);
    Params.Bind(&stInfo.ulReplyUserNum);

    ulLen = sprintf(acSQL, "SELECT SDSID, SendUserID, SendUserName, SendTime, SDSType, "
                    "SDSIndex, SDSInfo, NeedReply, SendFlag, RecvUserNum, RecvSuccUserNum, ReplyUserNum FROM SDSInfo "
                    "WHERE SendTime>=%u AND SendTime<=%u AND SyncFlag>=0", ulStartTime, ulEndTime);
    if (ulSDSType != SDS_TYPE_NULL)
    {
        ulLen += sprintf(acSQL+ulLen, " AND SDSType=%u", ulSDSType);
    }
/*
    if (ulSDSFormat != SDS_FORMAT_NULL)
    {
        ulLen += sprintf(acSQL+ulLen, " AND SDSFormat=%u", ulSDSFormat);
    }
    */

    /*
    if (ulSendUserID != INVALID_USER_ID)
    {
        ulLen += sprintf(acSQL+ulLen, " AND SendUserID=%u", ulSendUserID);
    }*/

    if (szSendUserName && szSendUserName[0] != '\0')
    {
        ulLen += sprintf(acSQL+ulLen, " AND SendUserName LIKE %s%s%s", "'%", szSendUserName, "%'");
    }

    if (szKeyword && szKeyword[0] != '\0')
    {
        ulLen += sprintf(acSQL+ulLen, " AND SDSInfo LIKE %s%s%s", "'%", szKeyword, "%'");
    }

    sprintf(acSQL+ulLen, "%s", " ORDER BY SendTime");

    iRow = m_pPDBApp->Query(acSQL, Params);
    vInfo.clear();
    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query SDSInfo failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        vInfo.push_back(stInfo);
        if (vInfo.size() == ulMaxNum)
        {
            break;
        }
    }

    m_pPDBApp->CloseQuery();

    if (PDB_ERROR == enRet)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL CfgDao::GetLatestSDSInfo(UINT32 ulMaxNum, UINT32 ulDCUserID, VECTOR<SDS_INFO_T> &vInfo)
{
    INT32                   iRow = 0;
    PDBRET                  enRet;
    SDS_INFO_T              stInfo;
    PDBRsltParam            Params;
    CHAR                    acSQL[1024];
    UINT32                  ulLen;
    UINT32                  ulStartTime = gos_get_current_time();
    GOS_DATETIME_T          stTime;

    gos_get_localtime(&ulStartTime, &stTime);
    stTime.ucHour = 0;
    stTime.ucMinute = 0;
    stTime.ucSecond = 0;
    stTime.ucSn = 0;

    ulStartTime = gos_mktime(&stTime);

    Params.Bind(&stInfo.ulSDSID);
    Params.Bind(&stInfo.ulSendUserID);
    Params.Bind(stInfo.acSendUserName, sizeof(stInfo.acSendUserName));
    Params.Bind(&stInfo.ulSendTime);
    Params.Bind(&stInfo.ulSDSType);
//  Params.Bind(&stInfo.ulSDSFormat);
    Params.Bind(&stInfo.ulSDSIndex);
    Params.Bind(stInfo.acSDSInfo, sizeof(stInfo.acSDSInfo));
    Params.Bind(&stInfo.bNeedReply);
    Params.Bind(&stInfo.bSendFlag);
    Params.Bind(&stInfo.ulRecvUserNum);
    Params.Bind(&stInfo.ulRecvSuccUserNum);
    Params.Bind(&stInfo.ulReplyUserNum);
    Params.Bind(&stInfo.ulRecvDCUserID);

    ulLen = sprintf(acSQL, "SELECT SDSID, SendUserID, SendUserName, SendTime, SDSType, "
                    "SDSIndex, SDSInfo, NeedReply, SendFlag, RecvUserNum, RecvSuccUserNum, ReplyUserNum, RecvUserID "
                    "FROM SDSInfo WHERE SendTime>=%u AND SyncFlag>=0 ORDER BY SendTime DESC",
                    ulStartTime);

    iRow = m_pPDBApp->Query(acSQL, Params);
    vInfo.clear();
    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query SDSInfo failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        if (ulDCUserID == stInfo.ulSendUserID ||
            ulDCUserID == stInfo.ulRecvDCUserID )
        {
            vInfo.push_back(stInfo);
        }

        if (vInfo.size() == ulMaxNum)
        {
            break;
        }
    }

    m_pPDBApp->CloseQuery();

    if (PDB_ERROR == enRet)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL CfgDao::AddSDSInfo(SDS_INFO_T &stRec)
{
    return SetSDSInfo(stRec, SYNC_ADD);
    /*INT32         iAffectedRow = 0;
    PDBParam        Params;

    stRec.ulRecvSuccUserNum = 0;
    stRec.ulReplyUserNum = 0;

    Params.Bind(&stRec.ulSDSID);
    Params.Bind(&stRec.ulSendUserID);
    Params.Bind(stRec.acSendUserName, sizeof(stRec.acSendUserName));
    Params.Bind(&stRec.ulSendTime);
    Params.Bind(&stRec.ulSDSType);
    Params.Bind(&stRec.ulSDSIndex);
    Params.Bind(stRec.acSDSInfo, sizeof(stRec.acSDSInfo));
    Params.Bind(&stRec.bNeedReply);
    Params.Bind(&stRec.bSendFlag);
    Params.Bind(&stRec.ulRecvUserNum);
    Params.Bind(&stRec.ulRecvSuccUserNum);
    Params.Bind(&stRec.ulReplyUserNum);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO SDSInfo(SDSID, SendUserID, SendUserName, SendTime, SDSType, "
        "SDSIndex, SDSInfo, NeedReply, SendFlag, RecvUserNum, RecvSuccUserNum, ReplyUserNum) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)", Params);
    if (iAffectedRow <= 0)
    {
        return FALSE;
    }

    return TRUE; */
}

BOOL CfgDao::SetSDSInfo(SDS_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulSDSID);
    Params.Bind(&stRec.ulSendUserID);
    Params.BindString(stRec.acSendUserName);
    Params.Bind(&stRec.ulSendTime);
    Params.Bind(&stRec.ulSDSType);
    Params.Bind(&stRec.ulSDSIndex);
    Params.BindString(stRec.acSDSInfo);
    Params.Bind(&stRec.bNeedReply);
    Params.Bind(&stRec.bSendFlag);
    Params.Bind(&stRec.ulRecvUserNum);
    Params.Bind(&stRec.ulRecvSuccUserNum);
    Params.Bind(&stRec.ulReplyUserNum);
    Params.Bind(&stRec.ulRecvDCUserID);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulSendUserID);
    Params.BindString(stRec.acSendUserName);
    Params.Bind(&stRec.ulSendTime);
    Params.Bind(&stRec.ulSDSType);
    Params.Bind(&stRec.ulSDSIndex);
    Params.BindString(stRec.acSDSInfo);
    Params.Bind(&stRec.bNeedReply);
    Params.Bind(&stRec.bSendFlag);
    Params.Bind(&stRec.ulRecvUserNum);
    Params.Bind(&stRec.ulRecvSuccUserNum);
    Params.Bind(&stRec.ulReplyUserNum);
    Params.Bind(&stRec.ulRecvDCUserID);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO SDSInfo(SyncFlag, SDSID, SendUserID, SendUserName, SendTime, "
            "SDSType, SDSIndex, SDSInfo, NeedReply, SendFlag, RecvUserNum, RecvSuccUserNum, ReplyUserNum, RecvUserID) "
            "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE SyncFlag=?, SendUserID=?, SendUserName=?, SendTime=?, "
            "SDSType=?, SDSIndex=?, SDSInfo=?, NeedReply=?, SendFlag=?, RecvUserNum=?, RecvSuccUserNum=?, ReplyUserNum=?, RecvUserID=?", Params);
    return iAffectedRow > 0;
}

BOOL CfgDao::GetSyncingSDSInfo(UINT32 ulMaxRecNum, VECTOR<SDS_INFO_T> &vAddRec, VECTOR<SDS_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    SDS_INFO_T              stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulSDSID);
    Params.Bind(&stRec.ulSendUserID);
    Params.Bind(stRec.acSendUserName, sizeof(stRec.acSendUserName));
    Params.Bind(&stRec.ulSendTime);
    Params.Bind(&stRec.ulSDSType);
    Params.Bind(&stRec.ulSDSIndex);
    Params.Bind(stRec.acSDSInfo, sizeof(stRec.acSDSInfo));
    Params.Bind(&stRec.bNeedReply);
    Params.Bind(&stRec.bSendFlag);
    Params.Bind(&stRec.ulRecvUserNum);
    Params.Bind(&stRec.ulRecvSuccUserNum);
    Params.Bind(&stRec.ulReplyUserNum);


    sprintf(acSQL, "SELECT SyncFlag, SDSID, SendUserID, SendUserName, SendTime, "
        "SDSType, SDSIndex, SDSInfo, NeedReply, SendFlag, RecvUserNum, RecvSuccUserNum, ReplyUserNum FROM SDSInfo WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query SDSInfo failed");
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

BOOL CfgDao::SyncOverSDSInfo(SDS_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stRec.ulSDSID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE SDSInfo SET SyncFlag=0 WHERE SDSID=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM SDSInfo WHERE SDSID=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

// sub group
BOOL CfgDao::GetSubGroup(UINT32 ulDCUserID, VECTOR<INT64> &vGroupID)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    INT64                   i64GroupID;
    CHAR                    acSQL[256];

    vGroupID.clear();
    Params.Bind(&i64GroupID);

    sprintf(acSQL, "SELECT GroupID FROM SubGroup WHERE DCUserID=%u AND SyncFlag>=0", ulDCUserID);
    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query subgroup failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        vGroupID.push_back(i64GroupID);
    }

    m_pPDBApp->CloseQuery();

    if (PDB_ERROR != enRet)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL CfgDao::SaveSubGroup(UINT32 ulDCUserID, VECTOR<INT64> &vDelGroup, VECTOR<INT64> &vAddGroup)
{
    INT32           iAffectedRow = 0;
    CHAR            acTmp[128];
    GString         strSQL;
    INT32           iSyncFlag;

    iSyncFlag = SYNC_DEL;
    FormatSyncFlag(iSyncFlag);

    TransBegin();

    if (vDelGroup.size() > 0)
    {
        sprintf(acTmp, "UPDATE SubGroup SET SyncFlag=%d WHERE DCUserID=%u AND GroupID IN ", iSyncFlag, ulDCUserID);
        strSQL.Append(acTmp);
        for (UINT32 i=0; i<vDelGroup.size(); i++)
        {
            sprintf(acTmp, "%s" FORMAT_I64 , i==0?"(":",", vDelGroup[i]);
            strSQL.Append(acTmp);
        }

        strSQL.Append(")");

        iAffectedRow = m_pPDBApp->Update(strSQL.GetString());
        if (iAffectedRow < 0)
        {
            goto fail;
        }
    }

    if (vAddGroup.size() > 0)
    {
        iSyncFlag = SYNC_ADD;
        FormatSyncFlag(iSyncFlag);

        strSQL.Clear();
        strSQL.Append("INSERT INTO SubGroup(SyncFlag, DCUserID, GroupID) VALUES ");
        for (UINT32 i=0; i<vAddGroup.size(); i++)
        {
            sprintf(acTmp, "%s(%d, %u, " FORMAT_I64 ")", i==0?"":",", iSyncFlag, ulDCUserID, vAddGroup[i]);
            strSQL.Append(acTmp);
        }

        iAffectedRow = m_pPDBApp->Update(strSQL.GetString());
        if (iAffectedRow <= 0)
        {
            goto fail;
        }
    }

    return TransCommit();

fail:
    TransRollback();
    return FALSE;
}

BOOL CfgDao::SetSubGroup(SUB_GROUP_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(&stRec.i64GroupID);

    Params.Bind(&iSyncFlag);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO SubGroup(SyncFlag, DCUserID, GroupID) "
                                "VALUES(?,?,?) ON DUPLICATE KEY UPDATE SyncFlag=?", Params);
    return iAffectedRow > 0;
}

BOOL CfgDao::GetSyncingSubGroup(UINT32 ulMaxRecNum, VECTOR<SUB_GROUP_T> &vAddRec, VECTOR<SUB_GROUP_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    SUB_GROUP_T             stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(&stRec.i64GroupID);

    sprintf(acSQL, "SELECT SyncFlag, DCUserID, GroupID FROM SubGroup WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query SubGroup failed");
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

BOOL CfgDao::SyncOverSubGroup(SUB_GROUP_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(&stRec.i64GroupID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE SubGroup SET SyncFlag=0 WHERE DCUserID=? AND GroupID=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM SubGroup WHERE DCUserID=? AND GroupID=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

// sub Video Group
BOOL CfgDao::GetSubVideoGroup(UINT32 ulDCUserID, VECTOR<INT64> &vGroupID)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    INT64                   i64GroupID;
    CHAR                    acSQL[256];

    vGroupID.clear();
    Params.Bind(&i64GroupID);

    sprintf(acSQL, "SELECT GroupID FROM SubVideoGroup WHERE DCUserID=%u AND SyncFlag>=0", ulDCUserID);
    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query SubVideoGroup failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        vGroupID.push_back(i64GroupID);
    }

    m_pPDBApp->CloseQuery();

    if (PDB_ERROR != enRet)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL CfgDao::SaveSubVideoGroup(UINT32 ulDCUserID, VECTOR<INT64> &vDelGroup, VECTOR<INT64> &vAddGroup)
{
    INT32           iAffectedRow = 0;
    CHAR            acTmp[128];
    GString         strSQL;
    INT32           iSyncFlag;

    iSyncFlag = SYNC_DEL;
    FormatSyncFlag(iSyncFlag);

    TransBegin();

    if (vDelGroup.size() > 0)
    {
        sprintf(acTmp, "UPDATE SubVideoGroup SET SyncFlag=%d WHERE DCUserID=%u AND GroupID IN ", iSyncFlag, ulDCUserID);
        strSQL.Append(acTmp);
        for (UINT32 i=0; i<vDelGroup.size(); i++)
        {
            sprintf(acTmp, "%s" FORMAT_I64 , i==0?"(":",", vDelGroup[i]);
            strSQL.Append(acTmp);
        }

        strSQL.Append(")");

        iAffectedRow = m_pPDBApp->Update(strSQL.GetString());
        if (iAffectedRow < 0)
        {
            goto fail;
        }
    }

    if (vAddGroup.size() > 0)
    {
        iSyncFlag = SYNC_ADD;
        FormatSyncFlag(iSyncFlag);

        strSQL.Clear();
        strSQL.Append("INSERT INTO SubVideoGroup(SyncFlag, DCUserID, GroupID) VALUES ");
        for (UINT32 i=0; i<vAddGroup.size(); i++)
        {
            sprintf(acTmp, "%s(%d, %u, " FORMAT_I64 ")", i==0?"":",", iSyncFlag, ulDCUserID, vAddGroup[i]);
            strSQL.Append(acTmp);
        }

        iAffectedRow = m_pPDBApp->Update(strSQL.GetString());
        if (iAffectedRow <= 0)
        {
            goto fail;
        }
    }

    return TransCommit();

fail:
    TransRollback();
    return FALSE;
}

BOOL CfgDao::SetSubVideoGroup(SUB_GROUP_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(&stRec.i64GroupID);

    Params.Bind(&iSyncFlag);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO SubVideoGroup(SyncFlag, DCUserID, GroupID) "
                            "VALUES(?,?,?) ON DUPLICATE KEY UPDATE SyncFlag=?", Params);
    return iAffectedRow > 0;
}

BOOL CfgDao::GetSyncingSubVideoGroup(UINT32 ulMaxRecNum, VECTOR<SUB_GROUP_T> &vAddRec, VECTOR<SUB_GROUP_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    SUB_GROUP_T             stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(&stRec.i64GroupID);

    sprintf(acSQL, "SELECT SyncFlag, DCUserID, GroupID FROM SubVideoGroup WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query SubVideoGroup failed");
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

BOOL CfgDao::SyncOverSubVideoGroup(SUB_GROUP_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(&stRec.i64GroupID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE SubVideoGroup SET SyncFlag=0 WHERE DCUserID=? AND GroupID=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM SubVideoGroup WHERE DCUserID=? AND GroupID=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

// sub user
BOOL CfgDao::GetSubUser(UINT32 ulDCUserID, VECTOR<UINT32> &vUserID)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    UINT32                  ulUserID;
    CHAR                    acSQL[256];

    vUserID.clear();
    Params.Bind(&ulUserID);

    sprintf(acSQL, "SELECT UserID FROM SubUser WHERE DCUserID=%u  AND SyncFlag>=0", ulDCUserID);
    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query subuser failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        vUserID.push_back(ulUserID);
    }

    m_pPDBApp->CloseQuery();

    if (PDB_ERROR != enRet)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL CfgDao::SaveSubUser(UINT32 ulDCUserID, VECTOR<UINT32> &vDelUser, VECTOR<UINT32> &vAddUser)
{
    INT32           iAffectedRow = 0;
    CHAR            acTmp[128];
    GString         strSQL;
    INT32           iSyncFlag;

    iSyncFlag = SYNC_DEL;
    FormatSyncFlag(iSyncFlag);

    TransBegin();

    if (vDelUser.size() > 0)
    {
        sprintf(acTmp, "UPDATE SubUser SET SyncFlag=%d WHERE DCUserID=%u AND UserID IN ", iSyncFlag, ulDCUserID);
        strSQL.Append(acTmp);
        for (UINT32 i=0; i<vDelUser.size(); i++)
        {
            sprintf(acTmp, "%s%u", i==0?"(":",", vDelUser[i]);
            strSQL.Append(acTmp);
        }

        strSQL.Append(")");

        iAffectedRow = m_pPDBApp->Update(strSQL.GetString());
        if (iAffectedRow < 0)
        {
            goto fail;
        }
    }

    if (vAddUser.size() > 0)
    {
        iSyncFlag = SYNC_ADD;
        FormatSyncFlag(iSyncFlag);

        strSQL.Clear();
        strSQL.Append("INSERT INTO SubUser(SyncFlag, DCUserID, UserID) VALUES ");
        for (UINT32 i=0; i<vAddUser.size(); i++)
        {
            sprintf(acTmp, "%s(%d, %u, %u)", i==0?"":",", iSyncFlag, ulDCUserID, vAddUser[i]);
            strSQL.Append(acTmp);
        }

        iAffectedRow = m_pPDBApp->Update(strSQL.GetString());
        if (iAffectedRow <= 0)
        {
            goto fail;
        }
    }

    return TransCommit();

fail:
    TransRollback();
    return FALSE;
}

BOOL CfgDao::SetSubUser(SUB_USER_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(&stRec.ulUserID);

    Params.Bind(&iSyncFlag);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO SubUser(SyncFlag, DCUserID, UserID) "
        "VALUES(?,?,?) ON DUPLICATE KEY UPDATE SyncFlag=?", Params);
    return iAffectedRow > 0;
}

BOOL CfgDao::GetSyncingSubUser(UINT32 ulMaxRecNum, VECTOR<SUB_USER_T> &vAddRec, VECTOR<SUB_USER_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    SUB_USER_T              stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(&stRec.ulUserID);

    sprintf(acSQL, "SELECT SyncFlag, DCUserID, UserID FROM SubUser WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query SubUser failed");
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

BOOL CfgDao::SyncOverSubUser(SUB_USER_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(&stRec.ulUserID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE SubUser SET SyncFlag=0 WHERE DCUserID=? AND UserID=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM SubUser WHERE DCUserID=? AND UserID=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

// sub video group
BOOL CfgDao::GetSubVideoUser(UINT32 ulDCUserID, VECTOR<UINT32> &vUserID)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    UINT32                  ulUserID;
    CHAR                    acSQL[256];

    vUserID.clear();
    Params.Bind(&ulUserID);

    sprintf(acSQL, "SELECT UserID FROM SubVideoUser WHERE DCUserID=%u  AND SyncFlag>=0", ulDCUserID);
    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query SubVideoUser failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        vUserID.push_back(ulUserID);
    }

    m_pPDBApp->CloseQuery();

    if (PDB_ERROR != enRet)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL CfgDao::SaveSubVideoUser(UINT32 ulDCUserID, VECTOR<UINT32> &vDelUser, VECTOR<UINT32> &vAddUser)
{
    INT32           iAffectedRow = 0;
    CHAR            acTmp[128];
    GString         strSQL;
    INT32           iSyncFlag;

    iSyncFlag = SYNC_DEL;
    FormatSyncFlag(iSyncFlag);

    TransBegin();

    if (vDelUser.size() > 0)
    {
        sprintf(acTmp, "UPDATE SubVideoUser SET SyncFlag=%d WHERE DCUserID=%u AND UserID IN ", iSyncFlag, ulDCUserID);
        strSQL.Append(acTmp);
        for (UINT32 i=0; i<vDelUser.size(); i++)
        {
            sprintf(acTmp, "%s%u", i==0?"(":",", vDelUser[i]);
            strSQL.Append(acTmp);
        }

        strSQL.Append(")");

        iAffectedRow = m_pPDBApp->Update(strSQL.GetString());
        if (iAffectedRow < 0)
        {
            goto fail;
        }
    }

    if (vAddUser.size() > 0)
    {
        iSyncFlag = SYNC_ADD;
        FormatSyncFlag(iSyncFlag);

        strSQL.Clear();
        strSQL.Append("INSERT INTO SubVideoUser(SyncFlag, DCUserID, UserID) VALUES ");
        for (UINT32 i=0; i<vAddUser.size(); i++)
        {
            sprintf(acTmp, "%s(%d, %u, %u)", i==0?"":",", iSyncFlag, ulDCUserID, vAddUser[i]);
            strSQL.Append(acTmp);
        }

        iAffectedRow = m_pPDBApp->Update(strSQL.GetString());
        if (iAffectedRow <= 0)
        {
            goto fail;
        }
    }

    return TransCommit();

fail:
    TransRollback();
    return FALSE;
}

BOOL CfgDao::SetSubVideoUser(SUB_USER_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(&stRec.ulUserID);

    Params.Bind(&iSyncFlag);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO SubVideoUser(SyncFlag, DCUserID, UserID) "
        "VALUES(?,?,?) ON DUPLICATE KEY UPDATE SyncFlag=?", Params);
    return iAffectedRow > 0;
}

BOOL CfgDao::GetSyncingSubVideoUser(UINT32 ulMaxRecNum, VECTOR<SUB_USER_T> &vAddRec, VECTOR<SUB_USER_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    SUB_USER_T              stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(&stRec.ulUserID);

    sprintf(acSQL, "SELECT SyncFlag, DCUserID, UserID FROM SubVideoUser WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query SubVideoUser failed");
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

BOOL CfgDao::SyncOverSubVideoUser(SUB_USER_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stRec.ulDCUserID);
    Params.Bind(&stRec.ulUserID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE SubVideoUser SET SyncFlag=0 WHERE DCUserID=? AND UserID=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM SubVideoUser WHERE DCUserID=? AND UserID=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL CfgDao::AddCellInfo(VECTOR<CELL_INFO_T> &vCellInfo)
{
    INT32           iAffectedRow = 0;
    CHAR            acTmp[128];
    GString         strSQL;
    INT32           iSyncFlag = SYNC_ADD;
    BOOL            bRet = TRUE;

    FormatSyncFlag(iSyncFlag);

    if (vCellInfo.size() == 0)
    {
        return TRUE;
    }

    for (UINT32 i=0; i<vCellInfo.size(); i++)
    {
        if (strSQL.Length() == 0)
        {
            m_pPDBApp->TransBegin();
            strSQL.Append("INSERT INTO CellInfo(SyncFlag, Time, UserID, TrainUnitID, CellID, StationID, UpsideStationID, DownsideStationID, TransferTrackFlag, TurnbackTrackFlag, Direction) VALUES ");
        }

        sprintf(acTmp, "%s(%d, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u)", i==0?"":",",
                iSyncFlag,
                vCellInfo[i].ulTime,
                vCellInfo[i].ulTrainUserID,
                vCellInfo[i].ulTrainUnitID,
                vCellInfo[i].ulCellID,
                vCellInfo[i].ulStationID,
                vCellInfo[i].ulUpsideStationID,
                vCellInfo[i].ulDownsideStationID,
                vCellInfo[i].ulTransferTrackFlag,
                vCellInfo[i].ulTurnbackTrackFlag,
                vCellInfo[i].ulDirection);
        strSQL.Append(acTmp);

        if (strSQL.Length() >= 30*1024 ||
            (i+1) == vCellInfo.size() )
        {
            iAffectedRow = m_pPDBApp->Update(strSQL.GetString());
            strSQL.Clear();
            if (iAffectedRow <= 0)
            {
                bRet = FALSE;
                m_pPDBApp->TransRollback();
            }
            else
            {
                if (!m_pPDBApp->TransCommit())
                {
                    bRet = FALSE;
                }
            }
        }
    }

    return bRet;
}

BOOL CfgDao::SetCellInfo(CELL_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulTime);
    Params.Bind(&stRec.ulTrainUserID);
    Params.Bind(&stRec.ulTrainUnitID);
    Params.Bind(&stRec.ulCellID);
    Params.Bind(&stRec.ulStationID);
    Params.Bind(&stRec.ulUpsideStationID);
    Params.Bind(&stRec.ulDownsideStationID);
    Params.Bind(&stRec.ulTransferTrackFlag);
    Params.Bind(&stRec.ulTurnbackTrackFlag);
    Params.Bind(&stRec.ulDirection);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulTrainUnitID);
    Params.Bind(&stRec.ulCellID);
    Params.Bind(&stRec.ulStationID);
    Params.Bind(&stRec.ulUpsideStationID);
    Params.Bind(&stRec.ulDownsideStationID);
    Params.Bind(&stRec.ulTransferTrackFlag);
    Params.Bind(&stRec.ulTurnbackTrackFlag);
    Params.Bind(&stRec.ulDirection);

    Params.Bind(&iSyncFlag);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO CellInfo(SyncFlag, Time, UserID, TrainUnitID, CellID, StationID, UpsideStationID, DownsideStationID, TransferTrackFlag, TurnbackTrackFlag, Direction) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE SyncFlag=?, TrainUnitID=?, CellID=?, StationID=?, UpsideStationID=?, DownsideStationID=?, TransferTrackFlag=?, TurnbackTrackFlag=?, Direction=?", Params);
    return iAffectedRow > 0;
}

BOOL CfgDao::GetSyncingCellInfo(UINT32 ulMaxRecNum, VECTOR<CELL_INFO_T> &vAddRec, VECTOR<CELL_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    CELL_INFO_T             stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulTime);
    Params.Bind(&stRec.ulTrainUserID);
    Params.Bind(&stRec.ulTrainUnitID);
    Params.Bind(&stRec.ulCellID);
    Params.Bind(&stRec.ulStationID);
    Params.Bind(&stRec.ulUpsideStationID);
    Params.Bind(&stRec.ulDownsideStationID);
    Params.Bind(&stRec.ulTransferTrackFlag);
    Params.Bind(&stRec.ulTurnbackTrackFlag);
    Params.Bind(&stRec.ulDirection);

    sprintf(acSQL, "SELECT SyncFlag, Time, UserID, TrainUnitID, CellID, StationID, UpsideStationID, DownsideStationID, TransferTrackFlag, TurnbackTrackFlag, Direction FROM CellInfo WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query CellInfo failed");
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

BOOL CfgDao::SyncOverCellInfo(CELL_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stRec.ulTime);
    Params.Bind(&stRec.ulTrainUserID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE CellInfo SET SyncFlag=0 WHERE Time=? AND UserID=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM CellInfo WHERE Time=? AND UserID=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

// group info
BOOL CfgDao::GetGroupInfo(VECTOR<GROUP_INFO_T> &vGroup)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    GROUP_INFO_T            stGroup = {0};

    vGroup.clear();

    Params.Bind(&stGroup.i64GroupID);
    Params.Bind(stGroup.acGroupName, sizeof(stGroup.acGroupName));
    Params.Bind(&stGroup.ulGroupType);
    Params.Bind(&stGroup.ulFuncType);

    iRow = m_pPDBApp->Query("SELECT GroupID, GroupName, GroupType, FuncType FROM GroupInfo WHERE SyncFlag>=0 ORDER BY GroupID", Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query group failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        vGroup.push_back(stGroup);
        memset(&stGroup, 0, sizeof(stGroup));
    }

    m_pPDBApp->CloseQuery();

    return (PDB_ERROR != enRet);
}

BOOL CfgDao::SaveGroupInfo(GROUP_INFO_T *pstGroup)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;
    UINT32          ulGroupType = STATIC_GROUP;

    Params.Bind(&pstGroup->i64GroupID);
    Params.BindString(pstGroup->acGroupName);
    Params.Bind(&ulGroupType);
    Params.Bind(&pstGroup->ulFuncType);

    iAffectedRow = m_pPDBApp->Update((CHAR*)"INSERT INTO GroupInfo(GroupID, GroupName, GroupType, FuncType) VALUES(?,?,?,?)", Params);
    if (iAffectedRow > 0)
    {
        return TRUE;
    }

    Params.Clear();
    Params.BindString(pstGroup->acGroupName);
    Params.Bind(&ulGroupType);
    Params.Bind(&pstGroup->ulFuncType);
    Params.Bind(&pstGroup->i64GroupID);

    iAffectedRow = m_pPDBApp->Update((CHAR*)"UPDATE GroupInfo SET GroupName=?, GroupType=?, FuncType=? WHERE GroupID=?", Params);
    if (iAffectedRow > 0)
    {
        return TRUE;
    }

    return FALSE;
}

BOOL CfgDao::SetGroupInfo(GROUP_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.i64GroupID);
    Params.BindString(stRec.acGroupName);
    Params.Bind(&stRec.ulGroupType);
    Params.Bind(&stRec.ulFuncType);

    Params.Bind(&iSyncFlag);
    Params.BindString(stRec.acGroupName);
    Params.Bind(&stRec.ulGroupType);
    Params.Bind(&stRec.ulFuncType);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO GroupInfo(SyncFlag, GroupID, GroupName, GroupType, FuncType) "
        "VALUES(?,?,?,?,?) ON DUPLICATE KEY UPDATE SyncFlag=?, GroupName=?, GroupType=?, FuncType=?", Params);
    return iAffectedRow > 0;
}

BOOL CfgDao::GetSyncingGroupInfo(UINT32 ulMaxRecNum, VECTOR<GROUP_INFO_T> &vAddRec, VECTOR<GROUP_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    GROUP_INFO_T                stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.i64GroupID);
    Params.Bind(stRec.acGroupName, sizeof(stRec.acGroupName));
    Params.Bind(&stRec.ulGroupType);
    Params.Bind(&stRec.ulFuncType);


    sprintf(acSQL, "SELECT SyncFlag, GroupID, GroupName, GroupType, FuncType FROM GroupInfo WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query GroupInfo failed");
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

BOOL CfgDao::SyncOverGroupInfo(GROUP_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stRec.i64GroupID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE GroupInfo SET SyncFlag=0 WHERE GroupID=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM GroupInfo WHERE GroupID=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

// train
BOOL CfgDao::GetTrainInfo(VECTOR<TRAIN_T> &vTrain)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    TRAIN_T                 stTrain = {0};

    vTrain.clear();

    Params.Bind(&stTrain.ulTrainUnitID);
    Params.Bind(&stTrain.ulTrainType);
    Params.Bind(&stTrain.i64DefaultGroupID);

    iRow = m_pPDBApp->Query("SELECT TrainUnitID, TrainType, DefaultGroupID FROM Train ORDER BY TrainUnitID", Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query train failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        vTrain.push_back(stTrain);
        memset(&stTrain, 0, sizeof(stTrain));
    }

    m_pPDBApp->CloseQuery();

    if (PDB_ERROR != enRet)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL CfgDao::AddTrainInfo(TRAIN_T &stInfo)
{
    return SetTrainInfo(stInfo, SYNC_ADD);
}

BOOL CfgDao::DelTrainInfo(UINT32 ulTrainUnitID)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;
    INT32           iSyncFlag;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&ulTrainUnitID);

    iAffectedRow = m_pPDBApp->Update("UPDATE Train SET SyncFlag=? WHERE TrainUnitID=?", Params);
    if (iAffectedRow < 0)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL CfgDao::SetTrainInfo(TRAIN_T &stInfo)
{
    return SetTrainInfo(stInfo, SYNC_ADD);
}

BOOL CfgDao::SetTrainInfo(TRAIN_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulTrainUnitID);
    Params.Bind(&stRec.ulTrainType);
    Params.Bind(&stRec.i64DefaultGroupID);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulTrainType);
    Params.Bind(&stRec.i64DefaultGroupID);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO Train(SyncFlag, TrainUnitID, TrainType, DefaultGroupID) "
        "VALUES(?,?,?,?) ON DUPLICATE KEY UPDATE SyncFlag=?, TrainType=?, DefaultGroupID=?", Params);

    return iAffectedRow > 0;
}

BOOL CfgDao::GetSyncingTrainInfo(UINT32 ulMaxRecNum, VECTOR<TRAIN_T> &vAddRec, VECTOR<TRAIN_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    TRAIN_T             stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulTrainUnitID);
    Params.Bind(&stRec.ulTrainType);
    Params.Bind(&stRec.i64DefaultGroupID);

    sprintf(acSQL, "SELECT SyncFlag, TrainUnitID, TrainType, DefaultGroupID FROM Train WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query Train failed");
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

BOOL CfgDao::SyncOverTrainInfo(TRAIN_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stRec.ulTrainUnitID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE Train SET SyncFlag=0 WHERE TrainUnitID=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM Train WHERE TrainUnitID=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

// station UE
BOOL CfgDao::GetStationUE(VECTOR<STATION_UE_INFO_T> &vUser)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    STATION_UE_INFO_T       stUser = {0};

    vUser.clear();

    Params.Bind(&stUser.ulUserID);
    Params.Bind(stUser.acName, sizeof(stUser.acName));
    Params.Bind(stUser.acLanAddr, sizeof(stUser.acLanAddr));
    Params.Bind(&stUser.ulStationID);
    Params.Bind(&stUser.i64DefaultGroupID);

    iRow = m_pPDBApp->Query("SELECT UserID, Name, LanAddr, StationID, DefaultGroup FROM StationUE WHERE SyncFlag>=0 ORDER BY UserID", Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query StationUE failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        vUser.push_back(stUser);
        memset(&stUser, 0, sizeof(stUser));
    }

    m_pPDBApp->CloseQuery();

    return PDB_ERROR != enRet;
}

BOOL CfgDao::AddStationUE(STATION_UE_INFO_T &stInfo)
{
    return SetStationUE(stInfo, SYNC_ADD);
}

BOOL CfgDao::DelStationUE(UINT32 ulUserID)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;
    INT32           iSyncFlag;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&ulUserID);

    iAffectedRow = m_pPDBApp->Update("UPDATE StationUE SET SyncFlag=? WHERE UserID=?", Params);
    if (iAffectedRow < 0)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL CfgDao::SetStationUE(STATION_UE_INFO_T &stInfo)
{
    return SetStationUE(stInfo, SYNC_ADD);
}

BOOL CfgDao::SetStationUE(STATION_UE_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulUserID);
    Params.BindString(stRec.acName);
    Params.BindString(stRec.acLanAddr);
    Params.Bind(&stRec.ulStationID);
    Params.Bind(&stRec.i64DefaultGroupID);

    Params.Bind(&iSyncFlag);
    Params.BindString(stRec.acName);
    Params.BindString(stRec.acLanAddr);
    Params.Bind(&stRec.ulStationID);
    Params.Bind(&stRec.i64DefaultGroupID);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO StationUE(SyncFlag, UserID, Name, LanAddr, StationID, DefaultGroup) "
        "VALUES(?,?,?,?,?,?) ON DUPLICATE KEY UPDATE SyncFlag=?, Name=?, LanAddr=?, StationID=?, DefaultGroup=?", Params);

    return iAffectedRow > 0;
}

BOOL CfgDao::GetSyncingStationUE(UINT32 ulMaxRecNum, VECTOR<STATION_UE_INFO_T> &vAddRec, VECTOR<STATION_UE_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    STATION_UE_INFO_T       stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulUserID);
    Params.Bind(stRec.acName, sizeof(stRec.acName));
    Params.Bind(stRec.acLanAddr, sizeof(stRec.acLanAddr));
    Params.Bind(&stRec.ulStationID);
    Params.Bind(&stRec.i64DefaultGroupID);

    sprintf(acSQL, "SELECT SyncFlag, UserID, Name, LanAddr, StationID, DefaultGroup FROM StationUE WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query StationUE failed");
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

BOOL CfgDao::SyncOverStationUE(STATION_UE_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stRec.ulUserID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE StationUE SET SyncFlag=0 WHERE UserID=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM StationUE WHERE UserID=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

// train UE

BOOL CfgDao::GetTrainUE(VECTOR<TRAIN_UE_INFO_T> &vUser)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    TRAIN_UE_INFO_T         stUser = {0};

    vUser.clear();

    Params.Bind(&stUser.ulUserID);
    Params.Bind(stUser.acName, sizeof(stUser.acName));
    Params.Bind(stUser.acLanAddr, sizeof(stUser.acLanAddr));
    Params.Bind(&stUser.ulTrainUnitID);

    iRow = m_pPDBApp->Query("SELECT UserID, Name, LanAddr, TrainUnitID FROM TrainUE WHERE SyncFlag>=0 ORDER BY UserID", Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query TrainUE failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        vUser.push_back(stUser);
        memset(&stUser, 0, sizeof(stUser));
    }

    m_pPDBApp->CloseQuery();

    return PDB_ERROR != enRet;
}

BOOL CfgDao::AddTrainUE(TRAIN_UE_INFO_T &stInfo)
{
    return SetTrainUE(stInfo, SYNC_ADD);
}

BOOL CfgDao::DelTrainUE(UINT32 ulUserID)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;
    INT32           iSyncFlag;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&ulUserID);

    iAffectedRow = m_pPDBApp->Update("UPDATE TrainUE SET SyncFlag=? WHERE UserID=?", Params);

    if (INVALID_ROWCOUNT == iAffectedRow)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL CfgDao::SetTrainUE(TRAIN_UE_INFO_T &stInfo)
{
    return SetTrainUE(stInfo, SYNC_ADD);
}

BOOL CfgDao::SetTrainUE(TRAIN_UE_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulUserID);
    Params.BindString(stRec.acName);
    Params.BindString(stRec.acLanAddr);
    Params.Bind(&stRec.ulTrainUnitID);

    Params.Bind(&iSyncFlag);
    Params.BindString(stRec.acName);
    Params.BindString(stRec.acLanAddr);
    Params.Bind(&stRec.ulTrainUnitID);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO TrainUE(SyncFlag, UserID, Name, LanAddr, TrainUnitID) "
        "VALUES(?,?,?,?,?) ON DUPLICATE KEY UPDATE SyncFlag=?, Name=?, LanAddr=?, TrainUnitID=?", Params);

    return iAffectedRow > 0;
}

BOOL CfgDao::GetSyncingTrainUE(UINT32 ulMaxRecNum, VECTOR<TRAIN_UE_INFO_T> &vAddRec, VECTOR<TRAIN_UE_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    TRAIN_UE_INFO_T     stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulUserID);
    Params.Bind(stRec.acName, sizeof(stRec.acName));
    Params.Bind(stRec.acLanAddr, sizeof(stRec.acLanAddr));
    Params.Bind(&stRec.ulTrainUnitID);

    sprintf(acSQL, "SELECT SyncFlag, UserID, Name, LanAddr, TrainUnitID FROM TrainUE WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query TrainUE failed");
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

BOOL CfgDao::SyncOverTrainUE(TRAIN_UE_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stRec.ulUserID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE TrainUE SET SyncFlag=0 WHERE UserID=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM TrainUE WHERE UserID=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

// portal UE
BOOL CfgDao::GetPortalUE(VECTOR<PORTAL_UE_INFO_T> &vUser)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    PORTAL_UE_INFO_T        stUser = {0};

    vUser.clear();

    Params.Bind(&stUser.ulUserID);
    Params.Bind(stUser.acName, sizeof(stUser.acName));
    Params.Bind(&stUser.ulType);
    Params.Bind(&stUser.ulFuncType);
    Params.Bind(&stUser.ulStationID);

    iRow = m_pPDBApp->Query("SELECT UserID, Name, Type, FuncType, StationID FROM PortalUE WHERE SyncFlag>=0 ORDER BY UserID", Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query PortalUE failed");
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        vUser.push_back(stUser);
        memset(&stUser, 0, sizeof(stUser));
    }

    m_pPDBApp->CloseQuery();

    return PDB_ERROR != enRet;
}

BOOL CfgDao::AddPortalUE(PORTAL_UE_INFO_T &stInfo)
{
    return SetPortalUE(stInfo, SYNC_ADD);
}

BOOL CfgDao::DelPortalUE(UINT32 ulUserID)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;
    INT32           iSyncFlag;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&ulUserID);

    iAffectedRow = m_pPDBApp->Update("UPDATE PortalUE SET SyncFlag=? WHERE UserID=?", Params);

    return iAffectedRow >= 0;
}

BOOL CfgDao::SetPortalUE(PORTAL_UE_INFO_T &stInfo)
{
    return SetPortalUE(stInfo, SYNC_ADD);
}

BOOL CfgDao::SetPortalUE(PORTAL_UE_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulUserID);
    Params.BindString(stRec.acName);
    Params.Bind(&stRec.ulType);
    Params.Bind(&stRec.ulFuncType);
    Params.Bind(&stRec.ulStationID);

    Params.Bind(&iSyncFlag);
    Params.BindString(stRec.acName);
    Params.Bind(&stRec.ulType);
    Params.Bind(&stRec.ulFuncType);
    Params.Bind(&stRec.ulStationID);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO PortalUE(SyncFlag, UserID, Name, Type, FuncType, StationID) "
        "VALUES(?,?,?,?,?,?) ON DUPLICATE KEY UPDATE SyncFlag=?, Name=?, Type=?, FuncType=?, StationID=?", Params);

    return iAffectedRow > 0;
}

BOOL CfgDao::GetSyncingPortalUE(UINT32 ulMaxRecNum, VECTOR<PORTAL_UE_INFO_T> &vAddRec, VECTOR<PORTAL_UE_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    PORTAL_UE_INFO_T        stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.ulUserID);
    Params.Bind(stRec.acName, sizeof(stRec.acName));
    Params.Bind(&stRec.ulType);
    Params.Bind(&stRec.ulFuncType);
    Params.Bind(&stRec.ulStationID);

    sprintf(acSQL, "SELECT SyncFlag, UserID, Name, Type, FuncType, StationID FROM PortalUE WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query PortalUE failed");
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

BOOL CfgDao::SyncOverPortalUE(PORTAL_UE_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stRec.ulUserID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE PortalUE SET SyncFlag=0 WHERE UserID=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM PortalUE WHERE UserID=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

// brd zone
BOOL CfgDao::GetBrdZoneInfo(UINT32 ulStationID, VECTOR<BRD_ZONE_INFO_T> &vBrdZoneInfo)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    CHAR                    acSQL[1024];
    BRD_ZONE_INFO_T         stInfo;

    vBrdZoneInfo.clear();

    Params.Bind(&stInfo.i64GroupID);
    Params.Bind(&stInfo.ulStationID);
    Params.Bind(&stInfo.ulZoneID);
    Params.Bind(stInfo.acZoneName, sizeof(stInfo.acZoneName));

    sprintf(acSQL, "SELECT GroupID,StationID,ZoneID,ZoneName FROM BrdZoneInfo WHERE StationID=%u AND SyncFlag>=0", ulStationID);
    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        vBrdZoneInfo.push_back(stInfo);
    }

    m_pPDBApp->CloseQuery();

    return (PDB_ERROR != enRet);
}

BOOL CfgDao::GetBrdZoneInfo(VECTOR<BRD_ZONE_INFO_T> &vBrdZoneInfo)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    CHAR                    acSQL[1024];
    BRD_ZONE_INFO_T         stInfo;

    vBrdZoneInfo.clear();

    Params.Bind(&stInfo.i64GroupID);
    Params.Bind(&stInfo.ulStationID);
    Params.Bind(&stInfo.ulZoneID);
    Params.Bind(stInfo.acZoneName, sizeof(stInfo.acZoneName));

    sprintf(acSQL, "SELECT GroupID,StationID,ZoneID,ZoneName FROM BrdZoneInfo WHERE SyncFlag>=0 ORDER BY StationID,ZoneID");
    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        return FALSE;
    }

    FOR_ROW_FETCH(enRet, m_pPDBApp)
    {
        vBrdZoneInfo.push_back(stInfo);
    }

    m_pPDBApp->CloseQuery();

    return (PDB_ERROR != enRet);
}


BOOL CfgDao::AddBrdZoneInfo(BRD_ZONE_INFO_T &stInfo)
{
    return SetBrdZoneInfo(stInfo, SYNC_ADD);
}

BOOL CfgDao::DelBrdZoneInfo(UINT32 ulStationID, UINT32 ulZoneID)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;
    INT32           iSyncFlag;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&ulStationID);
    Params.Bind(&ulZoneID);

    iAffectedRow = m_pPDBApp->Update("UPDATE BrdZoneInfo SET SyncFlag=? WHERE StationID=? AND ZoneID=?", Params);

    return iAffectedRow >= 0;
}

BOOL CfgDao::SetBrdZoneInfo(BRD_ZONE_INFO_T &stInfo)
{
    return SetBrdZoneInfo(stInfo, SYNC_ADD);
}

BOOL CfgDao::SetBrdZoneInfo(BRD_ZONE_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    FormatSyncFlag(iSyncFlag);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.i64GroupID);
    Params.Bind(&stRec.ulStationID);
    Params.Bind(&stRec.ulZoneID);
    Params.BindString(stRec.acZoneName);

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.i64GroupID);
    Params.BindString(stRec.acZoneName);

    iAffectedRow = m_pPDBApp->Update("INSERT INTO BrdZoneInfo(SyncFlag, GroupID, StationID, ZoneID, ZoneName) "
        "VALUES(?,?,?,?,?) ON DUPLICATE KEY UPDATE SyncFlag=?, GroupID=?, ZoneName=?", Params);

    return iAffectedRow > 0;
}

BOOL CfgDao::GetSyncingBrdZoneInfo(UINT32 ulMaxRecNum, VECTOR<BRD_ZONE_INFO_T> &vAddRec, VECTOR<BRD_ZONE_INFO_T> &vDelRec)
{
    INT32                   iRow = 0;
    PDBRET                  enRet = PDB_CONTINUE;
    PDBRsltParam            Params;
    BRD_ZONE_INFO_T         stRec;
    INT32                   iSyncFlag;
    CHAR                    acSQL[1024];

    vAddRec.clear();
    vDelRec.clear();

    Params.Bind(&iSyncFlag);
    Params.Bind(&stRec.i64GroupID);
    Params.Bind(&stRec.ulStationID);
    Params.Bind(&stRec.ulZoneID);
    Params.Bind(stRec.acZoneName, sizeof(stRec.acZoneName));

    sprintf(acSQL, "SELECT SyncFlag, GroupID, StationID, ZoneID, ZoneName FROM BrdZoneInfo WHERE SyncFlag!=0");

    iRow = m_pPDBApp->Query(acSQL, Params);

    if (INVALID_ROWCOUNT == iRow)
    {
        Log(LOG_ERROR, "query BrdZoneInfo failed");
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

BOOL CfgDao::SyncOverBrdZoneInfo(BRD_ZONE_INFO_T &stRec, INT32 iSyncFlag)
{
    INT32           iAffectedRow = 0;
    PDBParam        Params;

    Params.Bind(&stRec.ulStationID);
    Params.Bind(&stRec.ulZoneID);

    if (iSyncFlag > 0)
    {
        iAffectedRow = m_pPDBApp->Update("UPDATE BrdZoneInfo SET SyncFlag=0 WHERE StationID=? AND ZoneID=?", Params);
        if (iAffectedRow <= 0)
        {
            return FALSE;
        }
    }
    else if (iSyncFlag < 0)
    {
        iAffectedRow = m_pPDBApp->Update("DELETE FROM BrdZoneInfo WHERE StationID=? AND ZoneID=?", Params);
        if (iAffectedRow < 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}
