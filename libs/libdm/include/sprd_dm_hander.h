#ifndef __SPRDDM_HANDER_H__
#define __SPRDDM_HANDER_H__



typedef struct
{
    uint8                       inPath[70];
    int32 handletype;
} DM_TREE_ITEM_T;

typedef enum
{
    DM_NONE_SET,
    DM_APN_SET,
    DM_MON_SET,
    DM_DEV_VERSION_SET,
    DM_MAN_SET,
    DM_IMEI_SET,
    DM_SRV_ADDR,
    DM_SELF_REG_NUM,
    DM_SELF_REGPORT_NUM,
    DM_CNNECT_SERVER,
} MMIDM_DEBUG_TYPE_E;


typedef enum{

  DEVID_IO_HANDLER=0,
  FROMFILE_IO_HANDLER=1,
  MODEL_IO_HANDLER=2,
  MAN_IO_HANDLER=3,
  OEM_IO_HANDLER=4,

  LANG_IO_HANDLER=5,
  DMVERSION_IO_HANDLER=6,

  FWVERSION_IO_HANDLER=7,
  SWVERSION_IO_HANDLER=8,
  HWVERSION_IO_HANDLER=9,
  SERVER_ADDR_IO_HANDLER=10,

//DM Setting
  DM_CONN_PROFILE_IO_HANDLER=11,
  DM_APN_IO_HANDLER=12,
  DM_PROXY_IO_HANDLER=13,
  DM_PORT_IO_HANDLER=14,

//GPRS-CMNET Setting
  GPRS_CMNET_APN_IO_HANDLER=15,
  GPRS_CMNET_PROXY_IO_HANDLER=16,
  GPRS_CMNET_PORT_IO_HANDLER=17,
  GPRS_CMNET_USERNAME_IO_HANDLER=18,
  GPRS_CMNET_PASSWORD_IO_HANDLER=19,

//GPRS-CMWAP Setting
  GPRS_CMWAP_APN_IO_HANDLER=20,
  GPRS_CMWAP_PROXY_IO_HANDLER=21,
  GPRS_CMWAP_PORT_IO_HANDLER=22,
  GPRS_CMWAP_USERNAME_IO_HANDLER=23,
  GPRS_CMWAP_PASSWORD_IO_HANDLER=24,

//WAP Setting
  WAP_CONNPROFILE_IO_HANDLER=25,
  WAP_HOMEPAGE_IO_HANDLER=26,
  WAP_PROXY_IO_HANDLER=27,
  WAP_PORT_IO_HANDLER=28,
  WAP_USERNAME_IO_HANDLER=29,
  WAP_PASSWORD_IO_HANDLER=30,

//MMS Setting
  MMS_CONNPROFILE_IO_HANDLER=31,
  MMS_MMSC_IO_HANDLER=32,


//PIM Setting
  PIM_CONNPROFILE_URI_IO_HANDLER=33,
  PIM_SERVER_ADDR_IO_HANDLER=34,
  PIM_ADDRESS_BOOK_URI_IO_HANDLER=35,
  PIM_CALENDAR_URI_IO_HANDLER=36,

//PushMail Setting
  MAIL_CONNPROFILE_IO_HANDER=37,
  MAIL_SEND_SERVER_IO_HANDER=38,
  MAIL_SEND_PORT_IO_HANDER=39,
  MAIL_SEND_USE_SEC_CON_IO_HANDER=40,
  MAIL_RECV_SERVER_IO_HANDER=41,
  MAIL_RECV_PORT_IO_HANDER=42,
  MAIL_RECV_USE_SEC_CON_IO_HANDER=43,
  MAIL_RECV_PROTOCAL_IO_HANDER=44,


//Streaming Setting
  STREAMING_CONNPROFILE_IO_HANDLER=45,
  STREAMING_NAME_IO_HANDLER=46,
  STREAMING_MAX_UDP_PORT_IO_HANDLER=47,
  STREAMING_MIN_UDP_PORT_IO_HANDLER=48,
  STREAMING_NET_INFO_IO_HANDLER=49,


//AGPS Setting
  AGPS_CONNPROFILE_IO_HANDLER=50,
  AGPS_SERVER_IO_HANDLER=51,
  AGPS_SERVER_NAME_IO_HANDLER=52,
  AGPS_IAPID_IO_HANDLER=53,
  AGPS_PORT_IO_HANDLER=54,
  AGPS_PROVIDER_ID_IO_HANDLER=55,

  DM_WAP_CONN_PROFILE_IO_HANDLER=56,
  DM_WAP_APN_IO_HANDLER=57,
  DM_WAP_PROXY_IO_HANDLER=58,
  DM_WAP_PORT_IO_HANDLER=59,

  DM_SAVE_NONCE = 60,
  BROWSER_HOMEPAGE_IO_HANDLER = 61,
//bug 292626 begin
  AGPS_PREFCONREF_ID_IO_HANDLER = 62,
  AGPS_CONREF_IO_HANDLER = 63,
//bug 292626 end
}DM_TREE_HANDER_T;

#endif

