function COORD( x, y ) {
  this.x = x;
  this.y = y;
}

COORD.prototype = {
  constructor: COORD,
  View_WH2WH: function( mote ) {
    var data = {
     x: parseInt((mote.x / 1000) * this.x ),
     y: parseInt((mote.y / 1000) * this.y ),
     w: parseInt((mote.w / 1000) * this.x ),
     h: parseInt((mote.h / 1000) * this.y )
    }
    return data;
  },
  View_XY2XY: function( mote ) {
    var data = {
     x1: parseInt(parseInt(mote.x1) / 1000 * this.x),
     y1: parseInt(parseInt(mote.y1) / 1000 * this.y),
     x2: parseInt(parseInt(mote.x2) / 1000 * this.x),
     y2: parseInt(parseInt(mote.y2) / 1000 * this.y)
    }
    data.x1 = data.x1.toString();
    data.y1 = data.y1.toString();
    data.x2 = data.x2.toString();
    data.y2 = data.y2.toString();
    return data;
  },
  Mote_WH2WH: function( view ) {
    var data = {
     x: parseInt((view.x / this.x) * 1000 ),
     y: parseInt((view.y / this.y) * 1000 ),
     w: parseInt((view.w / this.x) * 1000 ),
     h: parseInt((view.h / this.y) * 1000 )
    }
    return data;
  },
  Mote_XY2XY: function( view ) {
    var data = {
     x1: parseInt((view.x1 / this.x) * 1000 ),
     y1: parseInt((view.y1 / this.y) * 1000 ),
     x2: parseInt((view.x2 / this.x) * 1000 ),
     y2: parseInt((view.y2 / this.y) * 1000 )
    }
    return data;
  }
}
