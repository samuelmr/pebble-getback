var initialized = false;
var divider = 1000000;
var lat1 = 0;
var lon1 = 0;
var lat2 = 0;
var lon2 = 0;
var head = 0;
var dist = 0;
var R = 6371000; // m

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

var locationOptions = { "timeout": 15000, "maximumAge": 60000 }; 

function locationSuccess(position) {
  console.log("Got location " + position.coords.latitude + ',' + position.coords.longitude);
  lat1 = position.coords.latitude;
  lon1 = position.coords.longitude;
  var msg = {"lat1": Math.round(lat1*divider),
             "lon1": Math.round(lon1*divider)};
  sendMessage(msg);
  calculate();
}

function calculate() {  
  if (lat2 || lon2) {
    if ((Math.round(lat2*divider) == Math.round(lat1*divider)) && 
        (Math.round(lon2*divider) == Math.round(lon1*divider))) {
      console.log("Not moved yet, still at  " + lat1 + ',' + lon1);
      dist = 0;
      head = 0;
      var msg = {"dist": dist,
                 "head": head};
      sendMessage(msg);
      return;
    }
    if (!lat2) {
      lat2 = 0;
    }
    if (!lon2) {
      lon2 = 0;
    }
    console.log("Found stored point " + lat2 + ',' + lon2);
    var dLat = (lat2-lat1).toRad();
    console.log("Here: " + dLat);
    var dLon = (lon2-lon1).toRad();
    var l1 = lat1.toRad();
    var l2 = lat2.toRad();
    console.log("Here: " + l1 + ',' + l2);

    var a = Math.sin(dLat/2) * Math.sin(dLat/2) +
            Math.sin(dLon/2) * Math.sin(dLon/2) * Math.cos(l1) * Math.cos(l2); 
    var c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a)); 
    dist = R * c;    
    msg["dist"] = dist;
    console.log("Calculated dist " + dist);
    
    var y = Math.sin(dLon) * Math.cos(lat2);
    var x = Math.cos(l1)*Math.sin(l2) -
            Math.sin(l1)*Math.cos(l2)*Math.cos(dLon);
    head = Math.atan2(y, x).toDeg();
    msg["head"] = head;
    console.log("Calculated head " + head);
    var msg = {"dist": dist,
               "head": head};
    sendMessage(msg);
  }
  else {
    console.log("no lat2 and lon2: " + lat2 + ',' + lon2);
  }
}

function locationError(error) {
  console.warn('location error (' + error.code + '): ' + error.message);
}

Pebble.addEventListener("ready", function(e) {
  console.log("JavaScript app ready and running! " + e.ready);
  locationWatcher = window.navigator.geolocation.watchPosition(locationSuccess, locationError, locationOptions);
  console.log(e.type);
  initialized = true;
});

Pebble.addEventListener("deviceorientation", function(event) {
  var msg = {"0": "orientation", "1": event.alpha, "2": event.beta, "3": event.gamma};
  sendMessage(msg);
}, true);

Pebble.addEventListener("appmessage",
  function(e) {
    console.log("Received message: " + e.payload);
    // for (var i in e.payload) {
    //   console.log(i + ': ' + e.payload[i]);
    // }
    if (e.payload["lat2"] || e.payload["lon2"]) {
      lat2 = e.payload["lat2"]/divider;
      lon2 = e.payload["lon2"]/divider;
    }
    calculate();
  }
);
