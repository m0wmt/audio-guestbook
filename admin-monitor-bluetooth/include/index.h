<!--
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-web-bluetooth/

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
-->
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Web BLE App</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="icon" type="image/png" href="">
</head>
<body>
  <h1>ESP32 Web BLE Application</h1>
  <button id="connectBleButton">Connect to BLE Device</button>
  <button id="disconnectBleButton">Disconnect BLE Device</button>
  <p>BLE state: <strong><span id="bleState" style="color:#d13a30;">Disconnected</span></strong></p>
  <h2>Fetched Value</h2>
  <p><span id="valueContainer">NaN</span></p>
  <p>Last reading: <span id="timestamp"></span></p>
  <h2>Control GPIO 2</h2>
  <button id="onButton">ON</button>
  <button id="offButton">OFF</button>
  <p>Last value sent: <span id="valueSent"></span></p>
  <p><a href="https://randomnerdtutorials.com/">Created by RandomNerdTutorials.com</a></p>
  <p><a href="https://RandomNerdTutorials.com/esp32-web-bluetooth/">Read the full project here.</a></p>
</body>
<script>
    // DOM Elements
    const connectButton = document.getElementById('connectBleButton');
    const disconnectButton = document.getElementById('disconnectBleButton');
    const onButton = document.getElementById('onButton');
    const offButton = document.getElementById('offButton');
    const retrievedValue = document.getElementById('valueContainer');
    const latestValueSent = document.getElementById('valueSent');
    const bleStateContainer = document.getElementById('bleState');
    const timestampContainer = document.getElementById('timestamp');

    //Define BLE Device Specs
    var deviceName ='ESP32';
    var bleService = 'cd4ef35f-e96f-49f1-a773-773c0aa005be';
    var ledCharacteristic = 'cd4ef35f-e96f-49f1-a773-773c0aa005be';
    var sensorCharacteristic= 'cd4ef35f-e96f-49f1-a773-773c0aa005be';

    //Global Variables to Handle Bluetooth
    var bleServer;
    var bleServiceFound;
    var sensorCharacteristicFound;

    // Connect Button (search for BLE Devices only if BLE is available)
    connectButton.addEventListener('click', (event) => {
        if (isWebBluetoothEnabled()){
            connectToDevice();
        }
    });

    // Disconnect Button
    disconnectButton.addEventListener('click', disconnectDevice);

    // Write to the ESP32 LED Characteristic
    onButton.addEventListener('click', () => writeOnCharacteristic(1));
    offButton.addEventListener('click', () => writeOnCharacteristic(0));

    // Check if BLE is available in your Browser
    function isWebBluetoothEnabled() {
        if (!navigator.bluetooth) {
            console.log("Web Bluetooth API is not available in this browser!");
            bleStateContainer.innerHTML = "Web Bluetooth API is not available in this browser!";
            return false
        }
        console.log('Web Bluetooth API supported in this browser.');
        return true
    }

    // Connect to BLE Device and Enable Notifications
    function connectToDevice(){
        console.log('Initializing Bluetooth...');
        navigator.bluetooth.requestDevice({
            filters: [{name: deviceName}],
            optionalServices: [bleService]
        })
        .then(device => {
            console.log('Device Selected:', device.name);
            bleStateContainer.innerHTML = 'Connected to device ' + device.name;
            bleStateContainer.style.color = "#24af37";
            device.addEventListener('gattservicedisconnected', onDisconnected);
            return device.gatt.connect();
        })
        .then(gattServer =>{
            bleServer = gattServer;
            console.log("Connected to GATT Server");
            return bleServer.getPrimaryService(bleService);
        })
        .then(service => {
            bleServiceFound = service;
            console.log("Service discovered:", service.uuid);
            return service.getCharacteristic(sensorCharacteristic);
        })
        .then(characteristic => {
            console.log("Characteristic discovered:", characteristic.uuid);
            sensorCharacteristicFound = characteristic;
            characteristic.addEventListener('characteristicvaluechanged', handleCharacteristicChange);
            characteristic.startNotifications();
            console.log("Notifications Started.");
            return characteristic.readValue();
        })
        .then(value => {
            console.log("Read value: ", value);
            const decodedValue = new TextDecoder().decode(value);
            console.log("Decoded value: ", decodedValue);
            retrievedValue.innerHTML = decodedValue;
        })
        .catch(error => {
            console.log('Error: ', error);
        })
    }

    function onDisconnected(event){
        console.log('Device Disconnected:', event.target.device.name);
        bleStateContainer.innerHTML = "Device disconnected";
        bleStateContainer.style.color = "#d13a30";

        connectToDevice();
    }

    function handleCharacteristicChange(event){
        const newValueReceived = new TextDecoder().decode(event.target.value);
        console.log("Characteristic value changed: ", newValueReceived);
        retrievedValue.innerHTML = newValueReceived;
        timestampContainer.innerHTML = getDateTime();
    }

    function writeOnCharacteristic(value){
        if (bleServer && bleServer.connected) {
            bleServiceFound.getCharacteristic(ledCharacteristic)
            .then(characteristic => {
                console.log("Found the LED characteristic: ", characteristic.uuid);
                const data = new Uint8Array([value]);
                return characteristic.writeValue(data);
            })
            .then(() => {
                latestValueSent.innerHTML = value;
                console.log("Value written to LEDcharacteristic:", value);
            })
            .catch(error => {
                console.error("Error writing to the LED characteristic: ", error);
            });
        } else {
            console.error ("Bluetooth is not connected. Cannot write to characteristic.")
            window.alert("Bluetooth is not connected. Cannot write to characteristic. \n Connect to BLE first!")
        }
    }

    function disconnectDevice() {
        console.log("Disconnect Device.");
        if (bleServer && bleServer.connected) {
            if (sensorCharacteristicFound) {
                sensorCharacteristicFound.stopNotifications()
                    .then(() => {
                        console.log("Notifications Stopped");
                        return bleServer.disconnect();
                    })
                    .then(() => {
                        console.log("Device Disconnected");
                        bleStateContainer.innerHTML = "Device Disconnected";
                        bleStateContainer.style.color = "#d13a30";

                    })
                    .catch(error => {
                        console.log("An error occurred:", error);
                    });
            } else {
                console.log("No characteristic found to disconnect.");
            }
        } else {
            // Throw an error if Bluetooth is not connected
            console.error("Bluetooth is not connected.");
            window.alert("Bluetooth is not connected.")
        }
    }

    function getDateTime() {
        var currentdate = new Date();
        var day = ("00" + currentdate.getDate()).slice(-2); // Convert day to string and slice
        var month = ("00" + (currentdate.getMonth() + 1)).slice(-2);
        var year = currentdate.getFullYear();
        var hours = ("00" + currentdate.getHours()).slice(-2);
        var minutes = ("00" + currentdate.getMinutes()).slice(-2);
        var seconds = ("00" + currentdate.getSeconds()).slice(-2);

        var datetime = day + "/" + month + "/" + year + " at " + hours + ":" + minutes + ":" + seconds;
        return datetime;
    }


</script>

</html>





// const char MAIN_page[] PROGMEM = R"=====(
// <!DOCTYPE HTML><html>
// <head>
//   <title>Audio Guestbook</title>
//   <meta name="viewport" content="width=device-width, initial-scale=1">
//   <style>
//     html {font-family: Arial; display: inline-block; text-align: center;}
//     p { font-size: 1.2rem;}
//     body {  margin: 0;}
//     .topnav { overflow: hidden; background-color: #50B8B4; color: white; font-size: 1rem; }
//     .content { padding: 20px; }
//     .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
//     .cards { max-width: 800px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); }
//     .reading { font-size: 1.4rem; }
//   </style>
// </head>
// <body>
//   <div class="topnav">
//     <h1>Audio Guestbook</h1>
//   </div>
//   <div class="content">
//     <div class="cards">
//       <div class="card">
//         <p style="color:rgb(10, 66, 64);">FIRMWARE</p><p><span class="reading"><span id="status">%STATUS%</span></span></p>
//       </div>
//       <div class="card">
//         <p style="color:rgb(10, 66, 64);">RECORDINGS</p><p><span class="reading"><span id="rec">%RECORDINGS%</span></span></p>
//       </div>
//       <div class="card">
//         <p style="color:rgb(10, 66, 64);">DISK SPACE</p><p><span class="reading"><span id="disk">%DISKSPACE%</span> Gb</span></p>
//       </div>
//     </div>
//   </div>
// <script>
// if (!!window.EventSource) {
//  var source = new EventSource('/events');
 
//  source.addEventListener('open', function(e) {
//   console.log("Events Connected");
//  }, false);
//  source.addEventListener('error', function(e) {
//   if (e.target.readyState != EventSource.OPEN) {
//     console.log("Events Disconnected");
//   }
//  }, false);
 
//  source.addEventListener('message', function(e) {
//   console.log("message", e.data);
//  }, false);
 
//  source.addEventListener('status', function(e) {
//   console.log("status", e.data);
//   document.getElementById("stat").innerHTML = e.data;
//  }, false);
 
//  source.addEventListener('recordings', function(e) {
//   console.log("recordings", e.data);
//   document.getElementById("rec").innerHTML = e.data;
//  }, false);
 
//  source.addEventListener('diskspace', function(e) {
//   console.log("diskspace", e.data);
//   document.getElementById("disk").innerHTML = e.data;
//  }, false);
// }
// </script>
// </body>
// </html>
// )=====";