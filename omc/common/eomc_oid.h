#ifndef EOMC_OID_H
#define EOMC_OID_H


// huawei eOMC
static SNMP_OID oid_eOMC_HeartBeat              [] = {1,3,6,1,4,1,42400,2,15,2,1,2,1,1,1,0,5};
//ZTE   eOMC 1.3.6.1.4.1.3902.6005.16.1.1.1.1
static SNMP_OID oid_ZTE_eOMC_HeartBeat          [] = {1,3,6,1,4,1,3902,6005,16,1,1,1,1};

static SNMP_OID oid_eOMC_HeartbeatSystemLabel   [] = {1,3,6,1,4,1,42400,2,15,2,1,2,1,1,1,1};     // OCTET STRING eOMC910系统标识，填写SNMPAgent。
static SNMP_OID oid_eOMC_HeartbeatPeriod        [] = {1,3,6,1,4,1,42400,2,15,2,1,2,1,1,1,2};     // INTEGER     心跳周期，单位为毫秒，固定为60000。
static SNMP_OID oid_eOMC_HeartbeatTimeStamp     [] = {1,3,6,1,4,1,42400,2,15,2,1,2,1,1,1,3};     // OCTET STRING 时间戳，产生心跳通知Trap的时间。采用UTC时间。

// 告警
static SNMP_OID oid_eOMC_AlarmReport            [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,0,1};


static SNMP_OID oid_eOMC_AlarmCSN               [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,1};    // OCTET STRING 实时告警上报通知。
static SNMP_OID oid_eOMC_AlarmCategory          [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,2};    // OCTET STRING 告警种类，

#define EOMC_ALARM_TYPE_RAISE       1   // 故障
#define EOMC_ALARM_TYPE_CLEAR       2   // 恢复
#define EOMC_ALARM_TYPE_EVENT       3   // 事件
#define EOMC_ALARM_TYPE_CONFIRM     4   // 确认
#define EOMC_ALARM_TYPE_UNCONFIRM   5   // 反确认
#define EOMC_ALARM_TYPE_UPDATE      9   // 变更

static SNMP_OID oid_eOMC_AlarmOccurTime             [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,3};    //  OCTET STRING 告警发生时间。
static SNMP_OID oid_eOMC_AlarmMOName                [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,4};    //  OCTET STRING 设备名称(填写网元名称)。
static SNMP_OID oid_eOMC_AlarmProductID             [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,5};    //  INTEGER 产品系列标识(填写-1)。
static SNMP_OID oid_eOMC_AlarmNEType                [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,6};    //  OCTET STRING 设备类型标识(填写网元类型)。
static SNMP_OID oid_eOMC_AlarmNEDevID               [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,7};    //  OCTET STRING 设备唯一标识(填写网元名称)。
static SNMP_OID oid_eOMC_AlarmDevCsn                [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,8};    //  OCTET STRING 告警的设备流水号，同一种网元内部唯一。
static SNMP_OID oid_eOMC_AlarmID                    [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,9};    //  INTEGER 告警ID，用来区分同一种类型设备的告警类型。
static SNMP_OID oid_eOMC_AlarmType                  [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,10};   //  INTEGER 告警类型，
/*
1：电源系统
2：环境系统
3：信令系统
4：中继系统
5：硬件系统
6：软件系统
7：运行系统
8：通讯系统
9：业务质量
10：处理出错
11：网管内部
12：完整性违背
13：操作违背
14：物理违背
15：安全服务或机制违背
16：时间域违背*/

static SNMP_OID oid_eOMC_AlarmLevel                 [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,11};   // INTEGER 告警级别，
#define EOMC_ALARM_LEVEL_FATAL      1   // 1：紧急
#define EOMC_ALARM_LEVEL_MAIN       2   // 2：重要
#define EOMC_ALARM_LEVEL_MINOR      3   // 3：次要
#define EOMC_ALARM_LEVEL_NORMAL     4   // 4：提示
#define EOMC_ALARM_LEVEL_CLEAR      6   // 6：恢复
#define EOMC_ALARM_LEVEL_EVENT      0   // 0: 事件

static SNMP_OID oid_eOMC_AlarmRestore               [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,12};   // INTEGER 告警恢复标志，
#define EOMC_ALARM_CLEAR            1   // 已恢复
#define EOMC_ALARM_NOT_CLEAR        2   // 未恢复
#define EOMC_ALARM_EVENT            0   // 事件

static SNMP_OID oid_eOMC_AlarmConfirm               [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,13};   // INTEGER 告警确认标志，
#define EOMC_ALARM_CONFIRM          1   // 已确认
#define EOMC_ALARM_NOT_CONFIRM      2   // 未确认
//0: 事件

static SNMP_OID oid_eOMC_AlarmAckTime               [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,14};   // OCTET STRING 告警确认时间。
static SNMP_OID oid_eOMC_AlarmRestoreTime           [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,15};   // OCTET STRING 告警恢复时间。
static SNMP_OID oid_eOMC_AlarmOperator              [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,16};   // OCTET STRING 进行确认操作的操作员。
static SNMP_OID oid_eOMC_AlarmExtendInfo            [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,27};   // OCTET STRING 告警扩展信息，主要包含了告警的定位信息。
static SNMP_OID oid_eOMC_AlarmProbablecause         [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,28};   // OCTET STRING 告警标题。
static SNMP_OID oid_eOMC_AlarmProposedrepairactions [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,29};   // OCTET STRING 告警修复建议。
static SNMP_OID oid_eOMC_AlarmSpecificproblems      [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,30};   // OCTET STRING 告警详细原因。
static SNMP_OID oid_eOMC_AlarmClearOperator         [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,46};   // OCTET STRING 清除告警的操作员。
static SNMP_OID oid_eOMC_AlarmObjectInstanceType    [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,47};   // OCTET STRING 告警对象实例类型。
static SNMP_OID oid_eOMC_AlarmClearCategory         [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,48};   // OCTET STRING 告警清除类别。
/* 1：可自动清除的告警
2：不能自动清除的告警
0: 事件 */
static SNMP_OID oid_eOMC_AlarmClearType             [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,49};   // OCTET STRING 告警清除类型。
/* 1：正常清除
2：复位清除
3：手动清除
4：配置清除
5：相关性清除
0：当告警种类不为清除告警时 */
static SNMP_OID oid_eOMC_AlarmServiceAffectFlag     [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,50};   // OCTET STRING 影响服务的标记。    0：否   1：是
static SNMP_OID oid_eOMC_AlarmAdditionalInfo        [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,51};   // OCTET STRING 附加信息。
static SNMP_OID oid_eOMC_AlarmNEIP                  [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,52};   // OCTET STRING 触发告警的网元IP

/*
上报的告警Trap的字符集编码格式为UTF-8。
告警时间VB字段的时间格式为UTC时间格式。
警流水号Csn的上报类型为字符串。
*/





#endif
