var locationWatchId = null;
var messageInFlight = false;
var pendingPosition = null;
var pendingError = null;

Pebble.addEventListener("ready",
  function (e) {
    locationWatchId = navigator.geolocation.watchPosition(
      function (pos) {
        queuePosition({
          "latitude": Math.round(pos.coords.latitude * 1e6),
          "longitude": Math.round(pos.coords.longitude * 1e6),
          "accuracy": Math.round(pos.coords.accuracy * 10),
        });
      },
      function (err) {
        console.log("GPS error: " + err.message);
        queueError({ "gpsError": err.code || 1 });
      },
      { enableHighAccuracy: true, maximumAge: 0, timeout: 5000 }
    );
  }
);


function queuePosition(message) {
  pendingPosition = message;
  sendPendingMessage();
}

function queueError(message) {
  pendingError = message;
  sendPendingMessage();
}

function sendPendingMessage() {
  if (messageInFlight || (pendingError === null && pendingPosition === null)) {
    return;
  }
  var message;
  if (pendingError !== null) {
    message = pendingError;
    pendingError = null;
  } else {
    message = pendingPosition;
    pendingPosition = null;
  }
  messageInFlight = true;
  Pebble.sendAppMessage(
    message,
    function () {
      messageInFlight = false;
      sendPendingMessage();
    },
    function (error) {
      var message = error && error.error && error.error.message
        ? error.error.message
        : "unknown error";
      console.log("AppMessage error: " + message);
      messageInFlight = false;
      sendPendingMessage();
    }
  );
}
