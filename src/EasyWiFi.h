/*
* EasyWiFi
* Based on Version 1.4.1 by John V. - 2020
* Released into the public domain on github: https://github.com/javos65/EasyWifi-for-MKR1010
* Modified by Daniel Patyk May 2023
* Version: 1.4.2 https://github/SirPytan/EasyWifi
* Editor:	http://www.visualmicro.com
*/
#ifndef EASYWIFI_h
#define EASYWIFI_h

#include "arduino.h"
#include <WiFiNINA.h>
#include <WiFiUdp.h>

// Define AccessPoint(AP) Wifi-Client parameters
#define MAX_SSID 10                          // MAX number of SSID's listed after search
#define SSID_BUFFER_SIZE 32                   // SSID name BUFFER size
#define ACCESS_POINT_CHANNEL  5                        // AP wifi channel
#define SECRET_SSID "UnKnownWireless"	    // Backup SSID - not required
#define SECRET_PASS "NoPassword"	        // Backup Pass - not required

#define ACCESS_POINT_NAME "EasyWiFi_AP"
#define MAX_CONNECT 4                        // Max number of wifi logon connects before opening AP
#define ESCAPE_CONNECT 15                    // Max number of Total wifi logon retries-connects before escaping/stopping the Wifi start

// Define UDP settings for DNS 
#define UDP_PACKET_SIZE 1024          // UDP packet size time out, preventign too large packet reads
#define DNS_HEADER_SIZE 12             // DNS Header
#define DNS_ANSWER_SIZE 16             // DNS Answer = standard set with Packet Compression
#define DNS_MAX_REQUESTS 32             // trigger first DNS requests, to redirect to own web-page
#define UDP_PORT  53                   // local port to listen for UDP packets

// Define RGB values for NINALed
#define RED 16,0,0
#define ORANGE 5,3,0
#define GREEN 0,8,0
#define BLUE 0,0,20
#define PURPLE 6,0,10
#define CYAN 0,6,10
#define BLACK 0,0,0

class EasyWiFi
{
public:
    EasyWiFi();
    void Start();
    byte Erase();
    byte SetAccessPointName(char* name);
    void SetSeed(int seed);
    void UseLED(boolean value);
    void UseAccessPoint(boolean value);
    void SetNINA_LED(char r, char g, char b);

private:
    void ListNetworks();
    void AccessPointSetup();
    void AccessPointDNSScan();
    void AccessPointWiFiClientCheck();
    void PrintWiFiStatus();
    bool IsWifiNotConnectedOrReachable(int wifiStatus);
};

#endif
