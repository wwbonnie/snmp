#ifndef SNMP_API_H
#define SNMP_API_H

#ifdef __cplusplus
extern "C" {
#endif

BOOL snmp_get_scalar_field(SNMP_SESSION_T *pstSnmpSession, SNMP_COLUMN_T *pstColumn, VOID* pucColumnData);
BOOL snmp_set_scalar_field(SNMP_SESSION_T *pstSnmpSession, SNMP_COLUMN_T *pstColumn, VOID* pucColumnData);
BOOL snmp_get_table_field(SNMP_SESSION_T *pstSnmpSession, SNMP_COLUMN_T *pstColumn, UINT32 ulKey, VOID* pucColumnData);
BOOL snmp_set_table_field(SNMP_SESSION_T *pstSnmpSession, SNMP_COLUMN_T *pstColumn, UINT32 ulKey, VOID* pucColumnData);
BOOL SnmpGetScalar(SNMP_SESSION_T *pstSnmpSession, UINT32 ulTableID, VOID *pData);
BOOL SnmpSetScalar(SNMP_SESSION_T *pstSnmpSession, UINT32 ulTableID, VOID *pData);

#ifdef __cplusplus
}
#endif

#endif
