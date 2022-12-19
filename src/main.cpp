#include <Arduino.h>
#include <WiFi.h>

#define TdsSensorPin 36
#define VREF 3.3              // analog reference voltage(Volt) of the ADC
#define SCOUNT  30            // sum of sample point


// connect to wifi
const char* ssid = "FRITZ!Box 7530 WT";
const char* password = "89580715389315208415";

WiFiServer server(80);

const int RED_LED = 25;
const int GREEN_LED = 33;
const int BUZZER = 32;


int analogBuffer[SCOUNT];     // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;


const int badReading = 1;
const int goodReading = 2;
const int neutralReading = 3;


float averageVoltage = 0;
float tdsValue = 0;
float temperature = 25;       // current temperature for compensation

// median filtering algorithm
int getMedianNum(int bArray[], int iFilterLen){
  int bTab[iFilterLen];
  for (byte i = 0; i<iFilterLen; i++)
  bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0){
    bTemp = bTab[(iFilterLen - 1) / 2];
  }
  else {
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
  return bTemp;
}


void setup(){
  Serial.begin(115200);
  Serial.println("Deeznuts");
  pinMode(TdsSensorPin,INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  server.begin();
  Serial.println("Server started");
  Serial.println(WiFi.localIP());

}


void readingIndicator(int readingLED)
{
  switch (readingLED)
  {
  case badReading:
    digitalWrite(BUZZER, HIGH);
    delay(100);
    digitalWrite(BUZZER, LOW);

      for(int n = 0; n < 255; n++)
        {
          analogWrite(RED_LED, n);
          delay(5); 
        }
      
      for(int n = 255; n > 0; n--)
        {
          analogWrite(RED_LED, n);
          delay(5);
        }

    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    loop();
    return;
    break;

  case goodReading:
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER, LOW);
    analogWrite(RED_LED, 0);

    break;
  
  case neutralReading:
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER, LOW);
    analogWrite(RED_LED, 0);
    break;

  default:
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER, LOW);
    digitalWrite(GREEN_LED, LOW);
    analogWrite(RED_LED, 0);
    break;
  }
}

void loop(){
  static unsigned long analogSampleTimepoint = millis();
  if(millis()-analogSampleTimepoint > 40U){     //every 40 milliseconds,read the analog value from the ADC
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);    //read the analog value and store into the buffer
    analogBufferIndex++;
    if(analogBufferIndex == SCOUNT){ 
      analogBufferIndex = 0;
    }
  }   
  
  static unsigned long printTimepoint = millis();
  if(millis()-printTimepoint > 800U){
    printTimepoint = millis();
    for(copyIndex=0; copyIndex<SCOUNT; copyIndex++){
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
      
      // read the analog value more stable by the median filtering algorithm, and convert to voltage value
      averageVoltage = getMedianNum(analogBufferTemp,SCOUNT) * (float)VREF / 4096.0;
      
      //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0)); 
      float compensationCoefficient = 1.0+0.02*(temperature-25.0);
      //temperature compensation
      float compensationVoltage=averageVoltage/compensationCoefficient;
      
      //convert voltage value to tds value
      tdsValue=(133.42*compensationVoltage*compensationVoltage*compensationVoltage - 255.86*compensationVoltage*compensationVoltage + 857.39*compensationVoltage)*0.5;
      
      //Serial.print("voltage:");
      //Serial.print(averageVoltage,2);
      //Serial.print("V   ");
      Serial.print("TDS Value:");
      Serial.print(tdsValue,0);
      Serial.println("ppm");
    }
  }

  if (tdsValue >= 500)
  {
    readingIndicator(badReading);
  } 
  else if (tdsValue < 600 && tdsValue > 50)
  {
    readingIndicator(goodReading);
  } 
  else 
  {
    readingIndicator(neutralReading);
  } 

  WiFiClient client = server.available();
  if (client) {
    // Send the tdsValue to the client
    client.println(tdsValue);
    client.flush();
    client.stop();
  }
}
