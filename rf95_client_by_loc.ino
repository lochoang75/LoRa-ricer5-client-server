#include "DHT.h"
#include <SPI.h>
#include <RH_RF95.h>
#include <string.h>

#define sendID 1 // Send ID to rf95 server
#define acceptID 2 // check ID has been accepted
#define waitingReq 3 // waitting server accept send data
#define sendData 4 // send data to server
#define finish 5 // finish the one loop

#define DHTPIN 3 // Pin data of sensor
#define DHTTYPE DHT11 // type of sensor use 

const char* ID ="6";

RH_RF95 rf95;
DHT dht(DHTPIN, DHTTYPE);
int max_time = 150;         // max time for waiting to send data 

void die(String error)
{
  Serial.println(error);
  delay(100);
}
void setup() 
{
  dht.begin();
  pinMode(8, OUTPUT);
  digitalWrite(8, HIGH);
  rf95.init();
  Serial.begin(115200);
  //while (!Serial) ; // Wait for serial port to be available
  if (!rf95.init()) die("Can't init rf95");
  rf95.setFrequency(434.0);
  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on
  //rf95.setModemConfig(RH_RF95::Bw500Cr45Sf128); 
  //rf95.setTxPower(7,false);
}

// Send ID to server
void send_ID(int& state)
{
  delay(56);        // Delay time to sent data
  //die("Start send ID to server");
  // Send a message to rf95_server
  uint8_t *id = new uint8_t;
  strcat((char*) id,ID);
  rf95.send(id, sizeof(id));
  digitalWrite(8,HIGH);
  rf95.waitPacketSent();
  //delay(100);
  //delete id;
  state++;
}

// Server accept ID from client
// Return true if received right ID from server
void accept_ID(int& state)
{
  if(rf95.waitAvailableTimeout(100)){
    uint8_t buf[strlen(ID)+2];
    char* checkSum = new char();
    checkSum ="w";
    strcat(checkSum,ID);
    uint8_t bufsize = sizeof(buf);
    
    if (rf95.recv(buf, bufsize) && !strcmp((char*)buf,checkSum))
    {
      die((char*)buf);
      die("ID has been accept. Please wait to sending your data");
      state++;
      delete checkSum;
      return;
    }
    else
    {
      state--;
      die("Not same");
      delete checkSum;
      return;
    }
  }
  else
  {
    state--;
    die("can't catch");
    return;
  }
}

// Waitting for request data from server
// Return true client has been accepted to send data
void waiting_request(int& state,char* correct_data){
  if(rf95.waitAvailableTimeout(100)){
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t bufsize = sizeof(uint8_t)*RH_RF95_MAX_MESSAGE_LEN; 
    if(rf95.recv(buf, &bufsize)) 
    {
      if(strcmp((char*)buf,correct_data) != 0) return;
      state++; // It's time to send data
      delete correct_data;
      return;
    }
    else delay(100); // reduce waitting time by 1 and countinue to waiting request from server
  }
  else
  {
    delay(100);
    return;
  }
}

void standalizedData (float tem, float hum, uint8_t* standarData, int& dataLength)
{
  // buffer to convert float to char[]
  char temBuf[8];
  char humBuf[8];

  //  Convert float to char buf
  if (tem < -100.0) dtostrf(tem, 5,2, temBuf);
  else if(tem < -10.0) dtostrf(tem, 4,2, temBuf);
  else if (tem < 0.0) dtostrf(tem, 3,2, temBuf);
  else if (tem < 10.0 && tem >= 0.0) dtostrf(tem, 3,2, temBuf);
  else if (tem < 100.0 && tem >= 10.0) dtostrf(tem, 4,2, temBuf);
  else dtostrf(tem, 5,2, temBuf);

  if (hum < 0.0) dtostrf(hum, 4,2, humBuf);
  else if (hum < 10.0 && hum >= 0.0 ) dtostrf(hum, 3,2, humBuf);
  else if (hum < 100.0) dtostrf(hum, 4,2, humBuf);
  else dtostrf(hum, 5,2, humBuf);

  //count length of data
  int counter = 1;
  standarData[0] = '*';
  for(int i = 0; i < strlen(ID); i++){
    standarData[counter++] = ID[i];
  }
  standarData[strlen(ID)+1] = '*';
  counter++;
  // Parse tempbuf to data string
  for (int i = 0; i < 8; i++) {
    if(temBuf[i] == '\0') break;
    standarData[counter++] = temBuf[i];
  }

  // add * like protocol
  standarData[counter++] = '*';

  // Parse humbuf to data string
  for (int i = 0; i < 8; i++) {
    if(humBuf[i] == '\0') break;
    standarData[counter++] = humBuf[i];
  }

  // ad * like protocol
  standarData[counter++] = '*';
  
  // end data string
  standarData[counter++] = 0;
  
  dataLength = counter;
}
void send_Data(int& state){
  // read data from DHT11
  float hum = dht.readHumidity();
  float tem = dht.readTemperature();

  // Verify data
  if (isnan(hum) || isnan(tem)) 
  {
    hum = -1.0;
    tem = -1.0;
    die("Can't read data from pin "+(String)DHTPIN);
  }
  // print out data
  Serial.print("Temperature: ");
  Serial.println(tem);
  Serial.print("Humidity: ");
  Serial.println(hum);

  uint8_t standarData[] = "" ;
  int dataLength = 0;

  standalizedData(tem, hum, standarData, dataLength);

  //print out
  Serial.println("Sending data: ");
  Serial.println((char*)standarData);

  // send data string to server
  rf95.send(standarData, dataLength);
  rf95.waitPacketSent();
  delete standarData;
  delay(100);
  Serial.println("Send done"); 
  state++;
}

void Finish(int& state){
  delay(5000);        // Depend on many of request to server
  state-=4;
}                     

void loop()
{
  static int state = 1;
  switch (state){
    case sendID:
        send_ID(state);
        break;
        
    case acceptID:
        accept_ID(state);
        break;
        
    case waitingReq:
    {
        char * correct_data = new char[strlen(ID)+3];
        correct_data = "ok";
        for(int i = 0; i < strlen(ID); i++)
        {
          correct_data[i+2] = ID[i]; 
        }
        correct_data[strlen(ID)+2] = '\0';
        delay(100);
        for(int i = 0;i < 250;i++){
          waiting_request(state,correct_data);
          if(state != 3) break;
        }
        if(state == 3) state -= 2;
        break;
    }
        
    case sendData:
        send_Data(state);
        break;
      
    case finish:
        Finish(state);
        break;
        
    default:
        break;
  }
}
