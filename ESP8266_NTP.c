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

#include "ESP8266_NTP.h"

//LOCAL LIBRARY VARIABLES////////////////////////////////
//DEBUG RELATED
static uint8_t _esp8266_ntp_debug;

//NTP TIME DATA STRUCTURE
static ESP8266_NTP_DATA* _esp8266_ntp_data;
static uint8_t* _esp8266_ntp_data_packet;

//IP / HOSTNAME RELATED
static char* _esp8266_ntp_server_1;
static char* _esp8266_ntp_server_2;
static char* _esp8266_ntp_server_3;
static uint8_t _esp8266_ntp_last_server_used;

//TIMEZONE RELATED
static int8_t _esp8266_ntp_timezone_hr;
static uint8_t _esp8266_ntp_timezone_min;

//TIMER RELATED
static uint16_t _esp8266_ntp_reply_timeout_ms;

//COUNTERS
static uint8_t _esp8266_ntp_retry_count;
static uint16_t _esp8266_ntp_cycle_count;
static uint8_t _esp8266_ntp_total_server_count;
static uint8_t _esp8266_ntp_server_counter;

//CALLBACK FUNCTION VARIABLES
static void (*_esp8266_ntp_data_ready_user_cb)(ESP8266_NTP_DATA*, uint16_t);
static void (*_esp8266_ntp_alarm_cb)(void);

//NTP TIME RELATED
static const char _esp8266_ntp_days_in_month[] = {
                                                    31, //MARCH
                                                    30, //APRIL
                                                    31, //MAY
                                                    30, //JUNE
                                                    31, //JULY
                                                    31, //AUGUST
                                                    30, //SEPTEMBER
                                                    31, //OCTOBER
                                                    30, //NOVEMBER
                                                    31, //DECEMBER
                                                    31, //JANUARY
                                                    29	//FEBRUARY
							                    };
							                    
static const char* _esp8266_ntp_month_names[12] =   {   
                                                        "January",
                                                        "February",
                                                        "March",
                                                        "April",
                                                        "May",
                                                        "June",
                                                        "July",
                                                        "August",
                                                        "September",
                                                        "October",
                                                        "November",
                                                        "December"
								                    };

static const uint8_t esp8266_ntp_month_names_length[12] = {
                                                            5,	//MARCH
                                                            5,	//APRIL
                                                            3,	//MAY
                                                            4,	//JUNE
                                                            4, //JULY
                                                            6,	//AUG
                                                            9,	//SEPT
                                                            7,	//OCT
                                                            8,	//NOV
                                                            8,	//DEC
                                                            7,	//JAN
                                                            8	//FEB
									                      };

static const char* _esp8266_ntp_day_names[7] =   {
                                                    "Sunday",
                                                    "Monday",
                                                    "Tuesday",
                                                    "Wednesday",
                                                    "Thursday",
                                                    "Friday",
                                                    "Saturday"
                                                 };

static const uint8_t esp8266_ntp_day_names_length[7] = {
                                                            6,  //SUNDAY
                                                            6,  //MONDAY
                                                            7,  //TUESDAY
                                                            9,  //WENESDAY
                                                            8,  //THURSDAY
                                                            6,  //FRIDAY
                                                            8   //SATURDAY
                                                       };

//END LOCAL LIBRARY VARIABLES/////////////////////////////

void ICACHE_FLASH_ATTR ESP8266_NTP_SetDebug(uint8_t debug_on)
{
    //SET DEBUG PRINTF ON(1) OR OFF(0)
    
    _esp8266_ntp_debug = debug_on;
}

void ICACHE_FLASH_ATTR ESP8266_NTP_Initialize(char* server1,
                                                	char* server2,
													char* server3,
													int8_t timezone_hr,
													uint8_t timezone_min,
													uint16_t ntp_timeout_ms)
{
    //SET THE NTP CONFIGURATION PARAMETERS
    
    _esp8266_ntp_server_1 = server1;
    _esp8266_ntp_server_2 = server2;
    _esp8266_ntp_server_3 = server3;
    
    _esp8266_ntp_timezone_hr = timezone_hr;
    _esp8266_ntp_timezone_min = timezone_min;
    
    _esp8266_ntp_reply_timeout_ms = ntp_timeout_ms;

    //CALCULATE TOTAL SERVER COUNT
    if(server1 == NULL)
    {
    	_esp8266_ntp_total_server_count = 0;
    	os_printf("ESP8266 : NTP : Error - must configure atleast 1 NTP server\n");
    }
    else if(server2 == NULL)
    {
    	//ASSUMES SERVER2 AND SERVER3 NULL
    	_esp8266_ntp_total_server_count = 1;
    }
    else if(server3 == NULL)
    {
    	_esp8266_ntp_total_server_count = 2;
    }
    else
    {
    	_esp8266_ntp_total_server_count = 3;
    }

    //ALLOCATE AND INITIALIZE NTP DATA STRUCTURE
    _esp8266_ntp_data = (ESP8266_NTP_DATA*)os_zalloc(sizeof(ESP8266_NTP_DATA));
    
    //ALLOCATE NTP DATA PACKET
    _esp8266_ntp_data_packet = (uint8_t*)os_zalloc(NTP_PACKET_SIZE);
    _esp8266_ntp_data_packet[0] = 0x1B;

    //SET INITIAL ESP8266 NTP STATUS
    _esp8266_ntp_data->state = ESP8266_NTP_STATE_OK;
    _esp8266_ntp_data->timestamp = 0;
    
    //INITIALIZE UDP PARAMETERS
    ESP8266_UDP_CLIENT_SetDebug(_esp8266_ntp_debug);
    
    ip_addr_t dns[2];
    dns[0].addr = ipaddr_addr("8.8.8.8");
    dns[0].addr = ipaddr_addr("8.8.4.4");
    ESP8266_UDP_CLIENT_SetDnsServer(2, dns);
    
    //SET UDP DATA CALLBACK FUNCTION
    ESP8266_UDP_CLIENT_SetCallbackFunctions(_esp8266_ntp_udp_data_sent_cb,
    											_esp8266_ntp_udp_data_recv_cb);
}

void ICACHE_FLASH_ATTR ESP8266_NTP_SetCallbackFunctions(void (*user_data_ready_cb)(ESP8266_NTP_DATA*, uint16_t),
                                                            void (*user_alarm_cb)())
{
    //SET THE LIBRARY CALLBACK FUNCTION POINTERS
    _esp8266_ntp_data_ready_user_cb = user_data_ready_cb;
    _esp8266_ntp_alarm_cb = user_alarm_cb;
}

int8_t ICACHE_FLASH_ATTR ESP8266_NTP_GetTimeZoneHour(void)
{
    //RETURN NTP TIMEZONE HOUR
    
    return _esp8266_ntp_timezone_hr;
}

uint8_t ICACHE_FLASH_ATTR ESP8266_NTP_GetTimeZoneMinute(void)
{
    //RETURN NTP TIMEZONE MINUTE
    
    return _esp8266_ntp_timezone_min;
}

uint8_t ICACHE_FLASH_ATTR ESP8266_NTP_GetLastNTPServerUsedNumber(void)
{
    //RETURN THE INDEX NUMBER OF THE NTP TIMESERVER USED
    //FOR THE LAST NTP TIME TRANSACTION (1, 2 OR 3))
    
    return _esp8266_ntp_last_server_used;
}

ESP8266_NTP_STATE ICACHE_FLASH_ATTR ESP8266_NTP_GetState(void)
{
    //RETURN ESP8266 NTP STATE VARIABLE
    
    return _esp8266_ntp_data->state;
}

ESP8266_NTP_DATA* ICACHE_FLASH_ATTR ESP8266_NTP_GetNTPDataStrcuture(void)
{
    //RETURN ESP8266 NTP DATA STRUCTURE
    
    return _esp8266_ntp_data;
}

void ICACHE_FLASH_ATTR ESP8266_NTP_GetTime(void)
{
    //SEND NTP PACKET TO NTP SERVER THROUGH UDP
    //SET UDP DATA RECEIVE CALLBACK FUNCTION
    //START NTP REPLY TIMER
    
    //SET THE NTP SERVER COUNTER
    _esp8266_ntp_server_counter = 1;
    _esp8266_ntp_retry_count = 0;
    
    if(_esp8266_ntp_debug)
    {
        os_printf("ESP8266 : NTP : Started DNS resolve NTP server %d %s\n", _esp8266_ntp_server_counter, _esp8266_ntp_server_1);
    }
    
    //SELECT NTP SERVER 1 AND START DNS RESOLVE
    ESP8266_UDP_CLIENT_Initialize(_esp8266_ntp_server_1, NULL, NTP_PORT, _esp8266_ntp_reply_timeout_ms);
    ESP8266_UDP_CLIENT_ResolveHostName(_esp8266_ntp_server_resolved_cb);
}

void ICACHE_FLASH_ATTR _esp8266_ntp_convert_time_to_text(void)
{
	//CONVERT NTP TIMESTAMP TO HUMAN READABLE TIME TEXT
	//AND SAVE IN GLOBAL TIME STRUCTURE

	//	YEAR	(BASE 2000)
	//	MONTH 	(JANUARY = 1)
	//	DATE 	(1 - 28 or 30/31 (MONTH DEPENDENT)
	//	WEEKDAY	(SUNDAY = 0)
	//	HOUR	(24 HOUR BASED)
	//	MINUTE
	//	SECOND

	//UTC OFFSET FOR USER SPECIFIED TIMEZONE
	int32_t timezone_offset_seconds = (_esp8266_ntp_timezone_hr * 3600) + (_esp8266_ntp_timezone_min * 60);
	uint32_t secs = _esp8266_ntp_data->timestamp;

	secs += timezone_offset_seconds;
	secs -= LEAPOCH;

	uint32_t days = (secs / 86400);
	uint32_t rem_secs = (secs % 86400);

	if(rem_secs < 0)
	{
		rem_secs += 86400;
		days--;
	}

	uint8_t wday = (days % 7) + 3;
	if(wday > 7)
	{
		wday -= 7;
	}

	uint32_t _400y_cycles = days / DAYS_PER_400Y;
	uint32_t rem_days = days % DAYS_PER_400Y;

	if(rem_days < 0)
	{
		rem_days += DAYS_PER_400Y;
		_400y_cycles--;
	}

	uint32_t _100y_cycles = rem_days / DAYS_PER_100Y;
	if(_100y_cycles == 4)
	{
		_100y_cycles--;
	}

	rem_days -= (_100y_cycles * DAYS_PER_100Y);

	uint32_t _4y_cycles = rem_days / DAYS_PER_4Y;
	if(_4y_cycles == 25)
	{
		_4y_cycles--;
	}

	rem_days -= (_4y_cycles * DAYS_PER_4Y);

	uint32_t rem_years = rem_days / 365;
	if(rem_years == 4)
	{
		rem_years --;
	}

	rem_days -= (rem_years * 365);

	uint16_t leap = !rem_years && (_4y_cycles || !_100y_cycles);
	uint16_t yday = rem_days + 31 + 28 + leap;

	if(yday > (365 + leap))
	{
		yday -= (365 + leap);
	}

	uint32_t years = rem_years + (4 * _4y_cycles) + (100 * _100y_cycles) + (400 * _400y_cycles);
	uint8_t months;

	for(months = 0; _esp8266_ntp_days_in_month[months] <= rem_days; months++)
	{
		rem_days -= _esp8266_ntp_days_in_month[months];
	}

	_esp8266_ntp_data->year = years;
	_esp8266_ntp_data->month_num = months + 3;
	if(_esp8266_ntp_data->month_num > 12)
	{
		_esp8266_ntp_data->month_num -= 12;
		_esp8266_ntp_data->year++;
	}

	//ADD 2000 TO YEAR SINCE OUT CALCULATED YEAR IS BASE 2000
	_esp8266_ntp_data->year += 2000;

	_esp8266_ntp_data->date = rem_days + 1;
	_esp8266_ntp_data->day_num = wday;
	_esp8266_ntp_data->hour = rem_secs / 3600;
	_esp8266_ntp_data->min = (rem_secs / 60) % 60;
	_esp8266_ntp_data->sec = rem_secs % 60;

	//SET THE DAY AND MONTH TEXT STRING POINTERS
	_esp8266_ntp_data->day_text = _esp8266_ntp_day_names[_esp8266_ntp_data->day_num];
	_esp8266_ntp_data->month_text = _esp8266_ntp_month_names[(_esp8266_ntp_data->month_num - 1)];
}

void ICACHE_FLASH_ATTR _esp8266_ntp_server_resolved_cb(ip_addr_t* ip)
{
    //ESP8266 NTP SERVER DNS RESOLVED CB
    
    //CHECK IF DNS RESOLUTION SUCCESSFULL
    if(ip != NULL)
    {
        //DNS RESOLUTION SUCCESSFULL
        if(_esp8266_ntp_debug)
        {
            os_printf("ESP8266 : NTP : Dns resolution OK\n");
        }
                
        //SEND UDP DATA
        ESP8266_UDP_CLIENT_SendData(_esp8266_ntp_data_packet, NTP_PACKET_SIZE);

    }
    else
    {
        //DNS RESOLUTION FAIL
        if(_esp8266_ntp_debug)
        {
            os_printf("ESP8266 : NTP : Dns resolution FAIL\n");
        }
        //CHANGE SERVER AND DO NEXT DNS RESOLUTION
        if(_esp8266_ntp_retry_count < NTP_MAX_TRIES)
        {
            _esp8266_ntp_server_counter++;
            if(_esp8266_ntp_server_counter > _esp8266_ntp_total_server_count)
            {
                _esp8266_ntp_server_counter = 1;
            }
            _esp8266_ntp_retry_count++;

            //START THE NEXT DNS RESOLUTION
            switch(_esp8266_ntp_server_counter)
            {
                case 1:
                        ESP8266_UDP_CLIENT_Initialize(_esp8266_ntp_server_1, NULL, NTP_PORT, _esp8266_ntp_reply_timeout_ms);
                        if(_esp8266_ntp_debug)
						{
							os_printf("ESP8266 : NTP : Started DNS resolve NTP server %d %s\n", _esp8266_ntp_server_counter, _esp8266_ntp_server_1);
						}
                        break;
                case 2:
                        ESP8266_UDP_CLIENT_Initialize(_esp8266_ntp_server_2, NULL, NTP_PORT, _esp8266_ntp_reply_timeout_ms);
                        if(_esp8266_ntp_debug)
						{
							os_printf("ESP8266 : NTP : Started DNS resolve NTP server %d %s\n", _esp8266_ntp_server_counter, _esp8266_ntp_server_2);
						}
                        break;
                case 3:
                        ESP8266_UDP_CLIENT_Initialize(_esp8266_ntp_server_3, NULL, NTP_PORT, _esp8266_ntp_reply_timeout_ms);
                        if(_esp8266_ntp_debug)
						{
							os_printf("ESP8266 : NTP : Started DNS resolve NTP server %d %s\n", _esp8266_ntp_server_counter, _esp8266_ntp_server_3);
						}
                        break;
            }
            ESP8266_UDP_CLIENT_ResolveHostName(_esp8266_ntp_server_resolved_cb);
        }
        else
        {
            //ALL NTP TRIES DONE. NTP FAIL. CALL USERCALLBACK WITH NTP_ERROR
            if(_esp8266_ntp_debug)
            {
                os_printf("ESP8266 : NTP : NTP retires finished. TERMINATED\n");
            }
            _esp8266_ntp_data->state = ESP8266_NTP_STATE_ERROR;
            
            //RESET COUNTERS
            _esp8266_ntp_retry_count = 0;
            _esp8266_ntp_server_counter = 1;

            //CALL USER CB IF NOT NULL WITH 0 ARGUMENT
            if(_esp8266_ntp_data_ready_user_cb != NULL)
            {
                _esp8266_ntp_data_ready_user_cb(_esp8266_ntp_data, 0);
            }
        }
    }
}

void ICACHE_FLASH_ATTR _esp8266_ntp_udp_data_sent_cb(void* arg)
{
	//NTP DATA SENT THROUGH THE UDP LIBRARY

	if(_esp8266_ntp_debug)
	{
		os_printf("ESP8266 : NTP : Data sent to NTP server index %d\n", _esp8266_ntp_server_counter);
	}
}

void ICACHE_FLASH_ATTR _esp8266_ntp_udp_data_recv_cb(char* pusrdata, uint16_t length)
{
    //NTP UDP DATA RECEIVED
    //DISABLE NTP TIMER
    //UPDATE TIMESTAMP IN NTP DATA STRUCTURE
    //DO NTP TIME CONVERSION TO THE SET TIMEZONE
    //CONVERT TIMESTAMP TO HUMAN READABLE DATETIME FORMAT
    //CALL USER NTP DATA READY CALLBACK FUNCTION
    
	//CHECK FOR THE VALIDITY OF DATA
	if(length == 0)
	{
		_esp8266_ntp_data->state = ESP8266_NTP_STATE_ERROR;
		if(_esp8266_ntp_debug)
		{
			os_printf("ESP8266 : NTP : Reply timeout\n");
		}

		//DO NTP CALL AGAIN WITH THE NEXT SERVER GIVEN RETRY COUNT
		//NO EXCEEDED

		 //CHANGE SERVER AND DO NEXT DNS RESOLUTION
		if(_esp8266_ntp_retry_count < NTP_MAX_TRIES)
		{
			_esp8266_ntp_server_counter++;
			if(_esp8266_ntp_server_counter > _esp8266_ntp_total_server_count)
			{
				_esp8266_ntp_server_counter = 1;
			}
			_esp8266_ntp_retry_count++;

			//START THE NEXT DNS RESOLUTION
			switch(_esp8266_ntp_server_counter)
			{
				case 1:
						ESP8266_UDP_CLIENT_Initialize(_esp8266_ntp_server_1, NULL, NTP_PORT, _esp8266_ntp_reply_timeout_ms);
						if(_esp8266_ntp_debug)
						{
							os_printf("ESP8266 : NTP : Started DNS resolve NTP server %d %s\n", _esp8266_ntp_server_counter, _esp8266_ntp_server_1);
						}
						break;
				case 2:
						ESP8266_UDP_CLIENT_Initialize(_esp8266_ntp_server_2, NULL, NTP_PORT, _esp8266_ntp_reply_timeout_ms);
						if(_esp8266_ntp_debug)
						{
							os_printf("ESP8266 : NTP : Started DNS resolve NTP server %d %s\n", _esp8266_ntp_server_counter, _esp8266_ntp_server_2);
						}
						break;
				case 3:
						ESP8266_UDP_CLIENT_Initialize(_esp8266_ntp_server_3, NULL, NTP_PORT, _esp8266_ntp_reply_timeout_ms);
						if(_esp8266_ntp_debug)
						{
							os_printf("ESP8266 : NTP : Started DNS resolve NTP server %d %s\n", _esp8266_ntp_server_counter, _esp8266_ntp_server_3);
						}
						break;
			}
			ESP8266_UDP_CLIENT_ResolveHostName(_esp8266_ntp_server_resolved_cb);
		}
		else
		{
			//ALL NTP TRIES DONE. NTP FAIL. CALL USERCALLBACK WITH NTP_ERROR
			if(_esp8266_ntp_debug)
			{
				os_printf("ESP8266 : NTP : NTP RETRIES finished. TERMINATED\n");
			}
			_esp8266_ntp_data->state = ESP8266_NTP_STATE_ERROR;

			//RESET COUNTERS
			_esp8266_ntp_retry_count = 0;
			_esp8266_ntp_server_counter = 1;

			 //CALL USER CB IF NOT NULL WITH EXTRACTED DATA IN STRUCTURE
			if(_esp8266_ntp_data_ready_user_cb != NULL)
			{
			   (_esp8266_ntp_data_ready_user_cb)(_esp8266_ntp_data, 0);
			}
		}
	}
	else
	{
		_esp8266_ntp_last_server_used = _esp8266_ntp_server_counter;
		_esp8266_ntp_cycle_count++;

		//RESET COUNTERS
		_esp8266_ntp_retry_count = 0;
		_esp8266_ntp_server_counter = 1;

		_esp8266_ntp_data->state = ESP8266_NTP_STATE_OK;

		//EXTRACT 32 BIT NTP TIMESTAMP FROM REPLY
		_esp8266_ntp_data->timestamp = pusrdata[40]; _esp8266_ntp_data->timestamp = (_esp8266_ntp_data->timestamp << 8);
		_esp8266_ntp_data->timestamp |= pusrdata[41]; _esp8266_ntp_data->timestamp = (_esp8266_ntp_data->timestamp << 8);
		_esp8266_ntp_data->timestamp |= pusrdata[42]; _esp8266_ntp_data->timestamp = (_esp8266_ntp_data->timestamp << 8);
		_esp8266_ntp_data->timestamp |= pusrdata[43];

		if(_esp8266_ntp_debug)
		{
			os_printf("ESP8266 : NTP : NTP data received of length %d\n", length);
			os_printf("ESP8266 : NTP : timestamp = %u\n", _esp8266_ntp_data->timestamp);
		}

		//CONVERT NTP TIME TO HUMAN READABLE
		_esp8266_ntp_convert_time_to_text();

		 //CALL USER CB IF NOT NULL WITH EXTRACTED DATA IN STRUCTURE
		if(_esp8266_ntp_data_ready_user_cb != NULL)
		{
		   (_esp8266_ntp_data_ready_user_cb)(_esp8266_ntp_data, length);
		}
	}
}
