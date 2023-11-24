#include "snmp_nms.h"
#include "snmp_def.h"
#include "snmp_table.h"

/*
[system]
agent_ip=192.168.1.30
agent_port=161
trap_port=162
community=private
timeout=5
*/


BOOL SnmpGetSystem(SNMP_SESSION_T *pstSnmpSession, SNMP_TABLE_SYSTEM_T *pstData)
{
    return SnmpGetScalar(pstSnmpSession, SNMP_TABLE_SYSTEM, pstData);
}

BOOL SnmpSetSystem(SNMP_SESSION_T *pstSnmpSession, SNMP_TABLE_SYSTEM_T *pstData)
{
    return SnmpSetScalar(pstSnmpSession, SNMP_TABLE_SYSTEM, pstData);
}

int main()
{
    UINT8           aucAgengIP[4];
    UINT16          usAgentPort = 161;
    CHAR            acCommunity[32];
    UINT32          ulTimeout = 2;
    SNMP_SESSION_T      *pstSession = NULL;

    aucAgengIP[0] = 127;
    aucAgengIP[1] = 0;
    aucAgengIP[2] = 0;
    aucAgengIP[3] = 1;

    sprintf(acCommunity, "public");
    pstSession = snmp_init_session(aucAgengIP, usAgentPort, acCommunity, ulTimeout);

    return 0;
}
