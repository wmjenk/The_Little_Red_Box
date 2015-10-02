/******************************************************************************


    Soil Moisture Sensor ----- Photon
        GND ------------------- GND
        VCC ------------------- D5
        SIG ------------------- A1

    DS18B20 Temp Sensor ------ Photon
        VCC (Red) ------------- 3.3V (VCC)
        GND (Black) ----------- GND
        SIG (White) ----------- D4

    SI1145 UV/Infrared/Visible Light Sensor
    from Adafruit.com
        VCC ------------------- 3.3V (VCC)
        GND ------------------- GND
        SDA ------------------- D0
        SCL ------------------- D1



  Development environment specifics:
  	IDE: Particle Dev
  	Hardware Platform: Particle Photon
                       Particle Core


  Distributed as-is; no warranty is given.
*******************************************************************************/
#include "OneWire.h"
#include "spark-dallas-temperature.h"
#include "SparkFunPhant.h"
#include "Adafruit_SI1145.h"
#include "SparkFunMAX17043.h" // Include the SparkFun MAX17043 library for battery state
double voltage = 0; // Variable to keep track of LiPo voltage
double soc = 0; // Variable to keep track of LiPo state-of-charge (SOC)
int alert = 0; // Variable to keep track of whether alert has been triggered


#define ONE_WIRE_BUS D4
#define TEMPERATURE_PRECISION 11
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Dweet parameters
#define thing_name  "NameOfYourThing"//change to a unique Thing name for your device
char SERVER[] = "www.dweet.io";//where you will send your data to link with displays on Freeboard.io

#define SOIL_MOIST A1
#define SOIL_MOIST_POWER D5

Adafruit_SI1145 uv = Adafruit_SI1145();
//Run I2C Scanner to get address of DS18B20(s)
//(found in the Firmware folder in the Photon Weather Shield Repo)
/***********REPLACE THIS ADDRESS WITH YOUR ADDRESS*************/
DeviceAddress inSoilThermometer =
{0x28, 0xFC, 0xAF, 0xF1, 0x06, 0x00, 0x00, 0x62};// Enter your devices Waterproof temp sensor address using
//the I2C Address Scanner
/***********REPLACE THIS ADDRESS WITH YOUR ADDRESS*************/

//Global Variables

double InTempC = 0;//original temperature in C from DS18B20
float soiltempf = 0;//converted temperature in F from DS18B20
float UVIndex = 0;
int VisibleLight = 0;
int IRLight = 0;
float soilmoisture = 0;//used for percent soil moisture
int soilMoisture = 0;//used for integer reading from soil moisture A1
int count = 0;//This triggers a post and print on the first time through the loop


////////////PHANT STUFF//////////////////////////////////////////////////////////////////
const char server[] = "data.sparkfun.com";//go to data.sparkfun.com to learn how
//to setup your own data respository
const char publicKey[] = "your public address";
const char privateKey[] = "your private address";
Phant phant(server, publicKey, privateKey);
/////////////////////////////////////////////////////////////////////////////////////////

void update18B20Temp(DeviceAddress deviceAddress, double &tempC);//predeclare to compile

//---------------------------------------------------------------
void setup()
{
    Serial.begin(9600);   // open serial over USB
    // DS18B20 initialization
    sensors.begin();
    sensors.setResolution(inSoilThermometer, TEMPERATURE_PRECISION);

    //Begin posting data immediately instead of waiting for a key press.
    pinMode(SOIL_MOIST_POWER, OUTPUT);//power control for soil moisture
    digitalWrite(SOIL_MOIST_POWER, LOW);//Leave off by defualt


    Serial.println("SI1145 Begin");

    while(! uv.begin()) {
    Serial.println("Didn't find SI1145");
    delay(1000);
  }

  Serial.println("SI1145_OK!");

  // Set up Spark variables (voltage, soc, and alert):
	Spark.variable("voltage", &voltage, DOUBLE);
	Spark.variable("soc", &soc, DOUBLE);
	Spark.variable("alert", &alert, INT);
	// To read the values from a browser, go to:
	// http://api.particle.io/v1/devices/{DEVICE_ID}/{VARIABLE}?access_token={ACCESS_TOKEN}

	// Set up the MAX17043 LiPo fuel gauge:
	lipo.begin(); // Initialize the MAX17043 LiPo fuel gauge

	// Quick start restarts the MAX17043 in hopes of getting a more accurate
	// guess for the SOC.
	lipo.quickStart();

	// We can set an interrupt to alert when the battery SoC gets too low.
	// We can alert at anywhere between 1% - 32%:
	lipo.setThreshold(20); // Set alert threshold to 20%.

}
//---------------------------------------------------------------
void loop()
{

    getSensorData();//Get readings from all sensors

    printInfo();//print readings to serial line
    postToPhant();//upload data to Phant
    sendDweet();//upload data to Dweet.io
    delay(20000);//stay awake for 20 seconds to allow for App updates
    //Power down between sends to save power, measured in seconds.
    System.sleep(SLEEP_MODE_DEEP, 40);  //for Particle Photon 1 minute
    //(includes 20 sec update delay) between postings-change this to alter update rate
    //Spark.sleep(SLEEP_MODE_DEEP,300);   //for Spark Core
}

//---------------------------------------------------------------
void printInfo()
{
  //This function prints the sensor data out to the default Serial Port
      Serial.print("Soil_Temp F:");
      Serial.print(soiltempf);
      Serial.print("F, ");
      Serial.print("Soil_Temp C:");
      Serial.print(InTempC);
      Serial.print("C, ");
      Serial.print("UV_Index:");
      Serial.print(UVIndex);
      Serial.print(", ");
      Serial.print("Infrared Light:");
      Serial.print(IRLight);
      Serial.print(", ");
      Serial.print("Visible Light:");
      Serial.print(VisibleLight);
      Serial.print(", ");
      Serial.print("Soil_Mositure:");
      Serial.print(soilMoisture);//Mositure Content is expressed as an analog
      //value, which can range from 0 (completely dry) to the value of the
      //materials' porosity at saturation. The sensor tends to max out between
      //3000 and 3500.
      Serial.print(", ");
      Serial.print("Soil_Mositure%:");
      Serial.print(soilmoisture);
      Serial.println("%, ");
      Serial.print("Voltage: ");
	    Serial.print(voltage);  // Print the battery voltage
	    Serial.println(" V");

      Serial.print("Alert: ");
      Serial.println(alert);

      Serial.print("Percentage: ");
      Serial.print(soc); // Print the battery state of charge
      Serial.println(" %");
	    Serial.println();

}
//---------------------------------------------------------------
void getLight()
{
  Adafruit_SI1145 uv = Adafruit_SI1145();
  UVIndex = uv.readUV();
  // the index is multiplied by 100 so to get the
  // integer index, divide by 100!
  UVIndex /= 100.0;//standard UV Index number

  VisibleLight = uv.readVisible();
  IRLight = uv.readIR();
}

//---------------------------------------------------------------
void getSoilTemp()
{
    //get temp from DS18B20
    sensors.requestTemperatures();
    update18B20Temp(inSoilThermometer, InTempC);
    //Every so often there is an error that throws a -127.00, this compensates
    if(InTempC < -100)
      soiltempf = soiltempf;//push last value so data isn't out of scope
    else
      soiltempf = (InTempC * 9)/5 + 32;//else grab the newest, good data
}
//---------------------------------------------------------------
void getSoilMositure()
{
    /*We found through testing that leaving the soil moisture sensor powered
    all the time lead to corrosion of the probes. Thus, this port breaks out
    Digital Pin D5 as the power pin for the sensor, allowing the Photon to
    power the sensor, take a reading, and then disable power on the sensor,
    giving the sensor a longer lifespan.*/
    digitalWrite(SOIL_MOIST_POWER, HIGH);
    delay(200);
    soilMoisture = analogRead(SOIL_MOIST);
    soilmoisture = ((soilMoisture/3330.00)*100.00);//max analog moisture sensor reading is 3334
    //0=dry, 3330 fully immersed in tap water
    delay(200);
    digitalWrite(SOIL_MOIST_POWER, LOW);

}
//---------------------------------------------------------------
void update18B20Temp(DeviceAddress deviceAddress, double &tempC)
{
  tempC = sensors.getTempC(deviceAddress);
}
//---------------------------------------------------------------

//---------------------------------------------------------------
void getSensorData()
{

    getSoilTemp();//Read the DS18B20 waterproof temp sensor
    getSoilMositure();//Read the soil moisture sensor
    getLight();//Read UV,Visible and IR light intensity
    getBattery();//Updates battery voltage, state of charge and alert threshold



}

//---------------------------------------------------------------
void getBattery()
{
  // lipo.getVoltage() returns a voltage value (e.g. 3.93)
voltage = lipo.getVoltage();
// lipo.getSOC() returns the estimated state of charge (e.g. 79%)
soc = lipo.getSOC();
// lipo.getAlert() returns a 0 or 1 (0=alert not triggered)
alert = lipo.getAlert();

}
//---------------------------------------------------------------
void sendDweet()//sends datat to dweet.io
{
  TCPClient client;
  char response[512];
  int i = 0;
  int retVal = 0;

  Serial.println("connecting...");
  if (client.connect(SERVER, 80))
  {
  Serial.println("Connected to Dweet.io");
  Serial.println("Sending request...");
  client.print(F("GET /dweet/for/"));
    client.print(thing_name);
    client.print(F("?Soil_Temperature_F="));
    client.print(soiltempf);
    client.print(F("&Soil_Temperature_C="));
    client.print(InTempC);
    client.print(F("&Soil_Moisture_Value="));
    client.print(soilMoisture);
    client.print(F("&Soil_Moisture_Percent="));
    client.print(soilmoisture);
    client.print(F("&UV_Index="));
    client.print(UVIndex);
    client.print(F("&Infrared_Light_Value="));
    client.print(IRLight);
    client.print(F("&Visible_Light_Value="));
    client.print(VisibleLight);
    client.print(F("&Voltage="));
    client.print(voltage);
    client.print(F("&Battery_Percentage="));
    client.print(soc);
    client.print(F("&Alert="));
    client.print(alert);
    client.println(F(" HTTP/1.1"));
    client.println(F("Host: dweet.io"));
    client.println(F("Connection: close"));
    client.println(F(""));
    Serial.println(F("done."));
  } else {
    Serial.println(F("Connection failed"));
    return;
  }

}
//---------------------------------------------------------------
int postToPhant()//sends datat to data.sparkfun.com
{

    phant.add("batterypercent", soc);
    phant.add("infraredvalue", IRLight);
    phant.add("soilmoistpercent", soilmoisture);
    phant.add("soilmoistvalue", soilMoisture);
    phant.add("stempc", InTempC);
    phant.add("stempf", soiltempf);
    phant.add("uvindex", UVIndex);
    phant.add("visiblevalue", VisibleLight);


    TCPClient client;
    char response[512];
    int i = 0;
    int retVal = 0;

    if (client.connect(server, 80))
    {
        Serial.println("Posting!");
        client.print(phant.post());
        delay(1000);
        while (client.available())
        {
            char c = client.read();
            Serial.print(c);
            if (i < 512)
                response[i++] = c;
        }
        if (strstr(response, "200 OK"))
        {
            Serial.println("Post success!");
            retVal = 1;
        }
        else if (strstr(response, "400 Bad Request"))
        {
            Serial.println("Bad request");
            retVal = -1;
        }
        else
        {
            retVal = -2;
        }
    }
    else
    {
        Serial.println("connection failed");
        retVal = -3;
    }
    client.stop();
    return retVal;

}
