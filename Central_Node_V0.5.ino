/*
ESP32 Central Node V0.5
28-08-2022
Robert James Hastings
Github CountZero1066

   ###############################################################
   #                 ESP32 Central Node V0.5                     #
   #                                                             #
   #                                                             #
   #                                                             #
   ###############################################################

   Version Details:
                  -Rebuilt the serial reader/interpreter.
                  -Central Node can now relay commands to the Smart 12V Power Supply device which is a duel core ESP32 that
                   executes two seperate threads. ESPnow communication to the Smart 12V Power Supply is only possible when 
                   one of the said devices threads finishes executing its respective loop and is no longer blocking the
                   ESPnow callback function. This translates to an issue for the Central Node where its ESPnow messages to the 
                   Smart 12V Power Supply device may or may not be received. To addresss this, the Central Node will attempt to 
                   reach the destination device through ESPnow, up to a total of 10 times until it is successful in communicating
                   with the destination device, which in testing, is either succesful in the 2nd attempt or 3rd. 
*/
#include <esp_now.h>
#include <WiFi.h>
#define CHANNEL 1

esp_now_peer_info_t slave;

uint8_t broadcastAddress[] = {0x9C,0x9C,0x1F,0xC8,0xB3,0xC0};
uint8_t NewAddress[sizeof(broadcastAddress)];
String message;
String message_edit ="";
String repeat_espnow_message[1];
int failed_attempts = 0;
int successful_attempt =0;

esp_now_peer_info_t peerInfo;

//-------------------------------------------------------------------------------------------
void setup() 
{
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);// Set device as a Wi-Fi Station 
    
    if (esp_now_init() == ESP_OK) {
         Serial.println("ESPNow Init Success");
      }
        else {
          Serial.println("err,ESPNow Init Failed");   
      ESP.restart();
        }
        
        esp_now_register_recv_cb(OnDataRecv);
        esp_now_register_send_cb(OnDataSent);
}
//-------------------------------------------------------------------------------------------

bool manageSlave() {
 // Serial.println("manageslave");
    
    const esp_now_peer_info_t *peer = &slave;
    const uint8_t *peer_addr = slave.peer_addr;
    // check if the peer exists
    bool exists = esp_now_is_peer_exist(peer_addr);
    if (exists) {
      // Slave already paired.
    
      return true;
    }
    else {
      // Slave not paired, attempt pair
      esp_err_t addStatus = esp_now_add_peer(peer);
      if (addStatus == ESP_OK) {
      //  Serial.println("pair success");
        // Pair success
        return true;
      }
      else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
      //  Serial.println("ESPNOW Not Init");
        return false;
      }
      else if (addStatus == ESP_ERR_ESPNOW_ARG) {
      //  Serial.println("Invalid Argument");
        return false;
      }
      else if (addStatus == ESP_ERR_ESPNOW_FULL) {
     //   Serial.println("Peer list full");
        return false;
      }
      else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
     //   Serial.println("Out of memory");
        return false;
      }
      else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
    //   Serial.println("Peer Exists");
        return true;
      }
      else {
     //   Serial.println("Not sure what happened");
        return false;
      }
    }
  }
  
//-------------------------------------------------------------------------------------------
void initBroadcastSlave(String temp_mac) 
{   
 // Serial.println("initbroadcast");    
char mac_received[32]; 
temp_mac.toCharArray(mac_received,32);
//message_in.substring(4,21).toCharArray(mac_received,18);
byte byteMac[sizeof(broadcastAddress)];
byte index = 0;
char *token;
char *ptr;
const char colon[2] = ":";
token = strtok(mac_received, colon);

            while ( token != NULL ){
              
              byte tokenbyte = strtoul(token, &ptr, 16);
              byteMac[index] = tokenbyte;
              index++;
              token = strtok(NULL, colon);
            }
            
  memset(&slave, 0, sizeof(slave));
  
  for (int n = 0; n < 6; ++n) {
       slave.peer_addr[n] = (uint8_t)byteMac[n];
  }
  
  slave.channel = 1;
  slave.encrypt = 0;

  manageSlave();
}
//-------------------------------------------------------------------------------------------
void OnDataRecv(const uint8_t * mac,const uint8_t *incomingData, int len) 
{ 
     char* buff = (char*) incomingData;
     String buffStr = String(buff);
     buffStr.trim();
     //relay ESPnow messages to C# app
     Serial.print(buffStr);
}

//-------------------------------------------------------------------------------------------
void serial_comm_inbound()
{
  /*
  Message Types: 
                 cam = IP camera (ON/OFF)
                 per = RFID door lock permission (TRUE/FALSE)
                 spw = Smart 12V Power System
                 tst = test
                           *nde = test serial com
                           *MAC = send Central Node's MAC addressto C# app
                           *ipc = test IP camera
                           *rfd = ping RFID device (tst,rfd,48:3F:DA:44:02:4C)
                           *spw = ping Smart 12V Power System (tst,spw,AC:67:B2:05:03:9C)
                 err = send error notification
                 prt = print message contents to C# appp without additional processing 
  */

  successful_attempt = 0;
  String message;
  String sub_strings[20];
  int sub_string_count = 0; 
  
  if (Serial.available()) {
       message = Serial.readString();
       message.trim();
  while (message.length() > 0)
  {
    int index = message.indexOf(',');
    if (index == -1) // No comma found
    {
      sub_strings[sub_string_count++] = message;
      break;
    }
    else
    {
      sub_strings[sub_string_count++] = message.substring(0, index);
      message = message.substring(index+1);
    }
  }
//  for (int i = 0; i < sub_string_count; i++)
//  {
//    Serial.print(i);
//    Serial.print(": \"");
//    Serial.print(sub_strings[i]);
//    Serial.println("\"");
//  }   
          //check for input data
                  if (sub_string_count > 0) {
                  //(Message Type,Destination MAC, ON/OFF)
                  if(sub_strings[0] == "cam"){
                     initBroadcastSlave(sub_strings[1]);
                     Send_message(sub_strings[2]);
                  }
                  //(Message Type,destination MAC,T/F,UID)
                  else if(sub_strings[0] == "per"){
                     initBroadcastSlave(sub_strings[1]);
                      Send_message(sub_strings[0] + "," + sub_strings[2] + "," + sub_strings[3]);
                  }
                  //the tst message type represents a series of tests the C# app can request of networked devices
                  else if(sub_strings[0] == "tst"){
                         //check serial com between central node(this device) and C# application
                         //(tst,nde)
                          if(sub_strings[1] == "nde"){
                           Serial.print("tst,Serial Com working");
                         }
                         //C# app asks for the central node to send back its MAC address
                         //(tst,MAC)
                         else if(sub_strings[1] == "MAC"){
                           Serial.print(WiFi.macAddress());
                         }
                             //check IP cam connection
                             //(tst,ipc,FF:FF:FF:FF:FF:FF)
                            else if(sub_strings[1] == "ipc"){
                            initBroadcastSlave(sub_strings[2]);
                            Send_message(sub_strings[0] + "," + sub_strings[2]);
                          }
                            //Check if the Networked RFID door lock device is reachable
                            //(tst,rfd,FF:FF:FF:FF:FF:FF)
                            else if(sub_strings[1] == "rfd"){
                            initBroadcastSlave(sub_strings[2]);
                            Send_message(sub_strings[0]);
                           }
                           else if(sub_strings[1] == "spw"){
                            initBroadcastSlave(sub_strings[2]);
                            Send_message(sub_strings[0]);
                                repeat_espnow_message[0] = sub_strings[2];
                                repeat_espnow_message[1] = sub_strings[0];
                           }
                  }        
               }                    
              message=""; 
            }  
}
//-------------------------------------------------------------------------------------------
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) 
{   
   // Serial.print("\r\nLast Packet Send Status:\t");
   // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
     if((status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail") == "Delivery Success" || (status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail") == "Success")
     {
      failed_attempts = 0;
      successful_attempt++;
     }
     else if ((status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail") == "Delivery Fail"){
      if (failed_attempts < 10 || successful_attempt == 0){
        failed_attempts = failed_attempts + 1;
        initBroadcastSlave(repeat_espnow_message[0]);
        Send_message(repeat_espnow_message[1]);
      }
       
      
     }
     
}
//-------------------------------------------------------------------------------------------
void Send_message(String instruction)
{
   const uint8_t *peer_addr = slave.peer_addr;
   uint8_t *buff = (uint8_t*) instruction.c_str();
   size_t sizeBuff = sizeof(buff) * instruction.length();
   esp_err_t result = esp_now_send(peer_addr, buff, sizeBuff);   
   //delete me    
 //  Serial.print("Send Status: ");
        if (result == ESP_OK) {
              //delete me
        //      Serial.println("Success");
        }        
        else if (result == ESP_ERR_ESPNOW_ARG) {
        //      Serial.println("err,Invalid Argument");
        }
        else if (result == ESP_ERR_ESPNOW_INTERNAL) {
        //      Serial.println("err,Internal Error");
        }
        else if (result == ESP_ERR_ESPNOW_NO_MEM) {
       //       Serial.println("err,No Memory");
        }
        else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
       //       Serial.println("err,Peer not found.");
        }
        else {
       //       Serial.println("err,Unknown Error");
        }
}
//-------------------------------------------------------------------------------------------
void loop() 
{
    serial_comm_inbound(); 
}
//-------------------------------------------------------------------------------------------
