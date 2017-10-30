//USING PAPERJS AS A REPLACEMENT FOR FREECAD, FOR FLAT LASERCUT DESIGNS
//then passing to Inkscape for normal laser-cutting toolchain (through CorelDraw, sigh)
//describing shape in tenths of a millimeter
//Inkscape (v<0.91) assumes 90 pixels : 1 inch
//so 100 units here = 100/90 inches, when it should equal 100/254 inches
//so scale factor = 90 / 254 = 0.35433
var scale = .35433; //pixel to .1mm
//.37795 for Inkscape v.90 or greater
var cuv_height = 540;
var cuv_diam = 246; //254
var cuv_lid_diam = 272; //280
var lip_height = 30;
var ridge_width = 90;
var width = 400;
var top_width = 475;
var led_diam = 50;
var led_offset = 15;
var screwhole_diam = 30;
var screwhole_offset = 85;
var height = cuv_height + lip_height*4;
var gap_width = width - 4 * lip_height - 2 * ridge_width;
var side_offset = width * 1.8;
var top_offset = new Point(0,width * 1.1);//-(top_width - width)/2,-(top_width - width)/2);
var origin = new Point(0,0);

var build_end = function(){
    var top_end = new Path.Rectangle(new Point(0,0), new Point(top_width,top_width));
    top_end.strokeColor = 'black';
    
    var top_slot1 = new Path.Rectangle(new Point(0,0), new Point(ridge_width,lip_height));
    top_slot1.strokeColor = 'black';
    top_slot1.translate(new Point(top_width/2, top_width/2));
    var top_slot2 = top_slot1.clone();
    var top_slot3 = top_slot1.clone();    
    var top_slot4 = top_slot1.clone();
    var top_slot5 = top_slot1.clone();
    top_slot1.translate(new Point(-gap_width/2 - ridge_width,-width/2));
    top_slot2.translate(new Point(gap_width/2, -width/2));
    top_slot3.translate(new Point(gap_width/2, width/2 - lip_height));
    top_slot4.translate(new Point(-gap_width/2 - ridge_width, width/2 - lip_height));
    top_slot5.rotate(-90);
    top_slot5.translate(-top_slot5.bounds._height/2, -top_slot5.bounds._width/2);
    top_slot5.translate(new Point(-width/2 + lip_height/2, -(gap_width + ridge_width)/2));
    var top_slot6 = top_slot5.clone();
    var top_slot7 = top_slot5.clone();
    var top_slot8 = top_slot5.clone();
    top_slot6.translate(new Point(width - lip_height, 0));
    top_slot7.translate(new Point(0,gap_width + ridge_width));
    top_slot8.translate(new Point(width - lip_height,gap_width + ridge_width));
    
    var top_hole = new Path.Circle({center: new Point(top_end.bounds._width/2,top_end.bounds._height/2),radius: cuv_lid_diam/2});
    top_hole.strokeColor = 'black';
    
    top_end.translate(top_offset);
    top_hole.translate(top_offset);
    top_slot1.translate(top_offset);
    top_slot2.translate(top_offset);
    top_slot3.translate(top_offset);
    top_slot4.translate(top_offset);
    top_slot5.translate(top_offset);
    top_slot6.translate(top_offset);
    top_slot7.translate(top_offset);
    top_slot8.translate(top_offset);
}

var build_side = function(){
    var side_top_seg = [
        new Point(lip_height, lip_height),
        new Point(lip_height*2, lip_height),
        new Point(lip_height*2, 0),
        new Point(lip_height*2 + ridge_width, 0),
        new Point(lip_height*2 + ridge_width, lip_height),
        new Point(lip_height*2 + ridge_width + gap_width, lip_height),
        new Point(lip_height*2 + ridge_width + gap_width, 0),
        new Point(lip_height*2 + ridge_width*2 + gap_width, 0),
        new Point(lip_height*2 + ridge_width*2 + gap_width, lip_height),
        new Point(width, lip_height),
    ];
    
    var side_top = new Path(side_top_seg);
    side_top.strokeColor = 'black';
    side_top.position = new Point(side_offset,lip_height/2);
    var side_bottom = side_top.clone();
    side_bottom.scale(-1,1);
    side_bottom.rotate(180);
    side_bottom.position = new Point(side_offset,height + lip_height *1.5);
    
    var side_left = new Path();
    side_left.add(new Point (0,0));
    side_left.add(new Point (0,height));
    side_left.strokeColor = 'black';
    side_left.position = new Point(side_offset - side_top.bounds._width / 2, height / 2 + lip_height)
    var side_right = side_left.clone();
    side_right.position = new Point(side_offset + side_top.bounds._width / 2, height / 2 + lip_height);
    
    var hole1 = new Path.Rectangle(new Point(0,0), new Point(ridge_width,lip_height));
    hole1.strokeColor = 'black';
    hole1.position = new Point(side_offset - side_top.bounds._width/2 + lip_height + ridge_width/2, 2 * lip_height + hole1.bounds._height/2);
    var hole2 = hole1.clone();
    var hole3 = hole1.clone();
    var hole4 = hole1.clone();
    var hole5 = hole1.clone();
    var hole6 = hole1.clone();
    hole2.translate(new Point(gap_width + ridge_width,0));
    hole3.translate(new Point(0,cuv_height/2));
    hole4.translate(new Point(gap_width + ridge_width,cuv_height/2));
    hole5.translate(new Point(0,cuv_height + lip_height));
    hole6.translate(new Point(gap_width + ridge_width,cuv_height + lip_height));
    
    var led_hole = new Path.Circle({center: new Point(0,0), radius: led_diam/2});
    led_hole.position = new Point(-side_top.bounds._width / 2 + lip_height + ridge_width + gap_width/2, led_offset + cuv_height * .78 + lip_height * 2);
    led_hole.translate(side_offset,0);
    led_hole.strokeColor = 'black';
    
    var bpw34_sensor_hole = new Path.Rectangle(new Point(0,0), new Point(50,50));
    bpw34_sensor_hole.position = new Point(-side_top.bounds._width / 2 + lip_height + ridge_width + gap_width/2, cuv_height * .78 + lip_height * 2);
    bpw34_sensor_hole.translate(side_offset,0);
    bpw34_sensor_hole.strokeColor = 'black';
    
    var pcb_screwhole1 = new Path.Circle({center: new Point(0,0), radius: screwhole_diam/2});
    pcb_screwhole1.position = new Point(-side_top.bounds._width / 2 + lip_height + ridge_width + gap_width/2,cuv_height * .78 + lip_height * 2);
    pcb_screwhole1.translate(side_offset,0);
    pcb_screwhole1.strokeColor = 'black';
    var pcb_screwhole2 = pcb_screwhole1.clone().translate(-screwhole_offset,-screwhole_offset);
    var pcb_screwhole3 = pcb_screwhole1.clone().translate(screwhole_offset,-screwhole_offset);
    var pcb_screwhole4 = pcb_screwhole1.clone().translate(-screwhole_offset,screwhole_offset);
    var pcb_screwhole5 = pcb_screwhole1.clone().translate(-screwhole_offset,led_offset-screwhole_offset);
    var pcb_screwhole6 = pcb_screwhole1.clone().translate(screwhole_offset,led_offset-screwhole_offset);
    var pcb_screwhole7 = pcb_screwhole1.clone().translate(-screwhole_offset,screwhole_offset-led_offset);
    var pcb_screwhole8 = pcb_screwhole1.clone().translate(screwhole_offset,screwhole_offset-led_offset);
    pcb_screwhole1.translate(screwhole_offset,screwhole_offset);
    
    
}

var build_middle = function(){
    var middle_outside_seg = [
        new Point(lip_height, lip_height),
        new Point(lip_height * 2, lip_height),
        new Point(lip_height * 2, 0),
        new Point(lip_height * 2 + ridge_width, 0),
        new Point(lip_height * 2 + ridge_width, lip_height),
        new Point(lip_height * 2 + ridge_width + gap_width, lip_height),
        new Point(lip_height * 2 + ridge_width + gap_width, 0),
        new Point(width - lip_height * 2, 0),
        new Point(width - lip_height * 2, lip_height),
        new Point(width - lip_height, lip_height)
    ]
    var middle_outside = new Path(middle_outside_seg);

    var middle_inside_center = new Point(width/2,width/2);

    var middle_inside = new Path.Circle({center: origin,radius: cuv_diam/2});
    middle_outside.strokeColor = 'black';
    middle_inside.strokeColor = 'black';
    middle_outside.position = new Point((width - lip_height*2)/2 + lip_height, lip_height/2);
    middle_inside.position = middle_inside_center;
    middle_outside2 = middle_outside.clone();
    middle_outside3 = middle_outside.clone();
    middle_outside4 = middle_outside.clone();
    middle_outside2.position = new Point((width - lip_height*2)/2 + lip_height, width - lip_height/2);
    middle_outside2.rotation = 180;
    middle_outside3.position = new Point(lip_height/2,width/2);
    middle_outside3.rotation = -90;
    middle_outside4.position = new Point(width - lip_height/2,width/2);
    middle_outside4.rotation = 90;
}

var downloadAsSVG = function (fileName) {
   if(!fileName) {fileName = "paperjs_example.svg"}
   var url = "data:image/svg+xml;utf8," + encodeURIComponent(paper.project.exportSVG({asString:true}));
   var link = document.createElement("a");
   link.download = fileName;
   link.href = url;
   link.click();
}

//------------------------------------------------
build_middle();
build_side();
build_end();
//downloadAsSVG();