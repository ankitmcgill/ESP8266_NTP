/****************************************************************
* ESP8266 NTP LIBRARY
*
* MAY 20 2017
*
* ANKIT BHATNAGAR
* ANKIT.BHATNAGARINDIA@GMAIL.COM
*
* REFERENCES
* ------------
*   (1) http://git.musl-libc.org/cgit/musl/plain/src/time/__secs_to_tm.c?h=v0.9.15
****************************************************************/

#ifndef _ESP8266_NTP_H_
#define _ESP8266_NTP_H_

#include "osapi.h"
#include "mem.h"
#include "ip_addr.h"
#include "ESP8266_UDP_CLIENT.h"

#define NTP_PORT				123
#define NTP_PACKET_SIZE			48
#define NTP_MAX_TRIES			5

#define LEAPOCH 				(36584UL) * 86400
#define DAYS_PER_4Y				(365*4 +1)
#define DAYS_PER_100Y			(365*100UL + 24)
#define DAYS_PER_400Y			(365*400UL + 97)

//CUSTOM VARIABLE STRUCTURES/////////////////////////////
typedef enum
{
	ESP8266_NTP_STATE_DNS_RESOLVED,
	ESP8266_NTP_STATE_ERROR,
	ESP8266_NTP_STATE_OK
} ESP8266_NTP_STATE;

typedef struct
{
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	uint8_t date;
	uint8_t day_num;
	const char* day_text;
	uint8_t month_num;
	const char* month_text;
	uint16_t year;
	uint32_t timestamp;
	ESP8266_NTP_STATE state;
} ESP8266_NTP_DATA;
//END CUSTOM VARIABLE STRUCTURES/////////////////////////

//FUNCTION PROTOTYPES/////////////////////////////////////
//CONFIGURATION FUNCTIONS
void ICACHE_FLASH_ATTR ESP8266_NTP_SetDebug(uint8_t debug_on);
void ICACHE_FLASH_ATTR ESP8266_NTP_Initialize(char* server1,
                                                	char* server2,
													char* server3,
													int8_t timezone_hr,
													uint8_t timezone_min,
													uint16_t ntp_timeout_ms);
void ICACHE_FLASH_ATTR ESP8266_NTP_SetCallbackFunctions(void (*user_data_ready_cb)(ESP8266_NTP_DATA*, uint16_t),
                                                            void (*user_alarm_cb)());

//GET PARAMETERS FUNCTIONS
int8_t ICACHE_FLASH_ATTR ESP8266_NTP_GetTimeZoneHour(void);
uint8_t ICACHE_FLASH_ATTR ESP8266_NTP_GetTimeZoneMinute(void);
uint8_t ICACHE_FLASH_ATTR ESP8266_NTP_GetLastNTPServerUsedNumber(void);
ESP8266_NTP_STATE ICACHE_FLASH_ATTR ESP8266_NTP_GetState(void);
ESP8266_NTP_DATA* ICACHE_FLASH_ATTR ESP8266_NTP_GetNTPDataStrcuture(void);

//CONTROL FUNCTIONS
void ICACHE_FLASH_ATTR ESP8266_NTP_GetTime(void);

//INITERNAL NTP TIME -> TEXT CONVERT FUNCTION
void ICACHE_FLASH_ATTR _esp8266_ntp_convert_time_to_text(void);

//INTERNAL CALLBACK FUNCTIONS
void ICACHE_FLASH_ATTR _esp8266_ntp_server_resolved_cb(ip_addr_t* ip);
void ICACHE_FLASH_ATTR _esp8266_ntp_udp_data_sent_cb(void* arg);
void ICACHE_FLASH_ATTR _esp8266_ntp_udp_data_recv_cb(char* pusrdata, uint16_t length);
//END FUNCTION PROTOTYPES/////////////////////////////////
#endif
