var initialized = false;
var divider = 1000000;
var interval;
var lat1 = 0;
var lon1 = 0;
var lat2;
var lon2;
var head = 0;
var dist = 0;
var units = "metric";
var R = 6371000; // m
var locationWatcher;
var locationInterval;
var locationOptions = {timeout: 15000, maximumAge: 1000, enableHighAccuracy: true };
var setPebbleToken = "DY3U";

Pebble.addEventListener("ready", function(e) {
  lat2 = parseFloat(localStorage.getItem("lat2")) || null;
  lon2 = parseFloat(localStorage.getItem("lon2")) || null;
  interval = parseInt(localStorage.getItem("interval")) || 0;
  units = localStorage.getItem("units") || "metric";
  if ((lat2 === null) || (lon2 === null)) {
    storeCurrentPosition();
  }
  else {
    // console.log("Target location known:" + lon2 + ',' + lat2);
  }
  startWatcher();
  // console.log(e.type);
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
    var uri = "http://x.setpebble.com/" + setPebbleToken + "/" + Pebble.getAccountToken();
    console.log("Configuration url: " + uri);
    Pebble.openURL(uri);
  }
);

Pebble.addEventListener("webviewclosed",
  function(e) {
    var options = JSON.parse(decodeURIComponent(e.response));
    console.log("Webview window returned: " + JSON.stringify(options));
    units = (options["1"] === 1) ? 'imperial' : 'metric';
    console.log("Units set to: " + units);
    localStorage.setItem("units", units);
    interval = options["2"] || 0;
    console.log("Interval set to: " + interval);
    localStorage.setItem("interval", interval);
    calculate();
    startWatcher();
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
  // console.log("Got location " + position.coords.latitude + ',' + position.coords.longitude + ', heading at ' + position.coords.heading);
  lat1 = position.coords.latitude;
  lon1 = position.coords.longitude;
  calculate();
}

function storeLocation(position) {
  lat2 = position.coords.latitude;
  lon2 = position.coords.longitude;
  localStorage.setItem("lat2", lat2);
  localStorage.setItem("lon2", lon2);
  // console.log("Stored location " + position.coords.latitude + ',' + position.coords.longitude);
  calculate();
}

function calculate() {
  var msg;
  if (lat2 || lon2) {
    if ((Math.round(lat2*divider) == Math.round(lat1*divider)) && 
        (Math.round(lon2*divider) == Math.round(lon1*divider))) {
      // console.log("Not moved yet, still at  " + lat1 + ',' + lon1);
      dist = 0;
      head = 0;
      msg = {"dist": dist,
             "head": head,
             "units": units};
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
    // console.log("Found stored point " + lat2 + ',' + lon2);
    var dLat = toRad(lat2-lat1);
    // console.log("Latitude difference (radians): " + dLat);
    var dLon = toRad(lon2-lon1);
    // console.log("Longitude difference (radians): " + dLon);
    var l1 = toRad(lat1);
    var l2 = toRad(lat2);
    // console.log("current and stored latitudes in radians: " + l1 + ',' + l2);

    var a = Math.sin(dLat/2) * Math.sin(dLat/2) +
            Math.sin(dLon/2) * Math.sin(dLon/2) * Math.cos(l1) * Math.cos(l2); 
    var c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a)); 
    dist = Math.round(R * c);
    // console.log("Calculated dist " + dist);
    
    var y = Math.sin(dLon) * Math.cos(l2);
    var x = Math.cos(l1)*Math.sin(l2) -
            Math.sin(l1)*Math.cos(l2)*Math.cos(dLon);
    head = toDeg(Math.round(Math.atan2(y, x)));
    // console.log("Calculated head " + head);
    msg = {"dist": dist,
           "head": head,
           "units": units};
    sendMessage(msg);
  }
  else {
    // console.log("Will not calculate: no lat2 and lon2: " + lat2 + ',' + lon2);
  }
}

function locationError(error) {
  console.warn('location error (' + error.code + '): ' + error.message);
}

function storeCurrentPosition() {
  // console.log("Attempting to store current position.");
  navigator.geolocation.getCurrentPosition(storeLocation, locationError, locationOptions);
}

function startWatcher() {  
  if (locationInterval) {
    clearInterval(locationInterval);
  }
  if (locationWatcher) {
    navigator.geolocation.clearWatch(locationWatcher);
  }
  if (interval > 0) {
    console.log('Interval is ' + interval + ', using getCurrentPosition and setInterval');
    locationInterval = setInterval(function() {
      navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
    }, interval * 1000);      
  }
  else {
    console.log('Interval not set, using watchPosition');
    navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
    locationWatcher = navigator.geolocation.watchPosition(locationSuccess, locationError, locationOptions);  
  }
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
function toRad(num) {
  return num * Math.PI / 180;  
}
function toDeg(num) {
  return num * 180 / Math.PI;
}
