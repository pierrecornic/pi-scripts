/*
 *    Weather Shield Example/essai
 * By: Nathan Seidle
 * SparkFun Electronics
 * Date: November 16th, 2013
 * License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).
 * 
 * Much of this is based on Mike Grusin's USB Weather Board code: https://www.sparkfun.com/products/10586
 * 
 * This is a more advanced example of how to utilize every aspect of the weather shield. See the basic
 * example if you're just getting started.
 * 
 * This code reads all the various sensors (wind speed, direction, rain gauge, humidity, pressure, light, batt_lvl)
 * and reports it over the serial comm port. This can be easily routed to a datalogger (such as OpenLog) or
 * a wireless transmitter (such as Electric Imp).
 * 
 * Measurements are reported once a second but windspeed and rain gauge are tied to interrupts that are
 * calculated at each report.
 * 
 * This example code assumes the GPS module is not used.
 * 
 * 
 * Updated by Joel Bartlett
 * 03/02/2017
 * Removed HTU21D code and replaced with Si7021
 * Rework by the G. and T. Cornic (see github/pierrecornic/pi-scripts)
 */

#include <Wire.h> //I2C needed for sensors
#include "SparkFunMPL3115A2.h" //Pressure sensor - Search "SparkFun MPL3115" and install from Library Manager
#include "SparkFun_Si7021_Breakout_Library.h" //Humidity sensor - Search "SparkFun Si7021" and install from Library Manager

MPL3115A2 myPressure; //Create an instance of the pressure sensor
Weather myHumidity;//Create an instance of the humidity sensor


/******** Hardware pin definitions ********/
// digital I/O pins
const byte WSPEED = 3;
const byte RAIN = 2;
const byte STAT1 = 7;
const byte STAT2 = 8;

// analog I/O pins
const byte REFERENCE_3V3 = A3;
const byte LIGHT = A1;
const byte BATT = A2;
const byte WDIR = A0;

/******** Global Variables ********/
long lastSecond; //The millis counter to see when a second rolls by
int seconds; //When it hits 60, increase the current minute
int seconds_2m; //Keeps track of the "wind speed/dir avg" over last 2 minutes array of data
int minutes; //Keeps track of where we are in various arrays of data
int minutes_10m; //Keeps track of where we are in wind gust/dir over last 10 minutes array of data

long lastWindCheck = 0;
volatile int anemometer_ticks = 0;

/*
 * We need to keep track of the following variables:
 * Wind speed/dir each update (no storage)
 * Wind gust/dir over the day (no storage)
 * Wind speed/dir, avg over 2 minutes (store 1 per second)
 * Wind gust/dir over last 10 minutes (store 1 per minute)
 * Rain over the past hour (store 1 per minute)
 * Total rain over date (store one per day)
 */

float windspdavg[120]; //120 bytes to keep track of 2 minute average

#define WIND_DIR_AVG_SIZE 120
int winddiravg[WIND_DIR_AVG_SIZE]; //120 ints to keep track of 2 minute average
float windgust_10m[10]; //10 floats to keep track of 10 minute max
int windgustdirection_10m[10]; //10 ints to keep track of 10 minute max

//These are all the weather values that wunderground expects:
int winddir = 0; // [0-360 instantaneous wind direction]
float windspeedmph = 0; // [mph instantaneous wind speed]
float daily_wind_max_mph = 0; // [mph current wind gust, using software specific time period]
int windgustdir = 0; // [0-360 using software specific time period]
float windspdmph_avg2m = 0; // [mph 2 minute average wind speed mph]
int winddir_avg2m = 0; // [0-360 2 minute average wind direction]
float last_10m_max_wind_mph = 0; // [mph past 10 minutes wind gust mph ]
int windgustdir_10m = 0; // [0-360 past 10 minutes wind gust direction]
float humidity = 0; // [%]
float tempf = 0; // [temperature F]
float tempc = 0; // [temperature °C]
//float baromin = 30.03;// [barom in] - It's hard to calculate baromin locally, do this in the agent
float pressure = 0;
//float dewptf; // [dewpoint F] - It's hard to calculate dewpoint locally, do this in the agent

float batt_lvl = 11.8; //[analog value from 0 to 1023]
float light_lvl = 455; //[analog value from 0 to 1023]

float last_hour_rain_inches = 0; // the accumulated rainfall in the past 60 min
// The two below can be modified by interrupts, so they must be volatile.
volatile float rain_per_minute[60]; // keep track of 60 minutes of rain
volatile float daily_rain_inches = 0; // the accumulated rainfall in the past 60 min

/********* Interrupt routines ************/
/* (these are called by the hardware interrupts, not by the main code) */

/**
 * @brief  Count the rain gauge bucket tips as they occur.
 * 
 * Activated by the magnet and reed switch in the rain gauge, attached to input D2
 */
void rain_irq_handler()
{
	// we're dealing with time, so use volatile
	volatile unsigned long time, time_interval;
	static unsigned long last_time;

	// grab current time
	time = millis();
	time_interval = time - last_time;

	// We can come here because of switch-bounce glitches, so let's force 10ms between edges.
	if (time_interval > 10) {
		daily_rain_inches += 0.011; //Each dump is 0.011" of water
		rain_per_minute[minutes] += 0.011; //Increase this minute's amount of rain

		last_time = time;
	}
}

/**
 * @brief anemometer tick interrupt
 * 
 * Activated by the magnet in the anemometer (2 ticks per rotation), attached to input D3
 */
void windspeed_irq_handler()
{
	// we're dealing with time, so use volatile
	volatile unsigned long time, time_interval;
	static unsigned long last_time;

	// grab current time
	time = millis();
	time_interval = time - last_time;

	// Ignore switch-bounce glitches less than 10ms (142MPH max reading) after the reed switch closes
	if (time_interval > 10) {
		anemometer_ticks++; // There is 1.492MPH for each click per second.
		last_time = time;
	}
}

/**
 * @brief Initial setup
 */
void setup()
{
	// Setup the serial link
	Serial.begin(9600);
	Serial.println("Setting up...");

	// Configure the pins
	pinMode(STAT1, OUTPUT); //Status LED Blue
	pinMode(STAT2, OUTPUT); //Status LED Green

	pinMode(WSPEED, INPUT_PULLUP); // input from wind meters windspeed sensor
	pinMode(RAIN, INPUT_PULLUP); // input from wind meters rain gauge sensor

	pinMode(REFERENCE_3V3, INPUT);
	pinMode(LIGHT, INPUT);

	// Configure the pressure sensor
	myPressure.begin(); // Get sensor online
	myPressure.setModeBarometer(); // Measure pressure in Pascals from 20 to 110 kPa
	myPressure.setOversampleRate(7); // Set Oversample to the recommended 128
	myPressure.enableEventFlags(); // Enable all three pressure and temp event flags

	// Configure the humidity sensor
	myHumidity.begin();

	// Initialize the time
	seconds = 0;
	lastSecond = millis();

	// Attach external interrupt pins to IRQ functions
	attachInterrupt(0, rain_irq_handler, FALLING);
	attachInterrupt(1, windspeed_irq_handler, FALLING);

	// Turn on interrupts
	interrupts();

	Serial.println("done.");
	Serial.println("Weather Shield online!");
}

void loop()
{
	float wind_speed;
	int wind_dir;

	// Keep track of which minute it is
	if(millis() - lastSecond >= 1000) {
		// Blink the status LED
		digitalWrite(STAT1, HIGH);

		lastSecond += 1000;

		// Take a speed and direction reading every second for 2 minute average
		if(++seconds_2m > 119)
			seconds_2m = 0;

		// Calc the wind speed and direction every second for 120 second to get 2 minute average
		wind_speed = get_wind_speed();
		wind_dir = get_wind_direction();
		windspdavg[seconds_2m] = wind_speed;
		winddiravg[seconds_2m] = wind_dir;
		// windspeedmph = wind_speed; // ajouté par Gildas

		// Check to see if this is a gust for the minute
		if(wind_speed > windgust_10m[minutes_10m]) {
			windgust_10m[minutes_10m] = wind_speed;
			windgustdirection_10m[minutes_10m] = wind_dir;
		}

		// Check to see if this is a gust for the day
		if(wind_speed > daily_wind_max_mph) {
			daily_wind_max_mph = wind_speed;
			windgustdir = wind_dir;
		}

		if(++seconds > 59) {
			seconds = 0;

			if(++minutes > 59) minutes = 0;
			if(++minutes_10m > 9) minutes_10m = 0;

			rain_per_minute[minutes] = 0; // Zero out this minute's rainfall amount
			windgust_10m[minutes_10m] = 0; // Zero out this minute's gust
		}

		// Report all readings every second
		print_weather();

		digitalWrite(STAT1, LOW); // Turn off the status LED
	}

	delay(100);
}

static float compute_average(float *values, int number)
{
	int i;
	float mean = 0;
	for(i = 0; i < number; i++)
		mean += values[i];

	mean /= (float)number;

	return mean;
}

/**
 * @brief Compute a circular average
 *
 * You can't just take the average. Google "mean of circular quantities" for more info.
 * We will use the Mitsuta method because it doesn't require trig functions and
 * because it sounds cool.
 * Based on: http://abelian.org/vlf/bearings.html
 * Based on: http://stackoverflow.com/questions/1813483/averaging-angles-again
 */
static int compute_circ_average(int *values, int number)
{
	long sum = winddiravg[0];
	int D = winddiravg[0];
	for(int i = 1 ; i < WIND_DIR_AVG_SIZE ; i++)
	{
		int delta = winddiravg[i] - D;

		if(delta < -180)
			D += delta + 360;
		else if(delta > 180)
			D += delta - 360;
		else
			D += delta;

		sum += D;
	}

	sum /= WIND_DIR_AVG_SIZE;
	if(sum >= 360) sum -= 360;
	if(sum < 0) sum += 360;

	return sum;
}

// Calculates each of the variables that wunderground is expecting
void calc_weather()
{
	//Calc winddir
	winddir = get_wind_direction();

	//Calc windspeed
	windspeedmph = get_wind_speed(); //This is calculated in the main loop

	//Calc daily_wind_max_mph
	//Calc windgustdir
	//These are calculated in the main loop

	// Compute the wind speed and direction average over the last 2 minutes.
	windspdmph_avg2m = compute_average(windspdavg, 120);
	winddir_avg2m = compute_circ_average(winddiravg, WIND_DIR_AVG_SIZE);

	// Calc last_10m_max_wind_mph
	// Calc windgustdir_10m
	// Find the largest windgust in the last 10 minutes
	last_10m_max_wind_mph = 0;
	windgustdir_10m = 0;

	//Step through the 10 minutes
	for(int i = 0; i < 10 ; i++)
	{
		if(windgust_10m[i] > last_10m_max_wind_mph)
		{
			last_10m_max_wind_mph = windgust_10m[i];
			windgustdir_10m = windgustdirection_10m[i];
		}
	}

	//Calc humidity
	humidity = myHumidity.getRH();
	//float temp_h = myHumidity.readTemperature();
	//Serial.print(" TempH:");
	//Serial.print(temp_h, 2);

	//Calc tempf from pressure sensor
	tempf = myPressure.readTempF();
	tempc = (tempf - 32)/1.8;
	//Serial.print(" TempP:");
	//Serial.print(tempc, 2);

	//Total rainfall for the day is calculated within the interrupt
	//Calculate amount of rainfall for the last 60 minutes
	last_hour_rain_inches = 0;
	for(int i = 0 ; i < 60 ; i++)
		last_hour_rain_inches += rain_per_minute[i];

	//Calc pressure
	pressure = myPressure.readPressure();

	//Calc dewptf

	//Calc light level
	light_lvl = get_light_level();

	//Calc battery level
	batt_lvl = get_battery_level();
}

// Returns the voltage of the light sensor based on the 3.3V rail
// This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
float get_light_level()
{
	float operatingVoltage = analogRead(REFERENCE_3V3);

	float lightSensor = analogRead(LIGHT);

	operatingVoltage = 3.3 / operatingVoltage; //The reference voltage is 3.3V

	lightSensor = operatingVoltage * lightSensor;

	return(lightSensor);
}

//Returns the voltage of the raw pin based on the 3.3V rail
//This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
//Battery level is connected to the RAW pin on Arduino and is fed through two 5% resistors:
//3.9K on the high side (R1), and 1K on the low side (R2)
float get_battery_level()
{
	float operatingVoltage = analogRead(REFERENCE_3V3);

	float rawVoltage = analogRead(BATT);

	operatingVoltage = 3.30 / operatingVoltage; //The reference voltage is 3.3V

	rawVoltage = operatingVoltage * rawVoltage; //Convert the 0 to 1023 int to actual voltage on BATT pin

	rawVoltage *= 4.90; //(3.9k+1k)/1k - multiple BATT voltage by the voltage divider to get actual system voltage

	return(rawVoltage);
}

//Returns the instataneous wind speed
float get_wind_speed()
{
	float deltaTime = millis() - lastWindCheck; //750ms

	deltaTime /= 1000.0; //Covert to seconds

	float windSpeed = (float)anemometer_ticks / deltaTime; //3 / 0.750s = 4

	anemometer_ticks = 0; //Reset and start watching for new wind
	lastWindCheck = millis();

	windSpeed *= 1.492; //4 * 1.492 = 5.968MPH

	/* Serial.println();
	   Serial.print("Windspeed:");
	   Serial.println(windSpeed);*/

	return(windSpeed);
}

//Read the wind direction sensor, return heading in degrees
int get_wind_direction()
{
	unsigned int adc;

	adc = analogRead(WDIR); // get the current reading from the sensor

	// The following table is ADC readings for the wind direction sensor output, sorted from low to high.
	// Each threshold is the midpoint between adjacent headings. The output is degrees for that ADC reading.
	// Note that these are not in compass degree order! See Weather Meters datasheet for more information.

	if (adc < 380) return (113);
	if (adc < 393) return (68);
	if (adc < 414) return (90);
	if (adc < 456) return (158);
	if (adc < 508) return (135);
	if (adc < 551) return (203);
	if (adc < 615) return (180);
	if (adc < 680) return (23);
	if (adc < 746) return (45);
	if (adc < 801) return (248);
	if (adc < 833) return (225);
	if (adc < 878) return (338);
	if (adc < 913) return (0);
	if (adc < 940) return (293);
	if (adc < 967) return (315);
	if (adc < 990) return (270);
	return (-1); // error, disconnected?
}


//Prints the various variables directly to the port
//I don't like the way this function is written but Arduino doesn't support floats under sprintf
void print_weather()
{
	calc_weather(); // Go calc all the various sensors

	Serial.println();
	Serial.print("$,winddir=");
	Serial.print(winddir);
	Serial.print(",windspeedmph=");
	Serial.print(windspeedmph, 1);
	//Serial.print(",daily_wind_max_mph=");
	//Serial.print(daily_wind_max_mph, 1);
	//Serial.print(",windgustdir=");
	//Serial.print(windgustdir);
	Serial.print(",windspdmph_avg2m=");
	Serial.print(windspdmph_avg2m, 1);
	Serial.print(",winddir_avg2m=");
	Serial.print(winddir_avg2m);
	//Serial.print(",last_10m_max_wind_mph=");
	//Serial.print(last_10m_max_wind_mph, 1);
	//Serial.print(",windgustdir_10m=");
	//Serial.print(windgustdir_10m);
	//Serial.print(",humidity=");
	//Serial.print(humidity, 1);
	Serial.print(",tempc=");
	Serial.print(tempc, 1);
	Serial.print(",last_hour_rain_inches=");
	Serial.print(last_hour_rain_inches, 2);
	Serial.print(",daily_rain_inches=");
	Serial.print(daily_rain_inches, 2);
	Serial.print(",pressure=");
	Serial.print(pressure, 2);
	//Serial.print(",batt_lvl=");
	//Serial.print(batt_lvl, 2);
	//Serial.print(",light_lvl=");
	//Serial.print(light_lvl, 2);
	Serial.print(",");
	Serial.println("#");
}
