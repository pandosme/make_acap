var AOI_Object = 0;
var AOI_Callback = 0;
var AOI_Settings = 0;
var AOI_Coord = 0;
var AOI_Type = "area";
function AOI_Moving( image, area ) {
	var mote = AOI_Coord.Mote_XY2XY(area);
	if( AOI_Settings == 0 )
		return;
	AOI_Callback( mote );
	if( AOI_Type == "area" ) {
		AOI_Settings.x1 = mote.x1;
		AOI_Settings.x2 = mote.x2;
		AOI_Settings.y1 = mote.y1;
		AOI_Settings.y2 = mote.y2;
	}
	if( AOI_Type == "size" ) {
		AOI_Settings.width = mote.x2-mote.x1;
		AOI_Settings.height = mote.y2-mote.y1;
	}
}

function AOI_Init( elementName, coordinates, callback ) {
	AOI_Callback = callback;
	AOI_Settings = 0;
	AOI_Coord = coordinates;
	var options = AOI_Coord.View_XY2XY( {x1: 0, y1: 0, x2: 1000, y2: 1000 } );
	options.onSelectChange = AOI_Moving;
	options.minHeight = 5;
	options.minWidth = 5;
	options.handles = true;
	options.hide = true;
	options.instance = true;
	AOI_Object = $(elementName).imgAreaSelect( options );
}

function AOI_Set( type, settings ) {
	AOI_Settings = settings;
	AOI_Type = type;
	if( settings === 0 ) {
		AOI_Object.setOptions({hide: true});
		return;
	}
	var mote = {
		x1: settings.x1,
		y1: settings.y1,
		x2: settings.x2,
		y2: settings.y2
	}
	if( type == "size" ) {
		mote = {
			x1: 500 - (settings.width/2),
			y1: 500 - (settings.height/2),
			x2: 500 + (settings.width/2),
			y2: 500 + (settings.height/2)
		};
	}
	area = AOI_Coord.View_XY2XY(mote);
	AOI_Callback( mote );
	AOI_Object.setSelection( area.x1, area.y1, area.x2, area.y2 );
	AOI_Object.setOptions({show: true});
}
