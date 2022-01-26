/*
ESP32 Central Node V0.3
25-01-2022
Robert James Hastings
117757785@umail.ucc.ie

   ###############################################################
   #                 ESP32 Central Node V0.3                     #
   #                                                             #
   #      Intended for use with the ESPIRIFF Systems ESP32.      #
   #      Facilitates communication between a C# application     #
   #      and a loose network of connected ESP8266s and          #
   #      ESP32-cams. Communication between this device and      # 
   #      the ESP8266/ESP32-cam units is conducted via the       #
   #      ESPnow protocol while communication between this       #
   #      device and the computer running the c# application     #
   #      is conducted via serial communication over UART.       #
   #      This device will process and interept communiques      #
   #      received via serial and relay messages to the          #
   #      appropriate devices, whilst relaying messages received #
   #      via ESPnow straight to serial.                         #
   #                                                             #
   ###############################################################
*/
#include <esp_now.h>
#include <WiFi.h>
#define CHANNEL 1

esp_now_peer_info_t slave;

uint8_t broadcastAddress[] = {0x9C,0x9C,0x1F,0xC8,0xB3,0xC0};
uint8_t NewAddress[sizeof(broadcastAddress)];
String message;
String message_edit ="";
//delete me
String string_address = "AC:67:B2:05:03:9C";
//delete me
String sample_in = "cam,AC:67:B2:05:03:9C,ON";

esp_now_peer_info_t peerInfo;

//-------------------------------------------------------------------------------------------
void setup() 
{
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);// Set device as a Wi-Fi Station
    InitESPNow();  
    esp_now_register_recv_cb(OnDataRecv);
    esp_now_register_send_cb(OnDataSent);
}
//-------------------------------------------------------------------------------------------
void InitESPNow() 
{
  if (esp_now_init() == ESP_OK) {
      Serial.println("ESPNow Init Success");
  }
  else {
      Serial.println("err,ESPNow Init Failed");   
      ESP.restart();
  }
}
//-------------------------------------------------------------------------------------------
void initBroadcastSlave(String message_in) 
{       
char mac_received[18]; 
message_in.substring(0,17).toCharArray(mac_received,18);
byte byteMac[sizeof(broadcastAddress)];
byte index = 0;
uint8_t NewAddress[sizeof(broadcastAddress)];
char *token;
char *ptr;
const char colon[2] = ":";
token = strtok(mac_received, colon);

            while ( token != NULL ){
              Serial.print("Token: '");
              Serial.print(token);
              Serial.println("'");
              byte tokenbyte = strtoul(token, &ptr, 16);
              byteMac[index] = tokenbyte;
              Serial.println(tokenbyte, HEX);
              index++;
              token = strtok(NULL, colon);
            }
            
  memset(&slave, 0, sizeof(slave));
  
  for (int n = 0; n < 6; ++n) {
       slave.peer_addr[n] = (uint8_t)byteMac[n];
  }
  
  slave.channel = 1;
  slave.encrypt = 0;
}
//-------------------------------------------------------------------------------------------
void OnDataRecv(const uint8_t * mac,const uint8_t *incomingData, int len) 
{ 
     char* buff = (char*) incomingData;
     String buffStr = String(buff);
     buffStr.trim();
     Serial.print(buffStr);
}
//-------------------------------------------------------------------------------------------
void serial_comm_inbound()
{
  /*
  Message Types: 
                 cam = IP camera (ON/OFF)
                 tst = test
                           *nde = test serial com
                           *ipc = test IP camera
                 err = send error notification
                 prt = print message contents to C# appp without additional processing 
  */
  delay(1); 
  if (Serial.available()) {
    
            while (message.length() < 1) {
                message = Serial.readString();
                
          //check for input data
                  if (message.length() > 1) {
                  message_edit = message;
                  message_edit.trim();      
                  //Serial.println(message_edit);
                  //Check first 3 characters of inbound serial message for the message type
                  //and decide on actions taken based on message type
                  String msg_type = message_edit.substring(0,3);
            
                        //Turn on or off IP camera
                        if(msg_type.equals("cam")){
                              String camera_instruction = message_edit.substring(22,24);         
                              initBroadcastSlave(message_edit);
                              Send_message(camera_instruction);              
                            }        
                          //check connected devices status
                           else if (msg_type.equals("tst")){ 
                                   //check serial com between central node(this device) and C# application
                                   //(tst,nde)
                                    if (message_edit.substring(4,7).equals("nde")){
                                      Serial.print("tst,Serial Com working");
                                    }
                                   //check IP cam connection
                                   //(tst,ipc,FF:FF:FF:FF:FF:FF)
                                  else if(message_edit.substring(4,7).equals("ipc")){
                                    initBroadcastSlave(message_edit.substring(4,23));
                                    Send_message(message_edit.substring(0,3));
                                  }
                                              
                           }
                    }                    
              } 
  }
  message="";  
}
//-------------------------------------------------------------------------------------------
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) 
{   
     Serial.print("\r\nLast Packet Send Status:\t");
     Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
//-------------------------------------------------------------------------------------------
void Send_message(String instruction)
{
   const uint8_t *peer_addr = slave.peer_addr;
   uint8_t *buff = (uint8_t*) instruction.c_str();
   size_t sizeBuff = sizeof(buff) * instruction.length();
   esp_err_t result = esp_now_send(peer_addr, buff, sizeBuff);   
   //delete me    
   Serial.print("Send Status: ");
        if (result == ESP_OK) {
              //delete me
              Serial.println("Success");
        }        
        else if (result == ESP_ERR_ESPNOW_ARG) {
              Serial.println("err,Invalid Argument");
        }
        else if (result == ESP_ERR_ESPNOW_INTERNAL) {
              Serial.println("err,Internal Error");
        }
        else if (result == ESP_ERR_ESPNOW_NO_MEM) {
              Serial.println("err,No Memory");
        }
        else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
              Serial.println("err,Peer not found.");
        }
        else {
              Serial.println("err,Unknown Error");
        }
}
//-------------------------------------------------------------------------------------------
void loop() 
{
    serial_comm_inbound();
    delay(1);
}
//-------------------------------------------------------------------------------------------
