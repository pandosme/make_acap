/*
Pandos (c) 2018

See eventselector.js for example

*/

function soap_EventDeclarations( successCallBack ) {
  var xml = "";
  xml += '<?xml version="1.0" encoding="utf-8"?>';
  xml += '<soap:Envelope xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:aev="http://www.axis.com/vapix/ws/event1" xmlns:tns1="http://www.onvif.org/ver10/topics" xmlns:tnsaxis="http://www.axis.com/2009/event/topics" xmlns:wsnt="http://docs.oasis-open.org/wsn/b-2" xmlns:soap="http://www.w3.org/2003/05/soap-envelope">';
  xml += '<soap:Body>';
  xml += '<aev:GetEventInstances xmlns="http://www.axis.com/vapix/ws/event1"></aev:GetEventInstances>';
  xml += '</soap:Body>';
  xml += '</soap:Envelope>';

	$.ajax({type: "POST",url: "/vapix/services",dataType: "xml", data: xml, cache: true,
    success: function( xmlDoc ) {
                var topicSet = xmlDoc.getElementsByTagName("wstop:TopicSet")[0];
                if( topicSet.length == 0 ){
                  alert("soap_EventDeclarations: Parse Error");
                  return;
                }
                var events = {
                  topic: "wstop:TopicSet",
                  name: "Topics",
                  uid: 0,
                  state: null,
                  filter: null,
                  children: null
                };
                events.children = parse_children( topicSet.children, "" );
                topic_cleanup( events.children );
                topic_unique_events( events.children );
                topic_display_name( events.children, "" );
                if( !(typeof successCallBack === 'undefined') )
                  successCallBack( events );
    },
    error: function( response ) {
      alert("soap_EventDeclarations: Response error")
    }
  });
};

function find_topic( topics, topicString ) {
  if( !topics )
    return false;
  if( topics.length == 0 )
    return false;
  for( var i = 0; i < topics.length; i++ ) {
    result = find_topic( topics[i],topicString );
    if( result != false )
      return result;
    if( topics[i].topic == topicString )
      return topics[i];
    result = find_topic( topics[i].children,topicString );
    if( result != false )
      return result;
  }  
  return false;
}

function remove_topic( topics, topicString ) {
  if( !topics )
    return false;
  if( topics.length == 0 )
    return false;
  for( var i = 0; i < topics.length; i++ ) {
    result = remove_topic( topics[i],topicString );
    if( result )
      return true;
    if( topics[i].topic == topicString ) {
      topics.splice(i, 1 );
      return true;
    }
    result = remove_topic( topics[i].children,topicString );
    if( result )
      return result;
  }  
  return false;
}


function topic_cleanup( topics ) {
  var ruleengine = find_topic( topics, "tns1:RuleEngine" );
  var acap = find_topic( topics, "tnsaxis:CameraApplicationPlatform" );
  if( acap ) {
    acap.name = "ACAP";
    if( ruleengine ) {
      acap.children.push.apply( acap.children, ruleengine.children );
      remove_topic( topics, "tns1:RuleEngine" );
    }
  }
  var timer = find_topic( topics, "tns1:UserAlarm/tnsaxis:Recurring/Pulse" );
  if( timer ) {
    remove_topic( topics, "tns1:UserAlarm/tnsaxis:Recurring/Pulse" );
    timer.name = "Timer";
    topics.push( timer );
  }
  var schedules = find_topic( topics, "tns1:UserAlarm/tnsaxis:Recurring/Interval" );
  if( schedules ) {
    remove_topic( topics, "tns1:UserAlarm/tnsaxis:Recurring/Interval" );
    schedules.name = "Schedules";
    topics.push( schedules );
  }
  remove_topic( topics , "tns1:UserAlarm" );
  var exceptions = find_topic( topics, "tns1:Device" );
  if( exceptions )
    exceptions.name = "Exceptions";
  remove_topic( topics , "tns1:PTZController/tnsaxis:ControlQueue");
  var detectors = {
    topic: "detectors",
    name: "Detectors",
    filter: null,
    state: null,
    source: null,
    data: null,
    children: []
  };
  var moveTopic = find_topic( topics, "tns1:Device/tnsaxis:IO");
  if( moveTopic ) {
    detectors.children.push.apply( detectors.children, moveTopic.children );
    remove_topic( topics, "tns1:Device/tnsaxis:IO" );
  }
  var moveTopic = find_topic( topics, "tns1:AudioSource" );
  if( moveTopic ) {
    detectors.children.push.apply( detectors.children, moveTopic.children );
    remove_topic( topics, "tns1:AudioSource" );
  }
  var moveTopic = find_topic( topics, "tns1:VideoSource" );
  if( moveTopic ) {
    detectors.children.push.apply( detectors.children, moveTopic.children );
    remove_topic( topics, "tns1:VideoSource" );
  }
  var moveTopic = find_topic( topics, "tns1:RecordingConfig");
  if( moveTopic ) {
    detectors.children.push.apply( detectors.children, moveTopic.children );
    remove_topic( topics, "tns1:RecordingConfig" );
  }
  var moveTopic = find_topic( topics, "tns1:PTZController" );
  if( moveTopic ) {
    moveTopic.name = "PTZ";
    detectors.children.push.apply( detectors.children, moveTopic.children );
    remove_topic( topics, "tns1:PTZController" );
  }
  var moveTopic = find_topic( topics, "tnsaxis:Storage" );
  if( moveTopic ) {
    detectors.children.push.apply( detectors.children, moveTopic.children );
    remove_topic( topics, "tnsaxis:Storage" );
  }
  var moveTopic = find_topic( topics, "tns1:Device/tnsaxis:Status" );
  if( moveTopic ) {
    detectors.children.push.apply( detectors.children, moveTopic.children );
    remove_topic( topics, "tns1:Device/tnsaxis:Status" );
  }
  topics.push(detectors);
}

String.prototype.hashCode = function() {
  var hash = 0, i, chr;
  if (this.length === 0) return hash;
  for (i = 0; i < this.length; i++) {
    chr   = this.charCodeAt(i);
    hash  = ((hash << 5) - hash) + chr;
    hash |= 0; // Convert to 32bit integer
  }
  hash = Math.abs( hash );
  return hash;
};

function topic_unique_events( topics ) {
  if( !topics )
    return false;
  if( topics.length == 0 )
    return false;
  for( var i = 0; i < topics.length; i++ ) {
    topics.filter = null;    
    if( topics[i].source != null || topics[i].data != null ) {
      var xEvent = topics[i];
      if( xEvent.source ) {
        var name = xEvent.source[0].name;
        var type = xEvent.source[0].type;
        var values = xEvent.source[0].values;
        var template = JSON.parse(JSON.stringify(xEvent));
        xEvent.children = [];
        for( var v = 0; v < values.length; v++ ) {
          var subEvent = JSON.parse(JSON.stringify(template));
          subEvent.name = values[v].name;
          subEvent.filter = {
            name: name,
            type: type,
            value: values[v].value,
            onvif: type + '(//SimpleItem[@Name="' + name + '" and @Value="' + values[v].value +'")'
          };
          var fullstring = subEvent.topic;
          if( subEvent.filter && subEvent.filter.onvif )
            fullstring += subEvent.filter.onvif;
          subEvent.uid = fullstring.hashCode();
          delete subEvent.source;
          delete subEvent.data;
          delete subEvent.isApplicationData;
          xEvent.children.push( subEvent );
        }
      }
    }
    delete topics[i].source;
    delete topics[i].data;
    delete topics[i].isApplicationData;
    topic_unique_events( topics[i].children );
  }
}

function topic_display_name( topics, parentName ) {
  if( !topics )
    return false;
  if( topics.length == 0 )
    return false;
  for( var i = 0; i < topics.length; i++ ) {
    if( parentName.length )
      topics[i].displayName = parentName + "/" + topics[i].name;
    else
      topics[i].displayName = topics[i].name;
    topic_display_name( topics[i].children, topics[i].displayName );
  }
}


function parse_children( node, parentTopic ) {
  var reply = [];
  
  for( var i = 0; i < node.length; i++ ) {
    var topic = {
      topic: node[i].tagName,
      name: node[i].getAttribute('aev:NiceName'),
      uid: node[i].tagName.hashCode(),
      isApplicationData: node[i].getAttribute('isApplicationData') == "true",
      state: null,
      filter: null,
      source: null,
      data: null,
      children: null
    }
    if( topic.name == null ) {
      var namespace = topic.topic.split(":");
      if( namespace.length > 1 )
        topic.name = namespace[1];
      else
        topic.name = namespace[0];
    }
    if( parentTopic.length )
      topic.topic = parentTopic + "/" + topic.topic;
    if( node[i].getAttribute('wstop:topic') )
      parse_event( topic, node[i] );
    else
      topic.children = parse_children( node[i].children, topic.topic );
    if( !topic.isApplicationData )
      reply.push( topic );
  }
  return reply;
}

function parse_event( topic, node ) {
  var sourceNode = node.getElementsByTagName('aev:SourceInstance')[0];
  if( sourceNode ) {
    topic.source = []; 
    for( var i = 0; i < sourceNode.children.length; i++ ) {
      var instance = {
        name: sourceNode.children[i].getAttribute('Name'),
        type: sourceNode.children[i].getAttribute('Type'),
        values: [],
      };
      instance.type = instance.type.split(":")[1];  //Remove Namespace
      var xNode = sourceNode.children[i];
      var sourceValues = xNode.children;
      var noName = false;
      for( var x = 0; x < sourceValues.length; x++ ) {
        noName = false;
        var value = {
          name: sourceValues[x].getAttribute('aev:NiceName'),
          value: sourceValues[x].textContent
        }
        if( value.name == null ) {
          noName = true;
          value.name = value.value;
        }
        instance.values.push( value );
      }
      if( instance.values.length == 0 )
        topic.source = null;
      if( noName &&  instance.values.length == 1 )
        topic.source = null;
      if( topic.source )
        topic.source.push( instance );
    }
  }
  var dataNode = node.getElementsByTagName('aev:DataInstance')[0];
  if( dataNode ) {
    topic.data = []; 
    for( var i = 0; i < dataNode.children.length; i++ ) {
      var instance = {
        name: dataNode.children[i].getAttribute('Name'),
        type: dataNode.children[i].getAttribute('Type'),
        isPropertyState: dataNode.children[i].getAttribute('isPropertyState') == "true",
        values: [],
      };
      instance.type = instance.type.split(":")[1];  //Remove Namespace
      var xNode = dataNode.children[i];
      var dataValues = xNode.children;
      var noName = false;
      for( var x = 0; x < dataValues.length; x++ ) {
        noName = false;
        var value = {
          name: dataValues[x].getAttribute('aev:NiceName'),
          value: dataValues[x].textContent
        }
        if( value.name == null ) {
          noName = true;
          value.name = value.value;
        }
        instance.values.push( value );
      }
      if( instance.isPropertyState ) {
        topic.state = instance.name;
      } else {
        if( instance.values.length == 0 )
          topic.data = null;
        if( noName &&  instance.values.length == 1 )
          topic.data = null;
        if( topic.data )
          topic.data.push( instance );
      }
    }
  }
  if( topic.source && topic.source.length == 0 )
    topic.source = null;
  if( topic.data && topic.data.length == 0 )
    topic.data = null;
}
