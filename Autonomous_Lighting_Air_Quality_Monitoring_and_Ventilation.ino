#include <DHT.h>                       //DHT Sensor Header
#include <SPI.h>                       //Serial Peripheral Interface Header                      
#include <SoftwareSerial.h>            //Software serial header
#include <Wire.h>                      //Wire connectivity header
#include <ESP8266WiFi.h>               //ESP8266 headers
#include <ESP8266WebServer.h>
#include <WiFiClientSecure.h>

SoftwareSerial SIM900(18, 19);

#define DHT11_data_pin 9               //DHT11 sensor data input pin
#define DHTTYPE DHT11                  //Setting the DHT Sensor Type
const int pir_sensor = 5;              //PIR sensor data input pin
int pirState = LOW;                    // we start, assuming no motion detected
int val = 0;                           // variable for reading the pin status

int LED_pin_1 = 51;                    // System Running Indicator LED Pin
int LED_pin_2 = 53;                    // PIR Indicator LED Pin

const int relay_pin_L = 3;             // Lighting Relay Signal Pin
const int relay_pin_V = 2;             // Ventilation Relay Signal Pin
const int relay_pin_S = 4;             // Water Sprinkler Relay Signal Pin

float temparature= 0;
float humidity = 0;
float CO2 = 0;
float Smoke = 0;

unsigned long sec = 0;
unsigned long hr = 0;
unsigned long Wh = 0;
unsigned long kWh = 0;

DHT dht(DHT11_data_pin, DHTTYPE);

extern volatile unsigned long timer0_millis;
unsigned long new_value = 0;

//MQ-135 Gas Sensor (CO2)
int mq_data_pin = A0;                            //define which analog input channel you are going to use
int Rl_value = 1;                                //define the load resistance on the board, in kilo ohms
float R0_clear_air_factor = 4.5;                 //RO_CLEAR_AIR_FACTOR=(Sensor resistance in clean air)/RO,

int calibration_sample_times = 50;               //define how many samples you are going to take in the calibration phase
int calibration_sample_interval = 500;           //define the time interal(in milisecond) between each samples in the
                                                 //cablibration phase
int read_sample_interval = 50;                   //define how many samples you are going to take in normal operation
int read_sample_times = 5;                       //define the time interal(in milisecond) between each samples in normal operation

int CO2_ID = 0;
int Smoke_ID = 1;

float CO2_curve[3] = {1.6,0.18,-0.55};            //two points are taken from the curve. 
                                                  //with these two points, a line is formed which is "approximately equivalent"
                                                  //to the original curve.
                                                  //data format:{ x, y, slope}; point1: (x1, y1), point2: (x2, y2) of the corresponding curve

float Smoke_curve[3] = {1.6,0.53,-0.44};          //two points are taken from the curve. 
                                                  //with these two points, a line is formed which is "approximately equivalent" 
                                                  //to the original curve.
                                                  //data format:{ x, y, slope}; point1: (x1, y1), point2: (x2, y2) of the corresponding curve

float Ro = 10;                                    //Ro is initialized to 10 kilo ohms

const char* ssid = "ESP8266 Access Point"; // The name of the Wi-Fi network that will be created
const char* password = "thereisnospoon";   // The password required to connect to it, leave blank for an open network
const char* ssid_AP     = "SSID";         // The SSID (name) of the Wi-Fi network you want to connect to
const char* password_AP = "PASSWORD";     // The password of the Wi-Fi network

const char* host = "script.google.com";
const int https_port = 443;
String script_id = "AKfycbzasfvhBRAYSLaSNBM3BJe3zyR_8rlIMQcU6ReQLdmGKqfRK4XU";

ESP8266WebServer server(80);
WiFiClientSecure client;

void setup() 
{
  SPI.begin();

  pinMode(pir_sensor, INPUT);     // declare PIR sensor as input
  pinMode(LED_pin_1, OUTPUT);
  pinMode(LED_pin_2, OUTPUT);
  
  pinMode(relay_pin_L, OUTPUT);
  pinMode(relay_pin_V, OUTPUT);
  pinMode(relay_pin_S, OUTPUT);

  digitalWrite(LED_pin_1,HIGH);

  Serial.print("Calibrating...\n");                
  Ro = MQ_calibration(mq_data_pin);               //Calibrating the sensor. Please make sure the sensor is in clean air 
                                                  //when you perform the calibration                    
  Serial.print("Calibration is done...\n"); 
  Serial.print("Ro=");
  Serial.print(Ro);
  Serial.print("kohm");
  Serial.print("\n");

  // Arduino communicates with SIM900A GSM module at a baud rate of 19200
  SIM900.begin(19200);
  // Network synchronization time for GSM module
  delay(20000);

  Serial.begin(115200);
  delay(10);
  Serial.println('\n');

  WiFi.softAP(ssid, password);             // Start the access point
  Serial.print("Access Point \"");
  Serial.print(ssid);
  Serial.println("\" started");

  Serial.print("IP address:\t");
  Serial.println(WiFi.softAPIP());         // Send the IP address of the ESP8266 to the computer

  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);

  server.begin();
  Serial.println("HTTP server started");

  Serial.println('\n');
  
  WiFi.begin(ssid, password);             // Connect to the network
  Serial.print("Connecting to ");
  Serial.print(ssid); Serial.println(" ...");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) 
  {                                       // Wait for the Wi-Fi to connect
    delay(1000);
    Serial.print(++i); Serial.print(' ');
  }

  Serial.println('\n');
  Serial.println("Connection to the Access Point Has Established!");  
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer
}

void loop() 
{
  //TImer
  unsigned long timer_c = millis();
  Serial.print("Time: ");
  Serial.println(timer_c); //prints time since program started

  //Temperature and Humidity
  float temparature = dht.readTemperature();

  float humidity = dht.readHumidity();

  if((temparature >= 35) || (CO2 >= 450))
  {
    digitalWrite(relay_pin_V, HIGH);      //Triggering the ventilation relay on
  }
  else
  {
    digitalWrite(relay_pin_V, LOW);       //Triggering the ventilation relay off
  }
  
  //PIR Motion Detection
  val = digitalRead(pir_sensor);          // read input value
  if(val == HIGH)
  {
    digitalWrite(LED_pin_2, HIGH);        // turn LED ON
    digitalWrite(relay_pin_L, HIGH);      //Triggering the lighting relay on
    unsigned long sec = timer_c/1000;
    unsigned long hr = sec/3600;
    unsigned long Wh = 40*hr;
    unsigned long kWh = Wh/1000;
    delay(500);

    if (pirState == LOW) 
    {
      Serial.println("Motion detected!"); // print on output change
      pirState = HIGH;
    }
  }
  else
  {
    digitalWrite(LED_pin_2, LOW);         // turn LED OFF
    digitalWrite(relay_pin_L, LOW);       //Triggering the lighting relay off
    
    if (pirState == HIGH)
    {
      Serial.println("Motion ended!");    // print on output change
      pirState = LOW;
    }
  }

  float CO2 = (MQ_determine_gas_percentage(MQ_data_read(mq_data_pin)/Ro, CO2_ID) );
  float Smoke = (MQ_determine_gas_percentage(MQ_data_read(mq_data_pin)/Ro, Smoke_ID) );

  if(Smoke >= 100)
  {
    digitalWrite(relay_pin_S, HIGH);      //Triggering the water sprinkler relay on

    //Command to select SMS mode
    SIM900.print("AT+CMGF=1\r"); 
    delay(100);

    //X's are needed to be replaced with desired receiver's mobile number
    // Use international mobile number format
    SIM900.println("AT + CMGS = \"+XXXXXXXXXXXX\""); 
    delay(100);
  
    // Desired message that is to be sent
    SIM900.println("Alert!!! Smoke or Fire Detected"); 
    delay(100);

    // End AT command with a ^Z, ASCII code 26
    SIM900.println((char)26); 
    delay(100);
    SIM900.println();
    // Give module time to send SMS
    delay(5000);
  }
  else
  {
    digitalWrite(relay_pin_S, LOW);       //Triggering the water sprinkler relay off
  }

  server.handleClient();

  if(timer_c >= 10000)
  {
    Serial.println("It is 10 second");
    setMillis(new_value);
  }
}

//Make ESP8266 to work as a HTTP server (AP)
void handle_OnConnect() 
{
  float temparature;
  float humidity;
  float CO2;
  float Smoke;

  unsigned long Wh;
  unsigned long kWh;


  server.send(200, "text/html", SendHTML(temparature,humidity,CO2,Smoke,Wh,kWh)); 
}

void handle_NotFound()
{
  server.send(404, "text/plain", "Not found");
}

//Processing the HTML page which is to be shown in the ESP8266 HTTP Server (AP)
String SendHTML(float temparature,float humidity,float CO2,float Smoke,unsigned long Wh,unsigned long kWh)
{
  String ptr = "<!DOCTYPE html>";
  ptr += "<html>";

  ptr += "<head>";

  ptr += "<title>Main Page</title>";
  ptr += "<br>";

  ptr += "<h1><center>Wireless Sensor Nodes Data</center></h1>";
  ptr += "</head>";

  ptr += "<body >";

  ptr += "<br>";
  ptr += "<br>";
  ptr += "<br>";
  
  ptr += "<center>";
  ptr += "<p><b>WSN-1</b></p>";
  ptr += "<center>"; 
  ptr += "<br>";
  ptr += "<br>";
  
  ptr += "<p>";
  ptr += "<b>Temperature : ";
  ptr += temparature;
  ptr += "&deg; C</b></p>";

  ptr += "<p>";
  ptr += "<b>Humidity : ";
  ptr += humidity;
  ptr += "%</b></p>";

  ptr += "<p>";
  ptr += "<b>CO<sub>2</sub> : ";
  ptr += CO2;
  ptr += "ppm</b></p>";

  ptr += "<p>";
  ptr += "<b>Smoke : ";
  ptr += Smoke;
  ptr += "ppm</b></p>";

  ptr += "<center>";
  ptr += "<p><b>Energy Consumption of Autonomous Lighting</b></p>";
  ptr += "<center>";
  ptr += "<br>";
  ptr += "<br>";

  ptr += "<p>";
  ptr += "<b>Wh : ";
  ptr += Wh;
  ptr += "</b></p>";

  ptr += "<p>";
  ptr += "<b>kWh : ";
  ptr += kWh;
  ptr += "</b></p>";
  
  ptr += "</center>";


  ptr += "</body>";

  ptr += "</html>";
  return ptr;
}

//Transitting data to the cloud after connecting with the nearby Wi-Fi network through an Access Point
void sendData(float temparature,float humidity,float CO2,float Smoke,unsigned long Wh,unsigned long kWh, int val3)
{
  int a1=val3;
  Serial.print("Connecting to ");
  Serial.println(host);
  if (!client.connect(host, https_port)) 
  {                                                                //Connecting to the IP address and port specified
    Serial.println("Connection Failed");
    a1=1;
  } 
  
  String url = "/macros/s/" + script_id + "/exec?output1=" + temparature + "&output2=" + humidity + "&output3=" + CO2 + "&output4=" + Smoke + "&output5=" + Wh + "&output6=" + kWh;
  Serial.print("Requesting URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: BuildFailureDetectorESP8266\r\n" +
               "Connection: close\r\n\r\n");                            //Printing data to the cloud server with which the client is connected to

  if(a1==0)
  {
    Serial.println("Connection Request is Sent");
  } 
  else 
  {
     Serial.println("Connection Request Failed");
  }
  while (client.connected()) 
  {
    String line = client.readStringUntil('\n');                           //Stream to string untill described character comes
    if (line == "\r") 
    {
      Serial.println("Headers Successfully Received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  //Serial.println(line);
  if (line.startsWith("{\"state\":\"success\"")) 
  {
    Serial.println("ESP8266 to cloud connection successfull");
  } 
  else 
  {
    Serial.println("ESP8266 to cloud connection has failed");
  }

  Serial.println("Terminating Connection");
}

//MQ Resistance Calculation
  float MQ_resistance_calculation(int raw_adc)
  {
    return ( ((float)Rl_value*(1023-raw_adc)/raw_adc));
  }

  //MQ Calibration
  float MQ_calibration(int mq_data_pin)
  {
    int i;
    float val = 0;

    for (i = 0; i<calibration_sample_times; i++) 
    {            //take multiple samples
      val += MQ_resistance_calculation(analogRead(mq_data_pin));
      delay(calibration_sample_interval);
    }
    val = val/calibration_sample_times;                   //calculate the average value

    val = val/R0_clear_air_factor;                        //divided by RO_CLEAN_AIR_FACTOR yields the Ro 
                                                        //according to the chart in the datasheet 

    return val; 
   }

  //MQ Read
  float MQ_data_read(int mq_data_pin)
  {
    int i;
    float rs = 0;

    for (i = 0; i<read_sample_times; i++) 
    {
      rs += MQ_resistance_calculation(analogRead(mq_data_pin));
      delay(read_sample_interval);
    }

    rs = rs/read_sample_times;

    return rs;  
  }

  // MQ Get Gas Percentage
  float MQ_determine_gas_percentage(float rs_ro_ratio, int gas_id)
  {
    if ( gas_id == CO2_ID ) 
    {
      return MQ_determine_percentage(rs_ro_ratio, CO2_curve);
    } 
    else if ( gas_id == Smoke_ID ) 
    {
      return MQ_determine_percentage(rs_ro_ratio, Smoke_curve);
    }
    else
    {
      return 0;
    }

    return 0;
  }

  //MQ Get Percentage 

  float  MQ_determine_percentage(float rs_ro_ratio, float *pcurve)
  {
    return (pow(10,( ((log(rs_ro_ratio)-pcurve[1])/pcurve[2]) + pcurve[0])));
  }

void setMillis(unsigned long new_millis)
{
  uint8_t SREG;
  uint8_t oldSREG = SREG;
  cli();
  timer0_millis = new_millis;
  SREG = oldSREG;
}
