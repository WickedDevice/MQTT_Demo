/***************************************************
 * 
 * This is an MQTT publish example for the Wildfire
 * 
 * 
 * 
 * This script uses some of the methods from the buildtest example for CC3000 that Limor Fried, Kevin 
 * 
 * Townsend and Phil Burgess 
 * 
 * wrote for Adafruit Industries to make debugging easier.
 * 
 ****************************************************/
#include <Client.h>
#include <WildFire_CC3000.h>
#include <SPI.h>
#include <utility/debug.h>
#include <utility/socket.h>
#include <WildFire.h>
#include <PubSubClient.h>
#define WLAN_SSID "linksys_almond" // SSID
#define WLAN_PASS "6YEKFGE6YC" // Passcode
#define BROKER "test.mosquitto.org" // The address of the broker
#define DEFAULT_DEV_ID "wildfireX0001"
#define STATUS "device/" DEFAULT_DEV_ID "/status"
#define CMDS "device/" DEFAULT_DEV_ID "/cmds"
#define SENSOR_DATA "device/" DEFAULT_DEV_ID "/sensors/data"
#define LED_PIN 6

// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY WLAN_SEC_WPA2

// Instantiates the wildfire object

WildFire wf;
WildFire_CC3000 cc3000;
WildFire_CC3000_Client client;
PubSubClient *mqttclient;

uint32_t serverIP = 0;
uint16_t k = 0;

void callback (char* topic, byte* payload, unsigned int length) {
  String msg;
  msg = "";
  for(uint8_t i = 0; i < length; i++){
    msg += (char)payload[i];
  }

  if(msg.equals("on")){
    digitalWrite(LED_PIN, HIGH);
    mqttclient->publish(STATUS, stringToByteArr("on"), 2, true);
  }
  else if(msg.equals("off") ){
    digitalWrite(LED_PIN, LOW);
    mqttclient->publish(STATUS, stringToByteArr("off"), 3, true);
  }
  else{
    Serial.println("Didn't match");
  }
}

void blinkLED(){
  digitalWrite(13, 1);
  digitalWrite(13, 0);
}

void connectToAP(const char* ssid, const char* psswd, uint8_t security, boolean newProfile){
  Serial.println(F("\nInitialising the CC3000 ..."));
  if (!cc3000.begin()) {
    Serial.println(F("Unable to initialise the CC3000!"));
    pinMode(13, OUTPUT);

    while(1){
      blinkLED();
    }
  }

  if(newProfile){
    if (!cc3000.deleteProfiles()) {
      Serial.println(F("Failed to delete profiles"));
      while(1);
    }
  }

  Serial.println(F("Sucessful Initialization"));
  displayMACAddress();
  Serial.print(F("\nAttempting to connect to "));
  Serial.println(WLAN_SSID);

  if (!cc3000.connectToAP(ssid, psswd, security)) {
    Serial.println(F("Failed!"));
    while(1);
  }

  Serial.println(F("Connected!"));

  // Wait for DHCP to complete
  Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP()) {
    delay(100); // ToDo: Insert a DHCP timeout!
  }

  // Display the IP address DNS, Gateway, etc.
  while (!displayConnectionDetails()) {
    delay(1000);
  }
}

void connectToBroker(char* addr, int port){
  uint8_t server_ip[4] = {0};
  
  // Resolves the broker by name
  Serial.print("Resolving Broker Address:");
  Serial.println(addr);

  // Resolve the IP address of the broker website
  while(serverIP == 0){
    if(!cc3000.getHostByName(addr, &serverIP)){
      Serial.println(F("Could not resolve website"));
    }
    delay(500);
  }

  Serial.println(serverIP);
  server_ip[0] = (uint8_t)(serverIP >> 24);
  server_ip[1] = (uint8_t)(serverIP >> 16);
  server_ip[2] = (uint8_t)(serverIP >> 8);
  server_ip[3] = (uint8_t)(serverIP);  
  
  Serial.println("Creating new MQTT client");
  // Create the new PubSubClient
  mqttclient = new PubSubClient(server_ip, port, callback, (Client&) client);

  // Attempts to connect to the broker five times before giving up
  Serial.println("Attempting connection to the broker");

  int i = 0;
  while(!client.connected() && i <= 5) {
    i++;
    Serial.print("Try# ");
    Serial.println(i);
    client = cc3000.connectTCP(serverIP, 1883);
    delay(500);
  }

  // If the connection succeded
  if(client.connected()) {
    Serial.println("Connected to the Server");

    if (mqttclient->connect(DEFAULT_DEV_ID)) {
      String payl = DEFAULT_DEV_ID;
      payl += " is now online";
      mqttclient->publish(STATUS, stringToByteArr(payl), strlen(STATUS), true);
      Serial.println("Published to Topic");
      mqttclient->subscribe(CMDS);
    }
  }
}

uint8_t* stringToByteArr(String msg){
  uint16_t len = msg.length();
  uint8_t *data = new uint8_t[len];
  for(int i = 0; i < len; i++){
    data[i] = msg[i];
  }

  return data;
}

void reconnect(){
  String msg = "Client was disconnected";
  Serial.println("Client was disconnected");
  mqttclient->connect(DEFAULT_DEV_ID);
  mqttclient->publish(STATUS, stringToByteArr(msg), msg.length());
}

void setup(void)
{
  wf.begin();
  Serial.begin(115200);
  Serial.println("Connect to Access Point? (y/n)");
  while(!Serial.available());
  if(Serial.read() == 'y'){
    connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY, false);
  }

  while(Serial.available()){ 
    Serial.println(Serial.read());
  }

  Serial.println("Connect to Broker? (y/n)");
  while(!Serial.available()){
    delay(500);
  }

  Serial.println("Serial is available");
  if(Serial.read() == 'y'){
    Serial.println("Connecting to broker");
    connectToBroker(BROKER, 1883);
  }
}

void loop(void) {
  String message;
  if (!mqttclient->connected() || !mqttclient->loop()) {
    reconnect();
  }

  delay(1000);
}

/**************************************************************************/
/*!
 @brief Displays the driver mode (tiny of normal), and the buffer
 size if tiny mode is not being used
 @note The buffer size and driver mode are defined in cc3000_common.h
 */
/**************************************************************************/

void displayDriverMode(void)
{
#ifdef CC3000_TINY_DRIVER
  Serial.println(F("CC3000 is configure in 'Tiny' mode"));
#else

  Serial.print(F("RX Buffer : "));
  Serial.print(CC3000_RX_BUFFER_SIZE);
  Serial.println(F(" bytes"));
  Serial.print(F("TX Buffer : "));
  Serial.print(CC3000_TX_BUFFER_SIZE);
  Serial.println(F(" bytes"));
#endif
}

/**************************************************************************/
/*!
 @brief Tries to read the CC3000's internal firmware patch ID
 */
/**************************************************************************/
uint16_t checkFirmwareVersion(void)
{
  uint8_t major, minor;
  uint16_t version;
  
#ifndef CC3000_TINY_DRIVER 
  if(!cc3000.getFirmwareVersion(&major, &minor))
  {
    Serial.println(F("Unable to retrieve the firmware version!\r\n"));
    version = 0;
  }
  else
  {
    Serial.print(F("Firmware V. : "));
    Serial.print(major); 
    Serial.print(F(".")); 
    Serial.println(minor);
    version = major; 
    version <<= 8; 
    version |= minor;
  }
#endif

  return version;
}

/**************************************************************************/
/*! 
 @brief Tries to read the 6-byte MAC address of the CC3000 module 
 */
/**************************************************************************/
void displayMACAddress(void)
{
  uint8_t macAddress[6];
  if(!cc3000.getMacAddress(macAddress))
  {
    Serial.println(F("Unable to retrieve MAC Address!\r\n"));
  }
  else
  {
    Serial.print(F("MAC Address : "));
    cc3000.printHex((byte*)&macAddress, 6);
  }
}

/**************************************************************************/
/*! 
 @brief Tries to read the IP address and other connection details 
 */
/**************************************************************************/
bool displayConnectionDetails(void)
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv))
  {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  }
  else
  {
    Serial.print(F("\nIP Addr: ")); 
    cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); 
    cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); 
    cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); 
    cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); 
    cc3000.printIPdotsRev(dnsserv);
    Serial.println();
    return true;
  }
}

