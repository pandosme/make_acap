/*
Pandos (c) 2018

Displays dropdown list for users to select a device event.  It is optimized for jQuery and bootstrap web pages.
The input requires eventselector.js

HTML usage:

<head>
  ...
  <script src="jquery.min.js"></script>
  <script src="bootstrap.min.js"</script>
  <script src="soapEventDeclarations.js"></script>
  <script src="eventselector.js"></script>
  ...
</head>

<body>
  ...
  <label>Device Event</label>
  <div id="event_selector"></div>
  <script>
    $(document).ready( function() {
      soap_EventDeclarations( function( topicset  ) {
        eventSelector( $("#event_selector"), topicSet, function(user_selected_event){
          var the_user_selects = user_selected_event;
          ...
        });
      });
    });
  </script>    
  ...
</body>


-------------------------------------------------
Output (user_selected_evet):

{
  "topic": "tns1:Device/tnsaxis:IO/Port",
  "name": "Input 1",
  "uid": 454800307,
  "state": "state",
  "filter": {
    "name": "state",
    "type": "boolean",
    "value": "1",
    "onvif": "boolean(//SimpleItem[@Name=\"state\" and @Value=\"1\")"
   },
   "displayName": "Detectors/Digital input port/Input 1"
}  

*/

var eventSelector_topicRoot = null;
var eventSelector_selectionList = null;
var eventSelector_event = null;
var eventSelector_callback = null;

function eventSelector( parentDIV, topicSet, callback ) {
  eventSelector_callback = callback;
  eventSelector_topicRoot = topicSet;
  eventSelector_event = topicSet;
  eventSelector_selectionList = [0];
  parentDIV.append('<div class="eventSelectorContainer"></div>');
  eventSelector_refresh();
};

function eventSelector_refresh() {
  $(".eventSelectorContainer").empty();
  var level = 0;
  eventSelector_event = eventSelector_topicRoot;
  for( i = 0; i < eventSelector_selectionList.length; i++ ) {
    eventSelector_DrawLevelSelector( level, eventSelector_event.children );
    if( eventSelector_selectionList[i] > 0 ) {
      var childSelect = eventSelector_selectionList[i]-1;
      eventSelector_event = eventSelector_event.children[childSelect];
    }
    var xSelect = "select.topiclevelselector#"+level;
    var selector = $(xSelect);
    selector.val(eventSelector_selectionList[i]);
    level++;
  }
  if( eventSelector_callback ) {
    var export_event = JSON.parse(JSON.stringify(eventSelector_event));
    delete export_event.children;
    eventSelector_callback( export_event );
  }
}

function eventSelector_DrawLevelSelector( level, topic ) {
  var html = '';
  html += '<select class="form-control select topiclevelselector" id="' + level + '">';
  html += '<option value="0" class="form-control select">Select...</option>';
  for( var i = 0; i < topic.length; i++ ) {
    html += '<option value="'+ (i + 1) + '"class="form-control select">' + topic[i].name + '</option>';
  }
  html += '</select>';
  $(".eventSelectorContainer").append( html );
}

function eventSelector_getSelection() {
  var export_event = JSON.parse(JSON.stringify(eventSelector_event));
  delete export_event.children;
  return export_event;
}

function eventSelector_setSelection( queryEvent ) {
  if( !queryEvent ) 
    return;
  eventSelector_selectionList = [];
  eventSelector_event = eventSelector_topicRoot;
  if( !eventSelector_buildPath( queryEvent, eventSelector_topicRoot.children, eventSelector_selectionList ) )
    eventSelector_selectionList = [0];
  eventSelector_refresh();
}


function eventSelector_buildPath( event, topics , pathlist ) {
  if( !event )
    return false;
  if( !topics )
    return false;
  
  for( var i = 0; i < topics.length; i++ ) {
    if( !topics[i].children ) {
      if( event.topic == topics[i].topic ) {
        if( event.filter == null && topics[i].filter == null ) {
          pathlist.unshift( i + 1 );
          return true;
        }
        if( event.filter && topics[i].filter ) {
          if( event.filter.value == topics[i].filter.value ) {
            pathlist.unshift( i + 1 );
            return true;
          }
        }
      }
    }
    if( topics[i].children ) {
      result = eventSelector_buildPath( event, topics[i].children , pathlist );
      if( result == true ) {
        pathlist.unshift( i + 1 );
        return true;
      }
    }
  }  
  return false;
}

$(document).on("change", ".topiclevelselector", function(){
  var level = parseInt( this.id );
  var selection = parseInt( this.value );
  if( level < eventSelector_selectionList.length ) {
    eventSelector_selectionList[level] = selection;
    eventSelector_selectionList.splice( level+1,5 );
    eventSelector_event = eventSelector_topicRoot;
    for( var i = 0; i < eventSelector_selectionList.length; i++ )
      eventSelector_event = eventSelector_event.children[eventSelector_selectionList[i]-1];
  } else {
    eventSelector_event = eventSelector_event.children[selection-1];
    eventSelector_selectionList.push(selection);
  }
  if( eventSelector_event.children )
    eventSelector_selectionList.push(0);
  eventSelector_refresh();
});