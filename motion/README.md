# ACAPP MOTION

Example how to get Axis Motion data (MOTE) with a user interface that provides configuration and a video with a simple motion box augmetation (data is update 2 times/s).  Motion bounding box coordinate system is [0,0] - [1000,1000] regardless of aspect ration and rotation.  Origo in the top left corner.  The rotation setting must match the cameras image rotation.  
Default settings for motion configuration is stored under source/html/config/mote.json.  

API:
GET /app		All app data
GET /settings	App specific settings
GET /mote		Motion configuration
GET /last5		Last 5 motion detections 
