#ifndef EOMC_OID_H
#define EOMC_OID_H


// huawei eOMC
static SNMP_OID oid_eOMC_HeartBeat              [] = {1,3,6,1,4,1,42400,2,15,2,1,2,1,1,1,0,5};
//ZTE   eOMC 1.3.6.1.4.1.3902.6005.16.1.1.1.1
static SNMP_OID oid_ZTE_eOMC_HeartBeat          [] = {1,3,6,1,4,1,3902,6005,16,1,1,1,1};

static SNMP_OID oid_eOMC_HeartbeatSystemLabel   [] = {1,3,6,1,4,1,42400,2,15,2,1,2,1,1,1,1};     // OCTET STRING eOMC910ϵͳ��ʶ����дSNMPAgent��
static SNMP_OID oid_eOMC_HeartbeatPeriod        [] = {1,3,6,1,4,1,42400,2,15,2,1,2,1,1,1,2};     // INTEGER     �������ڣ���λΪ���룬�̶�Ϊ60000��
static SNMP_OID oid_eOMC_HeartbeatTimeStamp     [] = {1,3,6,1,4,1,42400,2,15,2,1,2,1,1,1,3};     // OCTET STRING ʱ�������������֪ͨTrap��ʱ�䡣����UTCʱ�䡣

// �澯
static SNMP_OID oid_eOMC_AlarmReport            [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,0,1};


static SNMP_OID oid_eOMC_AlarmCSN               [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,1};    // OCTET STRING ʵʱ�澯�ϱ�֪ͨ��
static SNMP_OID oid_eOMC_AlarmCategory          [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,2};    // OCTET STRING �澯���࣬

#define EOMC_ALARM_TYPE_RAISE       1   // ����
#define EOMC_ALARM_TYPE_CLEAR       2   // �ָ�
#define EOMC_ALARM_TYPE_EVENT       3   // �¼�
#define EOMC_ALARM_TYPE_CONFIRM     4   // ȷ��
#define EOMC_ALARM_TYPE_UNCONFIRM   5   // ��ȷ��
#define EOMC_ALARM_TYPE_UPDATE      9   // ���

static SNMP_OID oid_eOMC_AlarmOccurTime             [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,3};    //  OCTET STRING �澯����ʱ�䡣
static SNMP_OID oid_eOMC_AlarmMOName                [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,4};    //  OCTET STRING �豸����(��д��Ԫ����)��
static SNMP_OID oid_eOMC_AlarmProductID             [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,5};    //  INTEGER ��Ʒϵ�б�ʶ(��д-1)��
static SNMP_OID oid_eOMC_AlarmNEType                [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,6};    //  OCTET STRING �豸���ͱ�ʶ(��д��Ԫ����)��
static SNMP_OID oid_eOMC_AlarmNEDevID               [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,7};    //  OCTET STRING �豸Ψһ��ʶ(��д��Ԫ����)��
static SNMP_OID oid_eOMC_AlarmDevCsn                [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,8};    //  OCTET STRING �澯���豸��ˮ�ţ�ͬһ����Ԫ�ڲ�Ψһ��
static SNMP_OID oid_eOMC_AlarmID                    [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,9};    //  INTEGER �澯ID����������ͬһ�������豸�ĸ澯���͡�
static SNMP_OID oid_eOMC_AlarmType                  [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,10};   //  INTEGER �澯���ͣ�
/*
1����Դϵͳ
2������ϵͳ
3������ϵͳ
4���м�ϵͳ
5��Ӳ��ϵͳ
6�����ϵͳ
7������ϵͳ
8��ͨѶϵͳ
9��ҵ������
10���������
11�������ڲ�
12��������Υ��
13������Υ��
14������Υ��
15����ȫ��������Υ��
16��ʱ����Υ��*/

static SNMP_OID oid_eOMC_AlarmLevel                 [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,11};   // INTEGER �澯����
#define EOMC_ALARM_LEVEL_FATAL      1   // 1������
#define EOMC_ALARM_LEVEL_MAIN       2   // 2����Ҫ
#define EOMC_ALARM_LEVEL_MINOR      3   // 3����Ҫ
#define EOMC_ALARM_LEVEL_NORMAL     4   // 4����ʾ
#define EOMC_ALARM_LEVEL_CLEAR      6   // 6���ָ�
#define EOMC_ALARM_LEVEL_EVENT      0   // 0: �¼�

static SNMP_OID oid_eOMC_AlarmRestore               [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,12};   // INTEGER �澯�ָ���־��
#define EOMC_ALARM_CLEAR            1   // �ѻָ�
#define EOMC_ALARM_NOT_CLEAR        2   // δ�ָ�
#define EOMC_ALARM_EVENT            0   // �¼�

static SNMP_OID oid_eOMC_AlarmConfirm               [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,13};   // INTEGER �澯ȷ�ϱ�־��
#define EOMC_ALARM_CONFIRM          1   // ��ȷ��
#define EOMC_ALARM_NOT_CONFIRM      2   // δȷ��
//0: �¼�

static SNMP_OID oid_eOMC_AlarmAckTime               [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,14};   // OCTET STRING �澯ȷ��ʱ�䡣
static SNMP_OID oid_eOMC_AlarmRestoreTime           [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,15};   // OCTET STRING �澯�ָ�ʱ�䡣
static SNMP_OID oid_eOMC_AlarmOperator              [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,16};   // OCTET STRING ����ȷ�ϲ����Ĳ���Ա��
static SNMP_OID oid_eOMC_AlarmExtendInfo            [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,27};   // OCTET STRING �澯��չ��Ϣ����Ҫ�����˸澯�Ķ�λ��Ϣ��
static SNMP_OID oid_eOMC_AlarmProbablecause         [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,28};   // OCTET STRING �澯���⡣
static SNMP_OID oid_eOMC_AlarmProposedrepairactions [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,29};   // OCTET STRING �澯�޸����顣
static SNMP_OID oid_eOMC_AlarmSpecificproblems      [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,30};   // OCTET STRING �澯��ϸԭ��
static SNMP_OID oid_eOMC_AlarmClearOperator         [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,46};   // OCTET STRING ����澯�Ĳ���Ա��
static SNMP_OID oid_eOMC_AlarmObjectInstanceType    [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,47};   // OCTET STRING �澯����ʵ�����͡�
static SNMP_OID oid_eOMC_AlarmClearCategory         [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,48};   // OCTET STRING �澯������
/* 1�����Զ�����ĸ澯
2�������Զ�����ĸ澯
0: �¼� */
static SNMP_OID oid_eOMC_AlarmClearType             [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,49};   // OCTET STRING �澯������͡�
/* 1���������
2����λ���
3���ֶ����
4���������
5����������
0�����澯���಻Ϊ����澯ʱ */
static SNMP_OID oid_eOMC_AlarmServiceAffectFlag     [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,50};   // OCTET STRING Ӱ�����ı�ǡ�    0����   1����
static SNMP_OID oid_eOMC_AlarmAdditionalInfo        [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,51};   // OCTET STRING ������Ϣ��
static SNMP_OID oid_eOMC_AlarmNEIP                  [] = {1,3,6,1,4,1,42400,2,15,2,4,3,3,52};   // OCTET STRING �����澯����ԪIP

/*
�ϱ��ĸ澯Trap���ַ��������ʽΪUTF-8��
�澯ʱ��VB�ֶε�ʱ���ʽΪUTCʱ���ʽ��
����ˮ��Csn���ϱ�����Ϊ�ַ�����
*/





#endif
