/* Automatically generated nanopb constant definitions */
/* Generated by nanopb-0.2.0 at Mon Dec 09 14:00:14 2013. */

#include "cfg_mgr_dynamic.pb.h"



const pb_field_t ConfigDynamic_fields[3] = {
    PB_FIELD(  1, MESSAGE , OPTIONAL, STATIC, ConfigDynamic, userInfo, userInfo, &UserInfo_fields),
    PB_FIELD(  2, MESSAGE , OPTIONAL, STATIC, ConfigDynamic, sysInfo, userInfo, &SystemInfo_fields),
    PB_LAST_FIELD
};

const pb_field_t UserInfo_fields[6] = {
    PB_FIELD(  1, STRING  , OPTIONAL, CALLBACK, UserInfo, dogName, dogName, 0),
    PB_FIELD(  2, MESSAGE , OPTIONAL, STATIC, UserInfo, userNetwork, dogName, &UserNetworks_fields),
    PB_FIELD(  3, MESSAGE , OPTIONAL, STATIC, UserInfo, btCfg, userNetwork, &BluetoothCfg_fields),
    PB_FIELD(  4, MESSAGE , OPTIONAL, STATIC, UserInfo, activityCfg, btCfg, &ActivityCfg_fields),
    PB_FIELD(  5, MESSAGE , OPTIONAL, STATIC, UserInfo, timeCfg, activityCfg, &TimeCfg_fields),
    PB_LAST_FIELD
};

const pb_field_t ActivityCfg_fields[2] = {
    PB_FIELD(  1, UINT32  , OPTIONAL, STATIC, ActivityCfg, daily_minutes_active_goal, daily_minutes_active_goal, 0),
    PB_LAST_FIELD
};

const pb_field_t TimeCfg_fields[2] = {
    PB_FIELD(  1, INT64   , OPTIONAL, STATIC, TimeCfg, timezone_offset, timezone_offset, 0),
    PB_LAST_FIELD
};

const pb_field_t UserNetworks_fields[3] = {
    PB_FIELD( 20, MESSAGE , REPEATED, CALLBACK, UserNetworks, network, network, &Network_fields),
    PB_FIELD( 21, STRING  , OPTIONAL, CALLBACK, UserNetworks, mru_ssid, network, 0),
    PB_LAST_FIELD
};

const pb_field_t Network_fields[3] = {
    PB_FIELD(  1, STRING  , OPTIONAL, CALLBACK, Network, ssid, ssid, 0),
    PB_FIELD(  2, MESSAGE , OPTIONAL, STATIC, Network, security, ssid, &NetworkSecurity_fields),
    PB_LAST_FIELD
};

const pb_field_t NetworkSecurity_fields[4] = {
    PB_FIELD(  1, UINT32  , OPTIONAL, STATIC, NetworkSecurity, securityType, securityType, 0),
    PB_FIELD(  2, MESSAGE , OPTIONAL, STATIC, NetworkSecurity, wepParams, securityType, &SecurityWEP_fields),
    PB_FIELD(  3, MESSAGE , OPTIONAL, STATIC, NetworkSecurity, wpaParams, wepParams, &SecurityWPA_fields),
    PB_LAST_FIELD
};

const pb_field_t SecurityWEP_fields[3] = {
    PB_FIELD(  1, STRING  , OPTIONAL, CALLBACK, SecurityWEP, key, key, 0),
    PB_FIELD(  2, UINT32  , OPTIONAL, STATIC, SecurityWEP, default_key_index, key, 0),
    PB_LAST_FIELD
};

const pb_field_t SecurityWPA_fields[5] = {
    PB_FIELD(  1, STRING  , OPTIONAL, CALLBACK, SecurityWPA, version, version, 0),
    PB_FIELD(  2, STRING  , OPTIONAL, CALLBACK, SecurityWPA, passphrase, version, 0),
    PB_FIELD(  3, UINT32  , OPTIONAL, STATIC, SecurityWPA, ucipher, passphrase, 0),
    PB_FIELD(  4, UINT32  , OPTIONAL, STATIC, SecurityWPA, mcipher, ucipher, 0),
    PB_LAST_FIELD
};

const pb_field_t BluetoothCfg_fields[4] = {
    PB_FIELD(  1, MESSAGE , REPEATED, CALLBACK, BluetoothCfg, LinkKeyInfo, LinkKeyInfo, &LinkKeyInfo_fields),
    PB_FIELD(  2, STRING  , OPTIONAL, CALLBACK, BluetoothCfg, BtDeviceName, LinkKeyInfo, 0),
    PB_FIELD(  3, UINT32  , OPTIONAL, STATIC, BluetoothCfg, mru_index, BtDeviceName, 0),
    PB_LAST_FIELD
};

const pb_field_t LinkKeyInfo_fields[5] = {
    PB_FIELD(  1, BYTES   , OPTIONAL, STATIC, LinkKeyInfo, Key, Key, 0),
    PB_FIELD(  2, BYTES   , OPTIONAL, STATIC, LinkKeyInfo, Addr, Key, 0),
    PB_FIELD(  3, UINT32  , OPTIONAL, STATIC, LinkKeyInfo, usage_count, Addr, 0),
    PB_FIELD(  4, ENUM    , OPTIONAL, STATIC, LinkKeyInfo, pom, usage_count, 0),
    PB_LAST_FIELD
};

const pb_field_t SystemInfo_fields[4] = {
    PB_FIELD(  1, UINT32  , OPTIONAL, STATIC, SystemInfo, lastFWUAttempt, lastFWUAttempt, 0),
    PB_FIELD(  2, UINT32  , OPTIONAL, STATIC, SystemInfo, installFlag, lastFWUAttempt, 0),
    PB_FIELD(  3, UINT32  , OPTIONAL, STATIC, SystemInfo, deviceHasCheckedIn, installFlag, 0),
    PB_LAST_FIELD
};


/* Check that field information fits in pb_field_t */
#if !defined(PB_FIELD_16BIT) && !defined(PB_FIELD_32BIT)
STATIC_ASSERT((pb_membersize(ConfigDynamic, userInfo) < 256 && pb_membersize(ConfigDynamic, sysInfo) < 256 && pb_membersize(UserInfo, userNetwork) < 256 && pb_membersize(UserInfo, btCfg) < 256 && pb_membersize(UserInfo, activityCfg) < 256 && pb_membersize(UserInfo, timeCfg) < 256 && pb_membersize(UserNetworks, network) < 256 && pb_membersize(Network, security) < 256 && pb_membersize(NetworkSecurity, wepParams) < 256 && pb_membersize(NetworkSecurity, wpaParams) < 256 && pb_membersize(BluetoothCfg, LinkKeyInfo) < 256), YOU_MUST_DEFINE_PB_FIELD_16BIT_FOR_MESSAGES_ConfigDynamic_UserInfo_ActivityCfg_TimeCfg_UserNetworks_Network_NetworkSecurity_SecurityWEP_SecurityWPA_BluetoothCfg_LinkKeyInfo_SystemInfo)
#endif

#if !defined(PB_FIELD_32BIT)
STATIC_ASSERT((pb_membersize(ConfigDynamic, userInfo) < 65536 && pb_membersize(ConfigDynamic, sysInfo) < 65536 && pb_membersize(UserInfo, userNetwork) < 65536 && pb_membersize(UserInfo, btCfg) < 65536 && pb_membersize(UserInfo, activityCfg) < 65536 && pb_membersize(UserInfo, timeCfg) < 65536 && pb_membersize(UserNetworks, network) < 65536 && pb_membersize(Network, security) < 65536 && pb_membersize(NetworkSecurity, wepParams) < 65536 && pb_membersize(NetworkSecurity, wpaParams) < 65536 && pb_membersize(BluetoothCfg, LinkKeyInfo) < 65536), YOU_MUST_DEFINE_PB_FIELD_32BIT_FOR_MESSAGES_ConfigDynamic_UserInfo_ActivityCfg_TimeCfg_UserNetworks_Network_NetworkSecurity_SecurityWEP_SecurityWPA_BluetoothCfg_LinkKeyInfo_SystemInfo)
#endif
