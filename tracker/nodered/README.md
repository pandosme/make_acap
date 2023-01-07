# Tracker Node-RED visualizer example

A Node-RED flow that shows an example how to visualize Tracker data in a dashboard template node using HTML canvas.  The aedes node provides an MQTT Broker in Node-RED on port 1880.  If you already have a broker you can remove the aedes node and change the MQTT input to point to your broker.

Import the following nodes (use Manage Palette) to your Node-RED before importing the example.json file into your Node-RED.

* [node-red-dashboard](https://flows.nodered.org/node/node-red-dashboard)
* [node-red-contrib-aedes](https://flows.nodered.org/node/node-red-contrib-aedes)
* [node-red-contrib-axis-com](https://flows.nodered.org/node/node-red-contrib-axis-com)
