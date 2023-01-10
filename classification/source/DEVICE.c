/*------------------------------------------------------------------
 *  Fred Juhlin (2023)
 *------------------------------------------------------------------*/
 
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <axsdk/axparameter.h>

#include "cJSON.h"
#include "HTTP.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

cJSON *DEVICE_Container = 0;
AXParameter *DEVICE_parameter_handler;

char**
string_split( char* a_str,  char a_delim) {
    char** result    = 0;
    size_t count     = 0;
    const char* tmp  = a_str;
    const char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;
    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;
    result = malloc(sizeof(char*) * count);
    if (result) {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);
        while (token) {
//            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
//        assert(idx == count - 1);
        *(result + idx) = 0;
    }
    return result;
}

cJSON*
DEVICE() {
	return DEVICE_Container;
}

char* 
DEVICE_Prop( const char *attribute ) {
	if( !DEVICE_Container )
		return 0;
	if( strcmp(attribute,"IPv4") == 0 ) {
		gchar *value = 0;
		if(ax_parameter_get(DEVICE_parameter_handler, "root.Network.eth0.IPAddress",&value,0 )) {
			cJSON_ReplaceItemInObject(DEVICE_Container,"IPv4",cJSON_CreateString(value));
			g_free(value);
		}
	}  
	return cJSON_GetObjectItem(DEVICE_Container,attribute) ? cJSON_GetObjectItem(DEVICE_Container,attribute)->valuestring : 0;
}

int
DEVICE_Prop_Int( const char *attribute ) {
  if( !DEVICE_Container )
    return 0;
  return cJSON_GetObjectItem(DEVICE_Container,attribute) ? cJSON_GetObjectItem(DEVICE_Container,attribute)->valueint : 0;
}

cJSON* 
DEVICE_JSON( const char *attribute ) {
  if( !DEVICE_Container )
    return 0;
  return cJSON_GetObjectItem(DEVICE_Container,attribute);
}


int
DEVICE_Seconds_Since_Midnight() {
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	int seconds = tm.tm_hour * 3600;
	seconds += tm.tm_min * 60;
	seconds += tm.tm_sec;
	return seconds;
}

char DEVICE_date[128] = "2023-01-01";
char DEVICE_time[128] = "00:00:00";

const char*
DEVICE_Date() {
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	sprintf(DEVICE_date,"%d-%02d-%02d",tm->tm_year + 1900,tm->tm_mon + 1, tm->tm_mday);
	return DEVICE_date;
}

const char*
DEVICE_Time() {
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	sprintf(DEVICE_time,"%02d:%02d:%02d",tm->tm_hour,tm->tm_min,tm->tm_sec);
	return DEVICE_time;
}

char DEVICE_timestring[128] = "2020-01-01 00:00:00";

const char*
DEVICE_Local_Time() {
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	sprintf(DEVICE_timestring,"%d-%02d-%02d %02d:%02d:%02d",tm->tm_year + 1900,tm->tm_mon + 1, tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec);
	LOG_TRACE("Local Time: %s\n",DEVICE_timestring);
	return DEVICE_timestring;
}

char DEVICE_isostring[128] = "2020-01-01T00:00:00+0000";

const char*
DEVICE_ISOTime() {
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	strftime(DEVICE_isostring, 50, "%Y-%m-%dT%T%z",tm);
	return DEVICE_isostring;
}

double
DEVICE_Timestamp(void) {
	long ms;
	time_t s;
	struct timespec spec;
	double timestamp;
	clock_gettime(CLOCK_REALTIME, &spec);
	s  = spec.tv_sec;
	ms = round(spec.tv_nsec / 1.0e6); // Convert nanoseconds to milliseconds
	if (ms > 999) {
		s++;
		ms -= 999;
	}
	timestamp = (double)s;
	timestamp *= 1000;
	timestamp += (double)ms;
	return timestamp;  
}

static void
DEVICE_HTPP_Request(HTTP_Response response,  HTTP_Request request) {
	if( !DEVICE_Container) {
		HTTP_Respond_Error( response, 500, "Device properties not initialized");
		return;
	}
	cJSON_ReplaceItemInObject(DEVICE_Container,"date", cJSON_CreateString(DEVICE_Date()) );
	cJSON_ReplaceItemInObject(DEVICE_Container,"time", cJSON_CreateString(DEVICE_Time()) );
	HTTP_Respond_JSON( response, DEVICE_Container );
}

double previousTransmitted = 0;
double previousNetworkTimestamp = 0;
double previousNetworkAverage = 0;

double
DEVICE_Network_Average() {
	FILE *fd;
	char   data[500] = "";
	char   readstr[500];
	char **stringArray;
	char   *subString;
	char *ptr;
	double transmitted = 0,rx = 0;
	 

	fd = fopen("/proc/net/dev","r");
	if( !fd ) {
		LOG_WARN("Error opening /proc/net/dev");
		return 0;
	}
  
	while( fgets(readstr,500,fd) ) {
		if( strstr( readstr,"eth0") != 0 )
			strcpy( data, readstr );
	}
	fclose(fd);
  
	if( strlen( data ) < 20 ) {
		LOG_WARN("Read_Ethernet_Traffic: read error");
		return 0;
	}
  
	stringArray = string_split(data, ' ');
	if( stringArray ) {
		int i;
		for (i = 0; *(stringArray + i); i++) {
			if( i == 1 ) {
			subString = *(stringArray + i);
				rx = (double)strtol( subString, &ptr, 10);
			}
			if( i == 9 ) {
				subString = *(stringArray + i);
				if(strlen(subString) > 9 )
					subString++;
				if(strlen(subString) > 9 )
					subString++;
				if(strlen(subString) > 9 )
					subString++;
				transmitted = (double)strtol( subString, &ptr, 10);
			}
			free(*(stringArray + i));
		}
		free(stringArray);
	}
	double diff = transmitted - previousTransmitted;
	previousTransmitted = transmitted;
	if( diff < 0 )
		return previousNetworkAverage;
	double timestamp = DEVICE_Timestamp();
	double timeDiff = timestamp - previousNetworkTimestamp;
	previousNetworkTimestamp = timestamp;
	timeDiff /= 1000;
	diff *= 8;  //Make to bits;
	diff /= 1024;  //Make Kbits;
	
	previousNetworkAverage = diff / timeDiff;
	if( previousNetworkAverage < 0.001 )
		previousNetworkAverage = 0;
	return previousNetworkAverage;
}

double
DEVICE_CPU_Average() {
	double loadavg = 0;
	struct sysinfo info;
	
	sysinfo(&info);
	loadavg = (double)info.loads[2];
	loadavg /= 65536.0;
	LOG_TRACE("%f\n",loadavg);
	return loadavg; 
}

double
DEVICE_Uptime() {
	struct sysinfo info;
	sysinfo(&info);
	return (double)info.uptime; 
};	


void 
DEVICE_Init( const char *packageid ) {
	gchar *value = 0, *pHead,*pTail;

	LOG_TRACE("DEVICE_Init: %s", packageid );

	DEVICE_Container = cJSON_CreateObject();
	DEVICE_parameter_handler = ax_parameter_new(packageid, 0);
	if( !DEVICE_parameter_handler ) {
		LOG_WARN("Cannot create parameter DEVICE_parameter_handler");
		return;
	}
	

	if(ax_parameter_get(DEVICE_parameter_handler, "root.Properties.System.SerialNumber", &value, 0)) {
		cJSON_AddItemToObject(DEVICE_Container,"serial",cJSON_CreateString(value));
		g_free(value);
	}
  
	if(ax_parameter_get(DEVICE_parameter_handler, "root.brand.ProdShortName", &value, 0)) {
		cJSON_AddItemToObject(DEVICE_Container,"model",cJSON_CreateString(value));
		g_free(value);
	}

	if(ax_parameter_get(DEVICE_parameter_handler, "root.Properties.System.Architecture", &value, 0)) {
		cJSON_AddItemToObject(DEVICE_Container,"platform",cJSON_CreateString(value));
		g_free(value);
	}

	if(ax_parameter_get(DEVICE_parameter_handler, "root.Properties.System.Soc", &value, 0)) {
		cJSON_AddItemToObject(DEVICE_Container,"chip",cJSON_CreateString(value));
		g_free(value);
	}

	if(ax_parameter_get(DEVICE_parameter_handler, "root.brand.ProdType", &value, 0)) {
		cJSON_AddItemToObject(DEVICE_Container,"type",cJSON_CreateString(value));
		g_free(value);
	}

	if(ax_parameter_get(DEVICE_parameter_handler, "root.Network.VolatileHostName.HostName", &value, 0)) {
		cJSON_AddItemToObject(DEVICE_Container,"hostname",cJSON_CreateString(value));
		g_free(value);
	}
  
	cJSON* resolutionList = cJSON_CreateArray();
	cJSON* resolutions = cJSON_CreateObject();
	cJSON_AddItemToObject(DEVICE_Container,"resolutions",resolutions);
	cJSON* resolutions169 = cJSON_CreateArray();
	cJSON_AddItemToObject(resolutions,"16:9",resolutions169);
	cJSON* resolutions43 = cJSON_CreateArray();
	cJSON_AddItemToObject(resolutions,"4:3",resolutions43);
	cJSON* resolutions11 = cJSON_CreateArray();
	cJSON_AddItemToObject(resolutions,"1:1",resolutions11);
	cJSON* resolutions1610 = cJSON_CreateArray();
	cJSON_AddItemToObject(resolutions,"16:10",resolutions1610);
	if(ax_parameter_get(DEVICE_parameter_handler, "root.Properties.Image.Resolution", &value, 0)) {
		pHead = value;
		pTail = value;
		while( *pHead ) {
			if( *pHead == ',' ) {
				*pHead = 0;
				cJSON_AddItemToArray( resolutionList, cJSON_CreateString(pTail) );
				pTail = pHead + 1;
			}
			pHead++;
		}
		cJSON_AddItemToArray( resolutionList, cJSON_CreateString(pTail) );
		g_free(value);
	}
	int length = cJSON_GetArraySize(resolutionList);
	int index;
	char data[30];
	LOG_TRACE("Resolutions");
	for( index = 0; index < length; index++ ) {
		char* resolution = strcpy(data,cJSON_GetArrayItem(resolutionList,index)->valuestring);
		if( resolution ) {
			char* sX = resolution;
			char* sY = 0;
			while( *sX != 0 ) {
				if( *sX == 'x' ) {
					*sX = 0;
					sY = sX + 1;
				}
				sX++;
			}
			if( sY ) {
				int x = atoi(resolution);
				int y = atoi(sY);
				if( x && y ) {
					int a = (x*100)/y;
					if( a == 177 )
						cJSON_AddItemToArray(resolutions169, cJSON_CreateString(cJSON_GetArrayItem(resolutionList,index)->valuestring));
					if( a == 133 )
						cJSON_AddItemToArray(resolutions43, cJSON_CreateString(cJSON_GetArrayItem(resolutionList,index)->valuestring));
					if( a == 160 )
						cJSON_AddItemToArray(resolutions1610, cJSON_CreateString(cJSON_GetArrayItem(resolutionList,index)->valuestring));
					if( a == 100 )
						cJSON_AddItemToArray(resolutions11, cJSON_CreateString(cJSON_GetArrayItem(resolutionList,index)->valuestring));
				}
			}
		}
	}
	cJSON_Delete(resolutionList);
	LOG_TRACE("Resolutions: Done");
  
	if(ax_parameter_get(DEVICE_parameter_handler, "root.Network.eth0.MACAddress",&value,0 )) {
		cJSON_AddItemToObject(DEVICE_Container,"mac",cJSON_CreateString(value));
		g_free(value);
	}  

	if(ax_parameter_get(DEVICE_parameter_handler, "root.ImageSource.I0.Sensor.AspectRatio",&value,0 )) {
		cJSON_AddItemToObject(DEVICE_Container,"aspect",cJSON_CreateString(value));
		g_free(value);
	}  

	if(ax_parameter_get(DEVICE_parameter_handler, "root.Image.I0.Appearance.Rotation",&value,0 )) {
		cJSON_AddItemToObject(DEVICE_Container,"rotation",cJSON_CreateNumber( atoi(value) ));
		g_free(value);
	}  

	if(ax_parameter_get(DEVICE_parameter_handler, "root.Network.eth0.IPAddress",&value,0 )) {
		cJSON_AddItemToObject(DEVICE_Container,"IPv4",cJSON_CreateString(value));
		g_free(value);
	}  
  
	if(ax_parameter_get(DEVICE_parameter_handler, "root.Properties.Firmware.Version",&value,0 )) {
		cJSON_AddItemToObject(DEVICE_Container,"firmware",cJSON_CreateString(value));
		g_free(value);
	}  

	cJSON_AddStringToObject(DEVICE_Container,"date", DEVICE_Date() );
	cJSON_AddStringToObject(DEVICE_Container,"time", DEVICE_Time() );
 
	ax_parameter_free(DEVICE_parameter_handler);
	
	HTTP_Node("device",DEVICE_HTPP_Request);
}

