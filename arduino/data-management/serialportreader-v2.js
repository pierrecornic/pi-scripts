#!/usr/bin/env node

function isEmpty(obj) {
    return Object.keys(obj).length === 0;
}

//programme qui lit les données du port série et le affiche dans la console
var serialport = require("serialport");  // chargement de la librairie serial port dans la variable serialport
var SerialPort = serialport.SerialPort;
var sp = new SerialPort("/dev/ttyACM0", {  parser: serialport.parsers.readline("\n")});
var windCorrection = 270;
var fs = require('fs');
sp.on("data", function (data) {
	if (data.length == 0) {
		return;
	}
	var dataArray	= data.split(",");
	var objectResult = {};
	var results = dataArray.map(function (i) {
                var item = i.split('=');
		var key = item[0];
		switch (key) {
			case 'winddir':
				objectResult.wind_direction=(parseInt(item[1])+windCorrection)%360;
				break;
			case 'windspeedmph':
				objectResult.wind_speed_knts = item[1] * 0.868976;
				break;
			case 'windgustmph':
				objectResult.wind_gust_speed_knts = item[1] * 0.868976;
				break;
			case 'windgustdir':
				objectResult.wind_gust_direction = (parseInt(item[1])+windCorrection)%360;
				break;
			case 'windspdmph_avg2m':
				objectResult.wind_speed_knts_2min = item[1] * 0.868976;
				break;
			case 'winddir_avg2m':
				objectResult.wind_direction_2min = (parseInt(item[1])+windCorrection)%360;
				break;
			case 'windgustmph_10m':
				objectResult.wind_gust_speed_knts_10min = item[1] * 0.868976;
				break;
			case 'windgustdir_10m':
				objectResult.wind_gust_direction_10min = (parseInt(item[1])+windCorrection)%360;
				break;
			case 'humidity':
				objectResult.humidity = parseFloat(item[1]);
				break;
			case 'tempc':
				objectResult.temperature = parseFloat(item[1]);
				break;
			case 'rainin':
				objectResult.rain_inch = parseFloat(item[1]);
				break;
			case 'dailyrainin':
				objectResult.rain_inch_daily = parseFloat(item[1]);
				break;
			case 'pressure':
				objectResult.pressure= parseFloat(item[1]);
				break;
			default:
			break;
		}
        });
	if (!isEmpty(objectResult)) {
		objectResult.timestamp  = new Date();
		fs.appendFileSync('data.log', JSON.stringify(objectResult)+"\n");
	}
});
