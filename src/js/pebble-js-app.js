var initialized = false;
var divider = 1000000;
var lat1 = 0;
var lon1 = 0;
var lat2;
var lon2;
var head = 0;
var dist = 0;
var units = "metric";
var R = 6371000; // m
var locationWatcher;
var locationOptions = {timeout: 15000, maximumAge: 15000, enableHighAccuracy: true };
var setPebbleToken = "JG36";

Pebble.addEventListener("ready", function(e) {
  if (typeof(Number.prototype.toRad) === "undefined") {
    Number.prototype.toRad = function() {
      return this * Math.PI / 180;
    }
  }
  if (typeof(Number.prototype.toDeg) === "undefined") {
     Number.prototype.toDeg = function() {
      return this * 180 / Math.PI;
     }
  }
  lat2 = parseFloat(localStorage.getItem("lat2")) || null;
  lon2 = parseFloat(localStorage.getItem("lon2")) || null;
  units = localStorage.getItem("units") || "metric";
  if ((lat2 === null) && (lon2 === null)) {
    storeCurrentPosition();
  }
  else {
    console.log("Target location known:" + lon2 + ',' + lat2);
  }
  startWatcher();
  console.log(e.type);
  initialized = true;
  console.log("JavaScript app ready and running! " + e.ready);
});

Pebble.addEventListener("appmessage",
  function(e) {
    if (e && e.payload && e.payload.cmd) {
      console.log("Got command: " + e.payload.cmd);
      switch (e.payload.cmd) {
        case 'set':
          storeCurrentPosition();
          break;
        case 'clear':
          lat2 = null;
          lon2 = null;
          break;
        case 'quit':
          navigator.geolocation.clearWatch(locationWatcher);
          break;
        default:
          console.log("Unknown command!");
      }
    }
  }
);

Pebble.addEventListener("showConfiguration",
  function() {
    var uri = "http://x.setpebble.com/api/" + setPebbleToken + "/" + Pebble.getAccountToken();
    console.log("Configuration url: " + uri);
    Pebble.openURL(uri);
  }
);

Pebble.addEventListener("webviewclosed",
  function(e) {
    var options = JSON.parse(decodeURIComponent(e.response));
    console.log("Webview window returned: " + JSON.stringify(options));
    var units = options["Units"];
    localStorage.setItem("units", units);
  }
);

function sendMessage(dict) {
  Pebble.sendAppMessage(dict, appMessageAck, appMessageNack);
  console.log("Sent message to Pebble! " + JSON.stringify(dict));
}

function appMessageAck(e) {
  console.log("Message accepted by Pebble!");
}

function appMessageNack(e) {
  console.log("Message rejected by Pebble! " + e.error);
}

function locationSuccess(position) {
  console.log("Got location " + position.coords.latitude + ',' + position.coords.longitude);
  lat1 = position.coords.latitude;
  lon1 = position.coords.longitude;
  calculate();
}

function storeLocation(position) {
  lat2 = position.coords.latitude;
  lon2 = position.coords.longitude;
  localStorage.setItem("lat2", lat2);
  localStorage.setItem("lon2", lon2);
  console.log("Stored location " + position.coords.latitude + ',' + position.coords.longitude);
  calculate();
}

function calculate() {  
  if (lat2 || lon2) {
    if ((Math.round(lat2*divider) == Math.round(lat1*divider)) && 
        (Math.round(lon2*divider) == Math.round(lon1*divider))) {
      console.log("Not moved yet, still at  " + lat1 + ',' + lon1);
      dist = 0;
      head = 0;
      var msg = {"dist": parseInt(dist),
                 "head": parseInt(head)};
      sendMessage(msg);
      return;
    }
    console.log("Moved to  " + lat1 + ',' + lon1);
    if (!lat2) {
      lat2 = 0;
    }
    if (!lon2) {
      lon2 = 0;
    }
    console.log("Found stored point " + lat2 + ',' + lon2);
    var dLat = (lat2-lat1).toRad();
    console.log("Latitude difference (radians): " + dLat);
    var dLon = (lon2-lon1).toRad();
    console.log("Longitude difference (radians): " + dLon);
    var l1 = lat1.toRad();
    var l2 = lat2.toRad();
    console.log("current and stored latitudes in radians: " + l1 + ',' + l2);

    var a = Math.sin(dLat/2) * Math.sin(dLat/2) +
            Math.sin(dLon/2) * Math.sin(dLon/2) * Math.cos(l1) * Math.cos(l2); 
    var c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a)); 
    dist = Math.round(R * c);
    console.log("Calculated dist " + dist);
    
    var y = Math.sin(dLon) * Math.cos(l2);
    var x = Math.cos(l1)*Math.sin(l2) -
            Math.sin(l1)*Math.cos(l2)*Math.cos(dLon);
    head = Math.round(Math.atan2(y, x).toDeg());
    console.log("Calculated head " + head);
    var msg = {"dist": dist,
               "head": head,
               "units": units};
    sendMessage(msg);
  }
  else {
    console.log("Will not calculate: no lat2 and lon2: " + lat2 + ',' + lon2);
  }
}

function locationError(error) {
  console.warn('location error (' + error.code + '): ' + error.message);
}

function storeCurrentPosition() {
  console.log("Attempting to store current position.");
  var opts = { "timeout": 15000, "maximumAge": 1000, "enableHighAccuracy": true }; 
  navigator.geolocation.getCurrentPosition(storeLocation, locationError, opts);
}

function startWatcher() {
  if (!locationWatcher) {
   locationWatcher = navigator.geolocation.watchPosition(locationSuccess, locationError, locationOptions);
   // for testing: randomize movement!
/*
   window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
   locationWatcher = window.setInterval(function() {
    lat1 = lat1 + Math.random()/100;
    lon1 = lon1 - Math.random()/100;
    calculate();
   }, 5000);
   console.log("Started location watcher: " + locationWatcher);
*/
  }
  else {
   console.log("Location watcher already running: " + locationWatcher);
  }
}
