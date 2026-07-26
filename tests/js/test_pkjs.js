"use strict";

const assert = require("assert");
const fs = require("fs");
const vm = require("vm");

let readyHandler;
let positionHandler;
let positionErrorHandler;
const sentMessages = [];

const context = {
  console: { log: function () {} },
  navigator: {
    geolocation: {
      watchPosition: function (success, failure) {
        positionHandler = success;
        positionErrorHandler = failure;
        return 1;
      },
    },
  },
  Pebble: {
    addEventListener: function (event, handler) {
      if (event === "ready") {
        readyHandler = handler;
      }
    },
    sendAppMessage: function (message, success, failure) {
      sentMessages.push({ message, success, failure });
    },
  },
};

vm.runInNewContext(fs.readFileSync("src/pkjs/index.js", "utf8"), context);
readyHandler();

positionHandler({ coords: { latitude: 1, longitude: 2, accuracy: 3 } });
positionErrorHandler({ code: 2, message: "unavailable" });
positionHandler({ coords: { latitude: 4, longitude: 5, accuracy: 6 } });
positionHandler({ coords: { latitude: 7, longitude: 8, accuracy: 9 } });
assert.strictEqual(sentMessages.length, 1);

sentMessages[0].success();
assert.strictEqual(sentMessages.length, 2);
assert.strictEqual(sentMessages[1].message.gpsError, 2);

sentMessages[1].success();
assert.strictEqual(sentMessages.length, 3);
assert.strictEqual(sentMessages[2].message.latitude, 7000000);
assert.strictEqual(sentMessages[2].message.longitude, 8000000);

positionHandler({ coords: { latitude: 10, longitude: 11, accuracy: 12 } });
sentMessages[2].failure({});
assert.strictEqual(sentMessages.length, 4);
assert.strictEqual(sentMessages[3].message.latitude, 10000000);

sentMessages[3].success();
positionErrorHandler({ code: 2, message: "unavailable" });
assert.strictEqual(sentMessages.length, 5);
assert.strictEqual(sentMessages[4].message.gpsError, 2);
