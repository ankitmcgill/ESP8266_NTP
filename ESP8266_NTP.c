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
static uint8_t _esp8266_ntp_data_packet[NTP_PACKET_SIZE] = {0}; packet[0] = 0x0B;

//IP / HOSTNAME RELATED
static char* _esp8266_ntp_server_1;
static char* _esp8266_ntp_server_2;
static char* _esp8266_ntp_server_3;
static uint8_t _esp8266_last_ntp_server_used;

//TIMEZONE RELATED
static int8_t _esp8266_ntp_timezone_hr;
static uint8_t _esp8266_ntp_timezone_min;

//TIMER RELATED
static os_timer_t _esp8266_ntp_reply_timer
static uint8_t _esp8266_ntp_server_counter;

//COUNTERS
static uint8_t _esp8266_ntp_retry_count;
static uint16_t _esp8266_ntp_cycle_count;

//CALLBACK FUNCTION VARIABLES
static void (*_esp8266_ntp_data_ready_user_cb)(ESP8266_NTP_DATA*, char*);
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
                                                        "March",
                                                        "April",
                                                        "May",
                                                        "June",
                                                        "July",
                                                        "August",
                                                        "September",
                                                        "October",
                                                        "November",
                                                        "December",
                                                        "January",
                                                        "February"
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
}
//END LOCAL LIBRARY VARIABLES/////////////////////////////

void ICACHE_FLASH_ATTR ESP8266_NTP_SetDebug(uint8_t debug_on)
{
    //SET DEBUG PRINTF ON(1) OR OFF(0)
    
    _esp8266_ntp_debug = debug_on;
}

void ICACHE_FLASH_ATTR ESP8266_NTP_Initialize(const char* server1,
                                                const char* server2,
                                                const char* server3,
                                                int8_t timezone_hr,
                                                uint8_t timezone_min))
{
    //SET THE NTP CONFIGURATION PARAMETERS
    
    _esp8266_ntp_server_1 = server1;
    _esp8266_ntp_server_2 = server2;
    _esp8266_ntp_server_3 = server3;
    
    _esp8266_ntp_timezone_hr = timezone_hr;
    _esp8366_ntp_timezone_min = timezone_min;
    
    //ALLOCATE AND INITIALIZE NTP DATA STRUCTURE
    _esp8266_ntp_data = (ESP8266_NTP_DATA*)os_zalloc(sizeof(ESP8266_NTP_DATA));
    
    //SET INITIAL ESP8266 NTP STATUS
    _esp8266_ntp_data->state = ESP8266_NTP_STATE_OK;
    _esp8266_ntp_data->timestamp = 0;
    
    //INITIALIZE UDP PARAMETERS
    ESP8266_UDP_CLIENT_SetDebug(0);
    
    struct ip_addr_t dns[2];
    dns[0].addr = ipaddr_addr("8.8.8.8");
    dns[0].addr = ipaddr_addr("8.8.4.4");
    ESP8266_UDP_CLIENT_SetDnsServer(2, &dns);
    
    //SET UDP DATA CALLBACK FUNCTION
    ESP8266_UDP_CLIENT_SetCallbackFunctions(_esp8266_ntp_udp_data_recv_cb);
    
    //SET DEBUG ON
    _esp8266_ntp_debug = 1;
}

void ICACHE_FLASH_ATTR ESP8266_NTP_SetCallbackFunctions(void (*user_data_ready_cb)(ESP8266_NTP_DATA*, char*),
                                                            void (*user_alarm_cb(void)))
{
    //SET THE LIBRARY CALLBACK FUNCTION POINTERS
    _esp8266_ntp_data_ready_user_cb = user_data_ready_cb;
    _esp8266_ntp_alarm_cb = user_alarm_cb
}

int8_t ICACHE_FLASH_ATTR ESP8266_NTP_GetTimeZoneHour(void)
{
    //RETURN NTP TIMEZONE HOUR
    
    return _esp8266_ntp_timezone_hr;
}

uint8_t ICACHE_FLASH_ATTR ESP8266_NTP_GetTimeZoneMinute(void)
{
    //RETURN NTP TIMEZONE MINUTE
    
    return _esp8366_ntp_timezone_min;
}

uint8_t ICACHE_FLASH_ATTR ESP8266_NTP_GetLastNTPServerUsedNumber(void)
{
    //RETURN THE INDEX NUMBER OF THE NTP TIMESERVER USED
    //FOR THE LAST NTP TIME TRANSACTION (1, 2 OR 3))
    
    return _esp8266_last_ntp_server_used;
}

ESP8266_NTP_STATE ICACHE_FLASH_ATTR ESP8266_NTP_GetState(void)
{
    //RETURN ESP8266 NTP STATE VARIABLE
    
    return _esp8266_ntp_data->STATE;
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
    _esp8266_ntp_server_counter = 0;
    _esp8266_ntp_retry_count = 0;
    
    if(_esp8266_ntp_debug)
    {
        os_printf("ESP8266 : NTP : Started DNS resolve NTP server %d %s\n", _esp8266_ntp_server_counter, _esp8266_ntp_server_1);
    }
    
    //SELECT NTP SERVER 1 AND START DNS RESOLVE
    ESP8266_UDP_CLIENT_Initialize(_esp8266_ntp_server_1, NULL, NTP_PORT);
    ESP8266_UDP_CLIENT_ResolveHostName(_esp8266_ntp_server_resolved_cb);
}

void ICACHE_FLASH_ATTR _esp8266_ntp_reply_timer_cb(void* arg)
{
    //NTP TIMER CALLBACK FUNCTION
    //TIME PERIOD = 1 SECOND
    //IF THIS TIMER IS CALLED => NTP REPLY NOT RECEIVED FROM SERVER
    //INITIATE ANOTHER NTP REQUEST WITH THE NEXT NTP SERVER
    //CALL USER NTP DATA READY CB FUNCTION WILL NULL ARGUMENT IF NTP RETRY LIMIT EXCEEDED
    
    
}

void ICACHE_FLASH_ATTR _esp8266_ntp_server_resolved_cb(ip_addt_t* ip)
{
    //ESP8266 NTP SERVER DNS RESOLVED CB
    
    //CHECK IF DNS RESOLUTION SUCCESSFULL
    if(ip != NULL)
    {
        //DNS RESOLUTION SUCCESSFULL
        if(_esp8266_ntp_debug)
        {
            os_printf("ESP8266 : NTP : Dns resolution OK\n");
            os_printf("ESP8266 : NTP : Sending NTP request OK\n");
        }
                
        //SEND UDP DATA
        ESP8266_UDP_CLIENT_SendData(_esp8266_ntp_data_packet, NTP_PACKET_SIZE);
        
        //SET UP 1 SECOND NTP TIMER AND ARM IT
        os_timer_setfn(&_esp8266_ntp_reply_timer, (os_timer_func_t*)_esp8266_ntp_reply_timer_cb, NULL);
	    os_timer_arm(&_esp8266_ntp_reply_timer, NTP_REPLY_TIMEOUT_MS, 1);
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
            if(_esp8266_ntp_server_counter == 3)
            {
                _esp8266_ntp_server_counter = 0;
            }
            //START THE NEXT DNS RESOLUTION
            if(_esp8266_ntp_debug)
            {
                os_printf("ESP8266 : NTP : Started DNS resolve NTP server %d %s\n", _esp8266_ntp_server_counter, _esp8266_ntp_server_1);
            }
            switch(_esp8266_ntp_server_counter)
            {
                case 0:
                        ESP8266_UDP_CLIENT_Initialize(_esp8266_ntp_server_1, NULL, NTP_PORT);
                        break;
                case 1:
                        ESP8266_UDP_CLIENT_Initialize(_esp8266_ntp_server_2, NULL, NTP_PORT);
                        break;
                case 2:
                        ESP8266_UDP_CLIENT_Initialize(_esp8266_ntp_server_3, NULL, NTP_PORT);
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
            os_timer_disarm(&_esp8266_ntp_reply_timer);
            _esp8266_ntp_data->state = ESP8266_NTP_STATE_ERROR;
            
            //CALL USER CB IF NOT NULL
            if(_esp8266_ntp_data_ready_user_cb != NULL)
            {
                _esp8266_ntp_data_ready_user_cb(_esp8266_ntp_data, NULL);
            }
        }
    }
    
}

void ICACHE_FLASH_ATTR _esp8266_ntp_udp_data_recv_cb(void* arg, char* pusrdata, unsigned short length)
{
    //NTP UDP DATA RECEIVED
    //DISABLE NTP TIMER
    //UPDATE TIMESTAMP IN NTP DATA STRUCTURE
    //DO NTP TIME CONVERSION TO THE SET TIMEZONE
    //CONVERT TIMESTAMP TO HUMAN READABLE DATETIME FORMAT
    //CALL USER NTP DATA READY CALLBACK FUNCTION
    
    os_timer_disarm(&_esp8266_ntp_reply_timer);
    
    _esp8266_ntp_data->state = ESP8266_NTP_STATE_OK;
    
    //EXTRACT 32 BIT NTP TIMESTAMP FROM REPLY
	uint32_t a = pusrdata[40];
	uint32_t b = pusrdata[41];
	uint32_t c = pusrdata[42];
	uint32_t d = pusrdata[43];

	_esp8266_ntp_data->timestamp = (a << 24) | (b << 16) | (c << 8) | d;

    if(_esp8266_ntp_debug)
    {
        os_printf("ESP8266 : NTP : NTP data received of length %d\n", length);
        os_printf("ESP8266 : NTP : timestamp = %d\n", _esp8266_ntp_data->timestamp);
    }
	
	
    //CALL USER CB IF NOT NULL
    if(_esp8266_ntp_data_ready_user_cb != NULL)
    {
       (_esp8266_ntp_data_ready_user_cb)(_esp8266_ntp_data);
    }
}
