/*------------------------------------------------------------------
 *  Fred Juhlin (2023)
 *------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <axsdk/axevent.h>
#include "EVENTS.h"
#include "HTTP.h"
#include "DEVICE.h"
#include "cJSON.h"
#include "FILE.h"
#include "APP.h"


#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

EVENTS_Callback EVENT_USER_CALLBACK = 0;
static void EVENTS_Main_Callback(guint subscription, AXEvent *axEvent, cJSON* event);
cJSON* EVENTS_SUBSCRIPTIONS = 0;
cJSON* EVENTS_DECLARATIONS = 0;
AXEventHandler *EVENTS_HANDLER = 0;


double ioFilterTimestamp0 = 0;
double ioFilterTimestamp1 = 0;
double ioFilterTimestamp2 = 0;
double ioFilterTimestamp3 = 0;

char EVENTS_PACKAGE[64];
char EVENTS_APPNAME[64];


cJSON* SubscriptionsArray = 0;
typedef struct S_AXEventKeyValueSet {
	GHashTable *key_values;
} T_ValueSet;

typedef struct S_NamespaceKeyPair {
  gchar *name_space;
  gchar *key;
} T_KeyPair;

typedef struct S_ValueElement {
  gint int_value;
  gboolean bool_value;
  gdouble double_value;
  gchar *str_value;
  AXEventElementItem *elem_value;
  gchar *elem_str_value;
  GList *tags;
  gchar *key_nice_name;
  gchar *value_nice_name;

  gboolean defined;
  gboolean onvif_data;

  AXEventValueType value_type;
} T_ValueElement;


int
EVENTS_SetCallback( EVENTS_Callback callback ){
	LOG_TRACE("%s: Entry\n",__func__);
	EVENT_USER_CALLBACK = callback;
}

cJSON*
EVENTS_Parse( AXEvent *axEvent ) {
	LOG_TRACE("%s: Entry\n",__func__);
	
	const T_ValueSet *set = (T_ValueSet *)ax_event_get_key_value_set(axEvent);
	
	cJSON *object = cJSON_CreateObject();
	GHashTableIter iter;
	T_KeyPair *nskp;
	T_ValueElement *value_element;
	char topics[6][32];
	char topic[200];
	int i;
	for(i=0;i<6;i++)
		topics[i][0]=0;
	
	g_hash_table_iter_init(&iter, set->key_values);
//	LOG_TRACE("EVENTS_ParsePayload:\n");
	while (g_hash_table_iter_next(&iter, (gpointer*)&nskp,(gpointer*)&value_element)) {
		int isTopic = 0;
		
		if( strcmp(nskp->key,"topic0") == 0 ) {
//			LOG_TRACE("Parse: Topic 0: %s\n",(char*)value_element->str_value);
			if( strcmp((char*)value_element->str_value,"CameraApplicationPlatform") == 0 )
				sprintf(topics[0],"acap");
			else
				sprintf(topics[0],"%s",(char*)value_element->str_value);
			isTopic = 1;
		}
		if( strcmp(nskp->key,"topic1") == 0 ) {
//			LOG_TRACE("Parse: Topic 1: %s\n",(char*)value_element->str_value);
			sprintf(topics[1],"%s",value_element->str_value);
			isTopic = 1;
		}
		if( strcmp(nskp->key,"topic2") == 0 ) {
//			LOG_TRACE("Parse: Topic 2: %s\n",(char*)value_element->str_value);
			sprintf(topics[2],"%s",value_element->str_value);
			isTopic = 1;
		}
		if( strcmp(nskp->key,"topic3") == 0 ) {
//			LOG_TRACE("Parse: Topic 3: %s\n",(char*)value_element->str_value);
			sprintf(topics[3],"%s",value_element->str_value);
			isTopic = 1;
		}
		if( strcmp(nskp->key,"topic4") == 0 ) {
			sprintf(topics[4],"%s",value_element->str_value);
			isTopic = 1;
		}
		if( strcmp(nskp->key,"topic5") == 0 ) {
			sprintf(topics[5],"%s",value_element->str_value);
			isTopic = 1;
		}
		
		if( isTopic == 0 ) {
			if (value_element->defined) {
				switch (value_element->value_type) {
					case AX_VALUE_TYPE_INT:
//						LOG_TRACE("Parse: Int %s = %d\n", nskp->key, value_element->int_value);
						cJSON_AddNumberToObject(object,nskp->key,(double)value_element->int_value);
					break;

					case AX_VALUE_TYPE_BOOL:
						LOG_TRACE("Bool %s = %d\n", nskp->key, value_element->bool_value);
//						if( strcmp(nskp->key,"state") != 0 )
//							cJSON_AddNumberToObject(object,nskp->key,(double)value_element->bool_value);
//						if( !value_element->bool_value )
//							cJSON_ReplaceItemInObject(object,"state",cJSON_CreateFalse());
						cJSON_AddBoolToObject(object,nskp->key,value_element->bool_value);
					break;

					case AX_VALUE_TYPE_DOUBLE:
						LOG_TRACE("Parse: Double %s = %f\n", nskp->key, value_element->double_value);
						cJSON_AddNumberToObject(object,nskp->key,(double)value_element->double_value);
					break;

					case AX_VALUE_TYPE_STRING:
						LOG_TRACE("Parse: String %s = %s\n", nskp->key, value_element->str_value);
						cJSON_AddStringToObject(object,nskp->key, value_element->str_value);
					break;

					case AX_VALUE_TYPE_ELEMENT:
						LOG_TRACE("Parse: Element %s = %s\n", nskp->key, value_element->str_value);
						cJSON_AddStringToObject(object,nskp->key, value_element->elem_str_value);
					break;

					default:
						LOG_TRACE("Parse: Undefined %s = %s\n", nskp->key, value_element->str_value);
						cJSON_AddNullToObject(object,nskp->key);
					break;
				}
			}
		}
	}
	strcpy(topic,topics[0]);
	for(i=1;i<6;i++) {
		if( strlen(topics[i]) > 0 ) {
			strcat(topic,"/");
			strcat(topic,topics[i]);
		}
	}

	//Special Device Event Filter
	if( strcmp(topic,"Device/IO/Port") == 0 ) {
		int port = cJSON_GetObjectItem(object,"port")?cJSON_GetObjectItem(object,"port")->valueint:-1;
		if( port == -1 ) {
			cJSON_Delete(object);
			return 0;
		}
		if( port == 0 ) {
			if( DEVICE_Timestamp() - ioFilterTimestamp0 < 1000 ) {
				cJSON_Delete(object);
				return 0;
			}
			ioFilterTimestamp0 = DEVICE_Timestamp();
		}
		
		if( port == 1 ) {
			if( DEVICE_Timestamp() - ioFilterTimestamp1 < 1000 ) {
				cJSON_Delete(object);
				return 0;
			}
			ioFilterTimestamp1 = DEVICE_Timestamp();
		}
		
		if( port == 2 ) {
			if( DEVICE_Timestamp() - ioFilterTimestamp2 < 1000 ) {
				cJSON_Delete(object);
				return 0;
			}
			ioFilterTimestamp2 = DEVICE_Timestamp();
		}

		if( port == 3 ) {
			if( DEVICE_Timestamp() - ioFilterTimestamp3 < 1000 ) {
				cJSON_Delete(object);
				return 0;
			}
			ioFilterTimestamp3 = DEVICE_Timestamp();
		}
	}

	cJSON_AddStringToObject(object,"event",topic);
	return object;
}

static void
EVENTS_Main_Callback(guint subscription, AXEvent *axEvent, cJSON* event) {
	LOG_TRACE("%s: Entry\n",__func__);

	cJSON* eventData = EVENTS_Parse(axEvent);
	if( !eventData )
		return;
	if( EVENT_USER_CALLBACK )
		EVENT_USER_CALLBACK( eventData );
}

int
EVENTS_Subscribe( cJSON *event ) {
	AXEventKeyValueSet *keyset = 0;	
	cJSON *topic;
	char *ns,*tag;
	guint declarationID = 0;
	int result;
	
	if( !event ) {
		LOG_WARN("EVENTS_Subscribe: Invalid event\n")
		return 0;
	}

	char* json = cJSON_PrintUnformatted(event);
	if( json ) {
		LOG_TRACE("%s: %s\n",__func__,json);
		free(json);
	}


	if( !cJSON_GetObjectItem( event,"topic0" ) ) {
		LOG_WARN("EVENTS_Subscribe: Producer event has no topic0\n");
		return 0;
	}
	
	keyset = ax_event_key_value_set_new();
	if( !keyset ) {	LOG_WARN("EVENTS_Subscribe: Unable to create keyset\n"); return 0;}


	// ----- TOPIC 0 ------
	topic = cJSON_GetObjectItem( event,"topic0" );
	if(!topic) {
		LOG_WARN("EVENTS_Subscribe: Invalid tag for topic 0");
		return 0;
	}
	result = ax_event_key_value_set_add_key_value( keyset, "topic0", topic->child->string, topic->child->valuestring, AX_VALUE_TYPE_STRING,NULL);
	if( !result ) {
		LOG_WARN("EVENTS_Subscribe: Topic 0 keyset error");
		ax_event_key_value_set_free(keyset);
		return 0;
	}
	LOG_TRACE("%s: TOPIC0 %s:%s\n",__func__, topic->child->string, topic->child->valuestring );
	
	// ----- TOPIC 1 ------
	if( cJSON_GetObjectItem( event,"topic1" ) ) {
		topic = cJSON_GetObjectItem( event,"topic1" );
		result = ax_event_key_value_set_add_key_value( keyset, "topic1", topic->child->string, topic->child->valuestring, AX_VALUE_TYPE_STRING,NULL);
		if( !result ) {
			LOG_WARN("EVENTS_Subscribe: Unable to subscribe to event (1)");
			ax_event_key_value_set_free(keyset);
			return 0;
		}
		LOG_TRACE("%s: TOPIC1 %s:%s\n",__func__, topic->child->string, topic->child->valuestring );
	}
	//------ TOPIC 2 -------------
	if( cJSON_GetObjectItem( event,"topic2" ) ) {
		topic = cJSON_GetObjectItem( event,"topic2" );
		result = ax_event_key_value_set_add_key_value( keyset, "topic2", topic->child->string, topic->child->valuestring, AX_VALUE_TYPE_STRING,NULL);
		if( !result ) {
			LOG_WARN("EVENTS_Subscribe: Unable to subscribe to event (2)");
			ax_event_key_value_set_free(keyset);
			return 0;
		}
		LOG_TRACE("%s: TOPIC2 %s:%s\n",__func__, topic->child->string, topic->child->valuestring );
	}
	
	if( cJSON_GetObjectItem( event,"topic3" ) ) {
		topic = cJSON_GetObjectItem( event,"topic3" );
		result = ax_event_key_value_set_add_key_value( keyset, "topic3", topic->child->string, topic->child->valuestring, AX_VALUE_TYPE_STRING,NULL);
		if( !result ) {
			LOG_WARN("EVENTS_Subscribe: Unable to subscribe to event (3)");
			ax_event_key_value_set_free(keyset);
			return 0;
		}
		LOG_TRACE("%s: TOPIC3 %s %s\n",__func__, topic->child->string, topic->child->valuestring );
	}
	int ax = ax_event_handler_subscribe( EVENTS_HANDLER, keyset, &(declarationID),(AXSubscriptionCallback)EVENTS_Main_Callback,(gpointer)event,NULL);
	ax_event_key_value_set_free(keyset);
	if( !ax ) {
		LOG_WARN("EVENTS_Subscribe: Unable to subscribe to event\n");
		return 0;
	}
	cJSON_AddItemToArray(EVENTS_SUBSCRIPTIONS,cJSON_CreateNumber(declarationID));
	if( cJSON_GetObjectItem(event,"id") )
		cJSON_ReplaceItemInObject( event,"subid",cJSON_CreateNumber(declarationID));
	else
		cJSON_AddNumberToObject(event,"subid",declarationID);
	LOG_TRACE("Event subsription %d OK\n",declarationID);
	return declarationID;
}

int
EVENTS_Unsubscribe() {
	LOG_TRACE("EVENTS_Unsubscribe\n");
	if(!EVENTS_SUBSCRIPTIONS)
		return 0;
	cJSON* event = EVENTS_SUBSCRIPTIONS->child;
	while( event ) {
		ax_event_handler_unsubscribe( EVENTS_HANDLER, (guint)event->valueint, 0);
		event = event->next;
	};
	cJSON_Delete( EVENTS_SUBSCRIPTIONS );
	EVENTS_SUBSCRIPTIONS = cJSON_CreateArray();
}

int
EVENTS_Add_Event( const char *id, const char* name, int state ) {
	AXEventKeyValueSet *set = NULL;
	int dummy_value = 0;
	guint declarationID;

	if( !EVENTS_HANDLER || !id || !name || !EVENTS_DECLARATIONS) {
		LOG_WARN("EVENTS_Add_Event: Invalid input\n");
		return 0;
	}

	LOG_TRACE("%s: Entry: ID=%s Name=%s State=%s\n",__func__,id, name, state?"Stateful":"Stateless");

	set = ax_event_key_value_set_new();
	
	ax_event_key_value_set_add_key_value( set, "topic0", "tnsaxis", "CameraApplicationPlatform", AX_VALUE_TYPE_STRING,NULL);
	ax_event_key_value_set_add_key_value( set, "topic1", "tnsaxis", EVENTS_PACKAGE , AX_VALUE_TYPE_STRING,NULL);
	ax_event_key_value_set_add_nice_names( set, "topic1", "tnsaxis", EVENTS_PACKAGE, EVENTS_APPNAME, NULL);
	ax_event_key_value_set_add_key_value( set, "topic2", "tnsaxis", id, AX_VALUE_TYPE_STRING,NULL);
	ax_event_key_value_set_add_nice_names( set, "topic2", "tnsaxis", id, name, NULL);

	int ax = 0;
	if( state ) {
		ax_event_key_value_set_add_key_value(set,"state", NULL, &dummy_value, AX_VALUE_TYPE_BOOL,NULL);
		ax_event_key_value_set_mark_as_data(set, "state", NULL, NULL);
		ax = ax_event_handler_declare(EVENTS_HANDLER, set, 0,&declarationID,NULL,NULL,NULL);
	} else {
		ax_event_key_value_set_add_key_value(set,"value", NULL, &dummy_value, AX_VALUE_TYPE_INT,NULL);
		ax_event_key_value_set_mark_as_data(set, "value", NULL, NULL);
		ax = ax_event_handler_declare(EVENTS_HANDLER, set, 1,&declarationID,NULL,NULL,NULL);
	}

	if( !ax ) {
		LOG_WARN("Error declaring event\n");
		ax_event_key_value_set_free(set);
		return 0;
	}
	LOG_TRACE("EVENTS_Add_Event: %s %s\n", id, name );
	cJSON_AddNumberToObject(EVENTS_DECLARATIONS,id,declarationID);
	ax_event_key_value_set_free(set);
	return declarationID;
}	

int
EVENTS_Remove_Event(const char *id ) {
	AXEventKeyValueSet *set = NULL;
	int dummy_value = 0;
	guint declarationID;

	if( !EVENTS_HANDLER || !id || !EVENTS_DECLARATIONS) {
		LOG_TRACE("EVENTS_Remove_Event: Invalid input\n");
		return 0;
	}
	
	cJSON *event = cJSON_DetachItemFromObject(EVENTS_DECLARATIONS,id);
	if( !event ) {
		LOG_WARN("Error remving event %s.  Event not found\n",id);
		return 0;
	}

	ax_event_handler_undeclare( EVENTS_HANDLER, event->valueint, NULL);
	cJSON_Delete( event);
	return 1;
}

int
EVENTS_Fire_State( const char* id, int value ) {
	LOG_TRACE("EVENTS_Fire_State: %s %d\n", id, value );
	
	if( !EVENTS_DECLARATIONS || !id ) {
		LOG_WARN("EVENTs_Fire_State: Error send event\n");
		return 0;
	}

	cJSON *event = cJSON_GetObjectItem(EVENTS_DECLARATIONS, id );
	if(!event) {
		LOG_WARN("Error sending event %s.  Event not found\n", id);
		return 0;
	}

	AXEventKeyValueSet* set = ax_event_key_value_set_new();

	if( !ax_event_key_value_set_add_key_value(set,"state",NULL , &value,AX_VALUE_TYPE_BOOL,NULL) ) {
		ax_event_key_value_set_free(set);
		LOG_WARN("EVENT_Fire_State: Could not send event %s.  Internal error\n", id);
		return 0;
	}

	AXEvent* axEvent = ax_event_new2(set, NULL);
	ax_event_key_value_set_free(set);

	if( !ax_event_handler_send_event(EVENTS_HANDLER, event->valueint, axEvent, NULL) )  {
		LOG_WARN("EVENT_Fire_State: Could not send event %s\n", id);
		ax_event_free(axEvent);
		return 0;
	}
	ax_event_free(axEvent);
	LOG_TRACE("EVENT_Fire_State: %s %d fired\n", id,value );
	return 1;
}

int
EVENTS_Fire( const char* id ) {
	GError *error = NULL;
	
	LOG_TRACE("%s: %s\n", __func__, id );
	
	if( !EVENTS_DECLARATIONS || !id ) {
		LOG_WARN("EVENTs_Fire_State: Error send event\n");
		return 0;
	}

	cJSON *event = cJSON_GetObjectItem(EVENTS_DECLARATIONS, id );
	if(!event) {
		LOG_WARN("%s: Event %s not found\n",__func__, id);
		return 0;
	}

	AXEventKeyValueSet* set = ax_event_key_value_set_new();

	guint value = 1;
	if( !ax_event_key_value_set_add_key_value(set,"value",NULL, &value,AX_VALUE_TYPE_INT,&error) ) {
		ax_event_key_value_set_free(set);
		LOG_WARN("%s: %s error %s\n", __func__,id,error->message);
		g_error_free(error);
		return 0;
	}

	AXEvent* axEvent = ax_event_new2(set,NULL);
	ax_event_key_value_set_free(set);

	if( !ax_event_handler_send_event(EVENTS_HANDLER, event->valueint, axEvent, &error) )  {
		LOG_WARN("%s: Could not send event %s %s\n",__func__, id, error->message);
		ax_event_free(axEvent);
		g_error_free(error);
		return 0;
	}
	ax_event_free(axEvent);
	LOG_TRACE("%s: %s fired %d\n",__func__,  id,value );
	return 1;
}

int
EVENTS_Add_Event_JSON( cJSON* event ) {
	AXEventKeyValueSet *set = NULL;
	GError *error = NULL;
	int dummy_value = 0;
	guint declarationID;
	char* json = 0;
	int success = 0;	
	set = ax_event_key_value_set_new();
	
	char *eventID = cJSON_GetObjectItem(event,"id")?cJSON_GetObjectItem(event,"id")->valuestring:0;
	char *eventName = cJSON_GetObjectItem(event,"name")?cJSON_GetObjectItem(event,"name")->valuestring:0;
	
	if(!eventID) {
		LOG_WARN("%s: Invalid id\n",__func__);
		return 0;
	}

	ax_event_key_value_set_add_key_value( set, "topic0", "tnsaxis", "CameraApplicationPlatform", AX_VALUE_TYPE_STRING,NULL);
	ax_event_key_value_set_add_key_value( set, "topic1", "tnsaxis", EVENTS_PACKAGE , AX_VALUE_TYPE_STRING,NULL);
	ax_event_key_value_set_add_nice_names( set, "topic1", "tnsaxis", EVENTS_PACKAGE, EVENTS_APPNAME, NULL);
	ax_event_key_value_set_add_key_value( set, "topic2", "tnsaxis", eventID, AX_VALUE_TYPE_STRING,NULL);
	if( eventName )
		ax_event_key_value_set_add_nice_names( set, "topic2", "tnsaxis", eventID, eventName, NULL);

	if( cJSON_GetObjectItem(event,"show") && cJSON_GetObjectItem(event,"show")->type == cJSON_False )
		ax_event_key_value_set_mark_as_user_defined(set,eventID,"tnsaxis","isApplicationData",NULL);

	int defaultInt = 0;
	double defaultDouble = 0;
	char defaultString[] = "";
	cJSON* source = cJSON_GetObjectItem(event,"source")?cJSON_GetObjectItem(event,"source")->child:0;
	while( source ) {
		cJSON* property = source->child;
		if( property ) {
			if(strcmp(property->valuestring,"string") == 0 )
				success = ax_event_key_value_set_add_key_value(set,property->string,NULL, &defaultString,AX_VALUE_TYPE_STRING,NULL);
			if(strcmp(property->valuestring,"int") == 0 )
				success = ax_event_key_value_set_add_key_value(set,property->string,NULL, &defaultInt,AX_VALUE_TYPE_INT,NULL);
			if(strcmp(property->valuestring,"bool") == 0 )
				success = ax_event_key_value_set_add_key_value(set,property->string,NULL, &defaultInt,AX_VALUE_TYPE_BOOL,NULL);
			ax_event_key_value_set_mark_as_source(set, property->string, NULL, NULL);
			LOG_TRACE("%s: %s Source %s %s\n",__func__,eventID,property->string,property->valuestring);
		}
		source = source->next;
	}

	cJSON* data = cJSON_GetObjectItem(event,"data")?cJSON_GetObjectItem(event,"data")->child:0;
	
	int propertyCounter = 0;
	while( data ) {
		cJSON* property = data->child;
		if( property ) {
			propertyCounter++;
			if(strcmp(property->valuestring,"string") == 0 ) {
				if( !ax_event_key_value_set_add_key_value(set,property->string,NULL, &defaultString,AX_VALUE_TYPE_STRING,&error) ) {
					LOG_WARN("%s: Unable to add string %s %s\n",__func__,property->string,error->message);
					g_error_free(error);
				} else {
					LOG_TRACE("%s: Added string %s\n",__func__,property->string);
				}
			}
			if(strcmp(property->valuestring,"int") == 0 )
				ax_event_key_value_set_add_key_value(set,property->string,NULL, &defaultInt,AX_VALUE_TYPE_INT,NULL);
			if(strcmp(property->valuestring,"double") == 0 )
				ax_event_key_value_set_add_key_value(set,property->string,NULL, &defaultDouble,AX_VALUE_TYPE_DOUBLE,NULL);
			if(strcmp(property->valuestring,"bool") == 0 )
				ax_event_key_value_set_add_key_value(set,property->string,NULL, &defaultInt,AX_VALUE_TYPE_BOOL,NULL);
			ax_event_key_value_set_mark_as_data(set, property->string, NULL, NULL);
			LOG_TRACE("%s: %s Data %s %s\n",__func__,eventID,property->string,property->valuestring);
		}
		data = data->next;
	}

	if( propertyCounter == 0 )  //An empty event requires at least one property
		ax_event_key_value_set_add_key_value(set,"value",NULL, &defaultInt,AX_VALUE_TYPE_INT,NULL);
	
	if( cJSON_GetObjectItem(event,"state") && cJSON_GetObjectItem(event,"state")->type == cJSON_True ) {
		ax_event_key_value_set_add_key_value(set,"state",NULL, &defaultInt,AX_VALUE_TYPE_BOOL,NULL);
		ax_event_key_value_set_mark_as_data(set, "state", NULL, NULL);
		LOG_TRACE("%s: %s is stateful\n",__func__,eventID);
		success = ax_event_handler_declare(EVENTS_HANDLER, set, 0,&declarationID,NULL,NULL,NULL);
	} else {
		success = ax_event_handler_declare(EVENTS_HANDLER, set, 1,&declarationID,NULL,NULL,NULL);
	}
	LOG_TRACE("%s: %s ID = %d\n",__func__,eventID,declarationID);
	if( !success ) {
		ax_event_key_value_set_free(set);
		LOG_WARN("Unable to register event %s\n",eventID);
		return 0;
	}
	cJSON_AddNumberToObject(EVENTS_DECLARATIONS,eventID,declarationID);
	ax_event_key_value_set_free(set);
	
/*	
	char *json;
	json = cJSON_PrintUnformatted(event);
	if( json ) {
		LOG("%s: %s\n",__func__,json);
		free(json);
	}
*/	
	return 1;
}


int
EVENTS_Fire_JSON( const char* Id, cJSON* data ) {
	AXEventKeyValueSet *set = NULL;
	int boolValue;
	float floatValue;
	GError *error = NULL;
	LOG_TRACE("%s: Entry\n",__func__);
	if(!data)
		return 0;

	cJSON *event = cJSON_GetObjectItem(EVENTS_DECLARATIONS, Id );
	if(!event) {
		LOG_WARN("Error sending event %s.  Event not found\n",Id);
		return 0;
	}


	set = ax_event_key_value_set_new();
	int success = 0;
	cJSON* property = data->child;
	while(property) {
		if(property->type == cJSON_True ) {
			boolValue = 1;
			success = ax_event_key_value_set_add_key_value(set,property->string,NULL , &boolValue,AX_VALUE_TYPE_BOOL,NULL);
			LOG_TRACE("%s: Set %s %s = True\n",__func__,Id,property->string);
		}
		if(property->type == cJSON_False ) {
			boolValue = 0;
			success = ax_event_key_value_set_add_key_value(set,property->string,NULL , &boolValue,AX_VALUE_TYPE_BOOL,NULL);
			LOG_TRACE("%s: Set %s %s = False\n",__func__,Id,property->string);
		}
		if(property->type == cJSON_String ) {
			success = ax_event_key_value_set_add_key_value(set,property->string,NULL , property->valuestring,AX_VALUE_TYPE_STRING,NULL);
			LOG_TRACE("%s: Set %s %s = %s\n",__func__,Id,property->string,property->valuestring);
		}
		if(property->type == cJSON_Number ) {
			success = ax_event_key_value_set_add_key_value(set,property->string,NULL , &property->valuedouble,AX_VALUE_TYPE_DOUBLE,NULL);
			LOG_TRACE("%s: Set %s %s = %f\n",__func__,Id,property->string,property->valuedouble);
		}
		if(!success)
			LOG_WARN("%s: Unable to add property\n",__func__);
		property = property->next;
	}

	AXEvent* axEvent = ax_event_new2(set, NULL);

	ax_event_key_value_set_free(set);

	success = ax_event_handler_send_event(EVENTS_HANDLER, event->valueint, axEvent, &error);
	if(!success)  {
		LOG_WARN("%s: Could not send event %s id = %d %s\n",__func__, Id, event->valueint, error->message);
		ax_event_free(axEvent);
		g_error_free(error);
		return 0;
	}
	ax_event_free(axEvent);
	return 1;
}

cJSON*
EVENTS() {
	LOG_TRACE("%s:\n",__func__);
	//Get the ACAP package ID and Nice Name
	cJSON* manifest = APP_Service("manifest");
	if(!manifest)
		return cJSON_CreateNull();
	cJSON* acapPackageConf = cJSON_GetObjectItem(manifest,"acapPackageConf");
	if(!acapPackageConf)
		return cJSON_CreateNull();
	cJSON* setup = cJSON_GetObjectItem(acapPackageConf,"setup");
	if(!setup)
		return cJSON_CreateNull();
	sprintf(EVENTS_PACKAGE,"%s", cJSON_GetObjectItem(setup,"appName")->valuestring);
	sprintf(EVENTS_APPNAME,"%s", cJSON_GetObjectItem(setup,"friendlyName")->valuestring);
	
	LOG_TRACE("%s: %s %s\n",__func__,EVENTS_PACKAGE,EVENTS_APPNAME);

	EVENTS_SUBSCRIPTIONS = cJSON_CreateArray();
	EVENTS_DECLARATIONS = cJSON_CreateObject();
	EVENTS_HANDLER = ax_event_handler_new();
	
	cJSON* events = FILE_Read( "html/config/events.json" );
	if(!events)
		LOG_WARN("Cannot load even event list\n")
	cJSON* event = events?events->child:0;
	while( event ) {
		EVENTS_Add_Event_JSON( event );
		event = event->next;
	}
	return events;
}
