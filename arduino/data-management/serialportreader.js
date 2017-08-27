//programme qui lit les données du port série et le affiche dans la console
var serialport = require("serialport");  // chargement de la librairie serial port dans la variable serialport
var SerialPort = serialport.SerialPort;
var sp = new SerialPort("/dev/ttyACM0", {  parser: serialport.parsers.readline("\n")});
sp.on("data", function (data) {  
    console.log(data);
});
