/*
* EasyWiFi
* Modified by Daniel Patyk May 2023 based on John V. Version 1.4.1
* Version: 1.4.2 https://github/SirPytan/EasyWifi
* 
*  RGB LED INDICATOR on uBlox nina Module
*  GREEN: Connected
*
*  BLUE: (Stored) Credentials found, connecting					 <<<<--_
*  YELLOW: No Stored Credentials found, connecting					     \
*  PURPLE: Can'i connect, opening Access Point for credentials input      |
*  CYAN: Client connected to Access Point, wait for credentials input >>--/
*
*  RED: Not connected / Can'i connect, wifi.start is stopped, return to program
*
* Released into the public domain on github: https://github.com/javos65/EasyWifi-for-MKR1010
*/

#include "EasyWiFi.h"
#include "CredentialsHandler.h"

#define Debug_On       // Debug option  -serial print
//#define Debug_On_X   // Debug option - incl packets

char G_AccessPointName[SSID_BUFFER_SIZE] = ACCESS_POINT_NAME; // ACCESS POINT name, dynamic adaptable
char G_SSID_List[MAX_SSID][SSID_BUFFER_SIZE];				// Store of available SSID's
int G_AP_Status = WL_IDLE_STATUS, G_AP_InputFlag;  // global AP flag to use
int G_SSID_Counter = 0;                           // Gloabl counter for number of found SSID's
char G_SSID[32] = SECRET_SSID;                   // optional init: your network SSID (name) 
char G_PASS[32] = SECRET_PASS;                   // optional init: your network password 
WiFiServer G_AP_Webserver(80);                    // Global Acces Point Web Server
WiFiUDP G_UDP_AP_DNS;                            // A UDP instance to let us send and receive packets over UDP
IPAddress G_AP_IP;                                // Global Acces Point IP adress 
IPAddress G_AP_DNS_CLIENT_IP;
int G_DNS_ClientPort;
int G_DNS_RequestCounter = 0;
boolean G_UseAP = 1; // use AP after loging failure, or quit with no AP service
boolean G_LED_On = 1; // leds on or of
byte G_UDP_PacketBuffer[UDP_PACKET_SIZE];  // buffer to hold incoming and outgoing packets
byte G_DNS_ReplyHeader[DNS_HEADER_SIZE] = {
  0x00,0x00,   // ID, to be filled in #offset 0
  0x81,0x80,   // answer header Codes
  0x00,0x01,   //QDCOUNT = 1 question
  0x00,0x01,   //ANCOUNT = 1 answer
  0x00,0x00,   //NSCOUNT / ignore
  0x00,0x00    //ARCOUNT / ignore
};
byte G_DNS_ReplyAnswer[DNS_ANSWER_SIZE] = {
  0xc0,0x0c,  // pointer to pos 12 : NAME Labels
  0x00,0x01,  // TYPE
  0x00,0x01,  // CLASS
  0x00,0x00,  // TTL
  0x18,0x4c,  // TLL 2 days
  0x00,0x04,  // RDLENGTH = 4
  0x00,0x00,  // IP adress octets to be filled #offset 12
  0x00,0x00   // IP adress octeds to be filled
};

// ***************************************


EasyWiFi::EasyWiFi() {}

// Login to local network  //
void EasyWiFi::Start()
{
	// Early exit if already connected
	bool alreadyConnected = !IsWifiNotConnectedOrReachable(WiFi.status());
	if (alreadyConnected)
	{
		SetNINA_LED(GREEN); // Set Green  
		#ifdef Debug_On
			Serial.println("* Already connected."); // you're already connected
			PrintWiFiStatus();
		#endif
		return;
	}

	// Read saved credentials from file
	const byte READ_FAILED = 0;
	if (CredentialsHandler::Read_Credentials(G_SSID, G_PASS) == READ_FAILED) // if no success use hardcoded credentials
	{
		SetNINA_LED(ORANGE); // no credentials found SET ORANGE
		#ifdef Debug_On
			Serial.println("* Using hardcoded credentials");
		#endif
	}

	// Start while loop for finding a connection 
	SetNINA_LED(BLUE); // Starting to connect: Set Blue  
	int totalConnectionAttempts = 0;
	while (IsWifiNotConnectedOrReachable(WiFi.status())) 
	{
		// Attempt to connect to WiFi network:
		//totalConnectionAttempts += TryToConnectToWifiWithCredentials();   // count total failed connects     

		// If connected, exit while loop
		if (WiFi.status() == WL_CONNECTED)
		{
			SetNINA_LED(GREEN); // Set Green   
			#ifdef Debug_On
				PrintWiFiStatus(); // you're connected now, so print out the status and break while loop
			#endif
			break;
		}
		
		if ((totalConnectionAttempts > ESCAPE_CONNECT) || (G_UseAP == false)) // quite login service?
		{
			SetNINA_LED(RED); // Set red 
			#ifdef Debug_On
				Serial.println("* Connection not possible after too many retries, quit wifi.start process");
			#endif
			break;
		}

		// No connection possible opening Access Point		
		#ifdef Debug_On
			Serial.println("* Connection not possible after several retries, opening Access Point");
		#endif

		// start direct-Wifi connect to manualy input Wifi credentials
		SetNINA_LED(RED); // no network, : RED
		ListNetworks();   // load avaialble networks in a list
		AccessPointSetup();
		SetNINA_LED(PURPLE); // start AP, : Purple

		G_AP_InputFlag = 0;
		while (!G_AP_InputFlag) // Keep AP open until input is received or until 30 seconds are over
		{
			UpdateDeviceConnectedStatus();

			if (G_AP_Status == WL_AP_CONNECTED)  // IF client connected to AP, start DNS and check Webserver
			{
				AccessPointDNSScan();          // check DNS requests
				AccessPointWiFiClientCheck_Test();  // check HTTP server Client
			}
		}
		G_UDP_AP_DNS.stop(); // Close UDP connection
		WiFi.end();
		WiFi.disconnect();
		SetNINA_LED(BLUE); // new credentials : BLUE
		delay(2000);
		
	} //while loop until connected

}



// Erase credentials from disk file
byte EasyWiFi::Erase()
{
	return CredentialsHandler::Erase_Credentials();
}

// Set Name of AccessPoint
byte EasyWiFi::SetAccessPointName(char* name)
{
	int i = 0;
	while (name[i] != 0)
	{
		G_AccessPointName[i] = name[i];
		i++;
		if (i >= SSID_BUFFER_SIZE)
			break;
	}
	G_AccessPointName[i] = 0; // close string
	return i;
}

// Set Seed of the Cypher, should be positive
void EasyWiFi::SetSeed(int seed)
{
	CredentialsHandler::SetSeed(seed);
}

/* Set Led indicator active on or off - for low power usage*/
void EasyWiFi::UseLED(boolean value)
{
	G_LED_On = value;
}

/* Set AP or no AP service*/
void EasyWiFi::UseAccessPoint(boolean value)
{
	G_UseAP = value;
}

/* Set RGB led on uBlox Module R-G-B , max 128*/
void EasyWiFi::SetNINA_LED(char r, char g, char b)
{
	if (G_LED_On)
	{
		// Set LED pin modes to output
		WiFiDrv::pinMode(25, OUTPUT);
		WiFiDrv::pinMode(26, OUTPUT);
		WiFiDrv::pinMode(27, OUTPUT);

		// Set all LED color 
		WiFiDrv::analogWrite(25, g % 128);    // GREEN
		WiFiDrv::analogWrite(26, r % 128);    // RED
		WiFiDrv::analogWrite(27, b % 128);    // BLUE
	}
}

// Scan for available Wifi Networks and place is Glovbal SSIDList
void EasyWiFi::ListNetworks()
{
	// scan for nearby networks:
	int foundNetworksAmount = WiFi.scanNetworks();
	if (foundNetworksAmount == -1)
	{
		#ifdef Debug_On        
			Serial.println("* Couldn't get a Wifi List");
		#endif
	}
	else
	{
		#ifdef Debug_On    
			Serial.print("* Found total "); Serial.print(foundNetworksAmount); Serial.println(" Networks.");
		#endif      
		G_SSID_Counter = 0;

		String tempString;
		// print the network number and name for each network found:
		for (int thisNetwork = 0; thisNetwork < foundNetworksAmount; thisNetwork++)
		{
			if (G_SSID_Counter < MAX_SSID) // store only maximum of <SSIDMAX> SSDI's with high dB > -80 && WiFi.RSSI(thisNet) > -81
			{
				// Transfering the MAX_SSID amounts of network names to the global list
				tempString = WiFi.SSID(thisNetwork);
				int i;
				for (i = 0; i < tempString.length(); ++i)
				{
					G_SSID_List[G_SSID_Counter][i] = tempString[i];
				}
				G_SSID_List[G_SSID_Counter][i] = 0;

				#ifdef Debug_On
					// print each network
					Serial.print(G_SSID_Counter);
					Serial.print(". ");
					Serial.print(G_SSID_List[G_SSID_Counter]);
					Serial.print("\t\tSignal: ");
					Serial.print(WiFi.RSSI(thisNetwork));
					Serial.println(" dBm");
					Serial.flush();
				#endif

				G_SSID_Counter++;
			}
		} // end for list loop
	}
}

/* Wifi Access Point Initialisation */
/* Starting Webserver and UDP Server on success */
void EasyWiFi::AccessPointSetup()
{
	int tries = 5;  // 5 tries to setup AccessPoint
	
	#ifdef Debug_On
		Serial.print("* Creating access point named: "); Serial.println(G_AccessPointName);
	#endif
	
	// Generate Access Point IP Adress and setup config
	G_AP_IP = IPAddress((char)random(11, 172), (char)random(0, 255), (char)random(0, 255), 0x01); // Generate random IP address in private IP range
	WiFi.end();																					 // close Wifi - just to be sure
	delay(3000);																				 // Wait 3 seconds
	WiFi.config(G_AP_IP, G_AP_IP, G_AP_IP, IPAddress(255, 255, 255, 0));							 // Setup config

	while (tries > 0)
	{
		G_AP_Status = WiFi.beginAP(G_AccessPointName, ACCESS_POINT_CHANNEL); // setup AccessPoint
		if (G_AP_Status != WL_AP_LISTENING) // if AccessPoint is not listening -> Retry
		{
			#ifdef Debug_On
				Serial.print(".");
			#endif        
			--tries;
			// Just to be sure, set config again
			WiFi.config(G_AP_IP, G_AP_IP, G_AP_IP, IPAddress(255, 255, 255, 0));
		}
		else
			break; // break while loop when AccessPoint is connected/listening
	}

	if (tries == 0)
	{  
		// not possible to connect in 5 retries
		#ifdef Debug_On  
			Serial.println("* Creating access point failed");
		#endif     
	}
	else
	{
		delay(2000);
		PrintWiFiStatus();            // you're connected now, so print out the status
		G_UDP_AP_DNS.begin(UDP_PORT); // start the UDP server
		G_AP_Webserver.begin();       // start the Access Point web server on port 80
	}
}

/* DNS Routines via UDP, act on DSN requests on Port 53 */
/* assume wifi UDP connection has been set up */
void EasyWiFi::AccessPointDNSScan()
{
	int i = 0; // generic loop counter
	int replyCounter, packetCounter;
	unsigned int packetSize = 0;
	unsigned int replySize = 0;
	byte G_DNSReplybuffer[UDP_PACKET_SIZE]; // buffer to hold the send DNS reply

	packetSize = G_UDP_AP_DNS.parsePacket();
	if (packetSize) // We've received a packet, read the data from it
	{
		G_UDP_AP_DNS.read(G_UDP_PacketBuffer, packetSize); // read the packet into the buffer
		G_AP_DNS_CLIENT_IP = G_UDP_AP_DNS.remoteIP();
		G_DNS_ClientPort = G_UDP_AP_DNS.remotePort();

		//if ( (G_AP_DNS_CLIENT_IP != G_APip) && (G_DNS_RequestCounter<=DNS_MAX_REQUESTS) )       // skip own requests - ie ntp-pool time requestfrom Wifi module
		if (G_AP_DNS_CLIENT_IP != G_AP_IP) // skip own requests - ie ntp-pool time requestfrom Wifi module
		{
			#ifdef Debug_On_X  
				Serial.print("DNS-packets ("); Serial.print(packetSize);
				Serial.print(") from "); Serial.print(G_AP_DNS_CLIENT_IP);
				Serial.print(" port "); Serial.println(G_DNS_ClientPort);
				for (i = 0; i < packetSize; ++i)
				{
					Serial.print(G_UDP_PacketBuffer[i], HEX); Serial.print(":");
				}
				Serial.println(" ");
				for (i = 0; i < packetSize; ++i)
				{
					Serial.print((char)G_UDP_PacketBuffer[i]);//Serial.print("");
				}
				Serial.println("");
			#endif

			//Copy Packet ID and IP into DNS header and DNS answer
			G_DNS_ReplyHeader[0] = G_UDP_PacketBuffer[0];
			G_DNS_ReplyHeader[1] = G_UDP_PacketBuffer[1]; // Copy ID of Packet offset 0 in Header (look at definition)
			G_DNS_ReplyAnswer[12] = G_AP_IP[0];
			G_DNS_ReplyAnswer[13] = G_AP_IP[1];
			G_DNS_ReplyAnswer[14] = G_AP_IP[2];
			G_DNS_ReplyAnswer[15] = G_AP_IP[3]; // copy Access Point Ip address offset 12 in Answer (look at definition)

			replyCounter = 0;   // set reply buffer counter
			packetCounter = 12; // set packetbuffer counter @ QUESTION QNAME section

			// copy Header into reply
			for (i = 0; i < DNS_HEADER_SIZE; ++i)
			{
				G_DNSReplybuffer[replyCounter++] = G_DNS_ReplyHeader[i];
			}

			// copy Qusetion into reply: Name labels until octet=0x00
			while (G_UDP_PacketBuffer[packetCounter] != 0)
			{
				G_DNSReplybuffer[replyCounter++] = G_UDP_PacketBuffer[packetCounter++];
			}

			// copy end of question plus Qtype and Qclass 5 octets
			for (i = 0; i < 5; ++i)
			{
				G_DNSReplybuffer[replyCounter++] = G_UDP_PacketBuffer[packetCounter++];
			}

			// copy Answer into reply
			for (i = 0; i < DNS_ANSWER_SIZE; ++i)
			{
				G_DNSReplybuffer[replyCounter++] = G_DNS_ReplyAnswer[i];
			}

			replySize = replyCounter;

			#ifdef Debug_On_X  
				Serial.print("* DNS-Reply ("); Serial.print(replySize);
				Serial.print(") from "); Serial.print(G_APip);
				Serial.print(" port "); Serial.println(UDP_PORT);
				for (i = 0; i < replySize; ++i)
				{
					Serial.print(G_DNSReplybuffer[i], HEX); Serial.print(":");
				}
				Serial.println(" ");
				for (i = 0; i < replySize; ++i)
				{
					Serial.print((char)G_DNSReplybuffer[i]);//Serial.print("");
				}
				Serial.println("");
			#endif     

			// Send DSN UDP packet
			G_UDP_AP_DNS.beginPacket(G_AP_DNS_CLIENT_IP, G_DNS_ClientPort); //reply DNS question
			G_UDP_AP_DNS.write(G_DNSReplybuffer, replySize);
			G_UDP_AP_DNS.endPacket();
			G_DNS_RequestCounter++;

		} // end loop correct IP
	} // end loop received packet
}

// Check the Access Point wifi Client Responses and read the inputs on the main Access Point web-page.
void EasyWiFi::AccessPointWiFiClientCheck()
{
	String PostLine = "";     // make a String to hold incoming POST-Data
	String currentLine = "";  // make a String to hold incoming data from the client
	char charBuffer;          // Character read buffer
	int t, u, v, pos1, pos2;  // loop counter

	WiFiClient client = G_AP_Webserver.available();  // listen for incoming clients
	if (client) // if you get a client,
	{
		#ifdef Debug_On     
			Serial.println("* New Access Point webclient");
		#endif

		while (client.connected()) // loop while the client's connected
		{
			if (client.available()) // if there's bytes to read from the client,
			{
				charBuffer = client.read(); // read a byte, then

				#ifdef Debug_On  
					// print it out the serial monitor
					Serial.write(charBuffer);                 
				#endif

				if (charBuffer == '\n') // if the byte is a newline character
				{
					// if the current line is blank, you got two newline characters in a row.
					// that's the end of the client HTTP request, so send a response:
					if (currentLine.length() == 0)
					{
						// HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
						// and a content-type so the client knows what's coming, then a blank line:
						client.println("HTTP/1.1 200 OK");
						client.println("Content-type:text/html");
						client.println();
						// the content of the HTTP response follows the header:
						client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"); // metaview
						client.println("<body style=\"background-color:SteelBlue\">"); // set color CCS HTML5 style . I | I . I | I .
						client.print("<p style=\"font-family:verdana; color:GhostWhite\">&nbsp<font size=3> l </font><font size=4> l </font><font size=5> | </font><font size=4> l </font><font size=3> l </font><font size=4> l </font><font size=5> | </font><font size=4> l </font><font size=3> l </font> <br>");
						client.print("<font size=5>Arduino</font>  <br><font size=5>"); client.print(WiFi.SSID()); client.println("</font>");
						client.print("<p style=\"font-family:verdana; color:Gainsboro\">");
						for (t = 0; t < G_SSID_Counter; t++)
						{
							client.print(t); client.print(". ["); client.print(G_SSID_List[t]); client.print("]<br>");
						}
						client.println("</font></p>");
						client.print("<p style=\"font-family:verdana; color:Gainsboro\">Enter Wifi-Ssid (Number or name) and Pass:<br>");
						client.println("<form method=POST action=\"checkpass.php\">");
						client.println("<input type=text name=XXID><br>");                      // XXID is a key word fo parsing the response
						client.println("<form method=POST action=\"checkpass.php\">");
						client.println("<input type=password name=XXPS><br>");                   //XXPS is a key word for parsing
						client.println("<input type=submit name=action value=Submit>");
						client.println("</form></p>");
						client.print("<meta http-equiv=\"refresh\" content=\"30;url=http://"); client.print(WiFi.localIP()); client.println("\">");
						// The HTTP response ends with another blank line:
						client.println();
						// break out of the while loop:
						break;
					}
					else // if you got a newline, then clear currentLine:
					{
						currentLine = "";
					}
				}
				else if (charBuffer != '\r') // if you got anything else but a carriage return character,
				{
					currentLine += charBuffer;      // add it to the end of the currentLine
				}
				// Check to see if the client request was a post on our checkpass.php
				if (currentLine.endsWith("POST /checkpass.php"))
				{
					#ifdef Debug_On            
						Serial.println("* Found AP Server POST");
					#endif        

					currentLine = "";
					while (client.connected()) // loop while the client's connected
					{
						if (client.available()) // if there's bytes to read from the client,
						{
							charBuffer = client.read();    // read a byte, then

							#ifdef Debug_On_X                      
								Serial.write(charBuffer);  // print it out the serial monitor
							#endif    

							if (charBuffer == '\n') // if the byte is a newline character
							{
								//if (currentLine.length() == 0) break; // no lenght :  end of data request
								currentLine = "";                      // if you got a newline, then clear currentLine:
							}
							else if (charBuffer != '\r')
							{
								currentLine += charBuffer;     // if you got anything else but a carriage return character, add to string
							}

							if (currentLine.endsWith("XXID="))
							{
								pos1 = currentLine.length();
							}

							if (currentLine.endsWith("&XXPS="))
							{
								pos2 = currentLine.length();
							}

							if (currentLine.endsWith("&action")) // Check read line on "data=" start that ends with "&action"
							{
								t = currentLine.length();

								if (t < 78)
								{
									if (pos2 - pos1 == 7)
									{
										u = (currentLine[pos1] - 48);
										if (u > G_SSID_Counter)
										{
											u = 0; // one digit - convert to max index
										}

										// copy list name to ssid
										for (v = 0; G_SSID_List[u][v] != 0; v++)
										{
											G_SSID[v] = G_SSID_List[u][v]; 
											G_SSID[v] = 0;
										}
									}
									else
									{
										u = 0;
										// if not one digit, copy input name to ssid
										for (v = pos1; v < (pos2 - 6); v++)
										{
											G_SSID[u++] = currentLine[v];
										}
										G_SSID[u] = 0;
									}

									u = 0;
									for (v = pos2; v < (t - 7); v++)
									{
										G_PASS[u++] = currentLine[v];
									}
									G_PASS[u] = 0;

									CredentialsHandler::Write_Credentials(G_SSID, sizeof(G_SSID), G_PASS, sizeof(G_PASS)); // write credentials to flash 
								}
								else
								{
									#ifdef Debug_On                        
										Serial.print("* Invalid input from AP Client"); Serial.println(currentLine);
									#endif                            
								}

								#ifdef Debug_On                        
									Serial.print("\n* AP client input found: ");
									Serial.print(G_SSID); Serial.print(","); Serial.println("******");
								#endif     

								// copy inputs to G_SSID and G_PASS
								G_AP_InputFlag = 1; // flag AP input
								break;
							}
						}
					}
					//delay(1000);
					client.println("HTTP/1.1 200 OK");
					client.println("Content-type:text/html");
					client.println();
					client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"); // metaview
					client.println("<body style=\"background-color:SteelBlue\">"); // set color CCS HTML5 style
					client.print("<p style=\"font-family:verdana; color:GhostWhite\">&nbsp<font size=3> l </font><font size=4> l </font><font size=5> | </font><font size=4> l </font><font size=3> l </font><font size=4> l </font><font size=5> | </font><font size=4> l </font><font size=3> l </font> <br>");
					client.print("<font size=5>Arduino</font>  <br><font size=5>"); client.print(WiFi.SSID()); client.println("</font></p>");
					client.print("<p style=\"font-family:verdana; color:DarkOrange\"><font size=5>Thank You.....</font><br>");
					client.println("<meta http-equiv=\"refresh\" content=\"6;url=/\" />");
					// The HTTP response ends with another blank line:
					client.println();
					// break out of the while loop: 
					delay(1000);   // wait 2 seconds to show the message !                     
					break;
				} // end loop POST check


				if (currentLine.endsWith("GET /generate_204")) // HTTP generate_204 HTTP/1.1
				{
					client.println("HTTP/1.1 200 OK");
					client.println("Content-type:text/html");
					client.println();
					client.print("<meta http-equiv=\"refresh\" content=\"0;url=http://"); client.print(WiFi.localIP()); client.print("\">");
					client.println();
					#ifdef Debug_On                  
						Serial.println("**generate_204 replyied 200 OK");
					#endif             
					// The HTTP response ends with another blank line:
					client.println();
					// break out of the while loop:              
					break;
				}

				/*
						if (currentLine.endsWith("GET /RESET"))
						{
						  client.println("HTTP/1.1 200 OK");
						  client.println("Content-type:text/html");
						  client.println();
						  client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"); // metaview
						  client.println("<body style=\"background-color:black\">"); // set color CCS HTML5 style
						  client.print( "<h2 style=\"font-family:verdana; color:GoldenRod\">Access Router ");client.print(WiFi.SSID());client.println("</h2>");
						  client.print("<packetCounter style=\"font-family:verdana; color:indianred\">Authentication failed<br>");
						  client.print("<meta http-equiv=\"refresh\" content=\"4;url=/\" />");
						  G_DNS_RequestCounter=0;
						  Serial.println("**Reset");
						  break;
						}

						if (currentLine.endsWith("GET /connecttest")) // HTTP redirect for browsers with connect-test
						{
						  // client.println("HTTP/1.1 302 found");
						  // client.print("Location: http://");client.print(WiFi.localIP());client.println("/");
						  client.println("HTTP/1.1 200 OK");
						  client.println("Content-type:text/html");
						  client.println();
						  client.print("<meta http-equiv=\"refresh\" content=\"1;url=/\" />");
						  client.println();
						  Serial.println("**generate_204 replyied 200 OK");
						  break;
						}
				*/

			} // end loop client data avaialbe    
		} // end while loop client connected

		// close the connection:
		client.stop();
		#ifdef Debug_On     
			Serial.println("* AP webclient disconnected");
		#endif
	} // end If Client
}

// Check the Access Point wifi Client Responses and read the inputs on the main Access Point web-page.
void EasyWiFi::AccessPointWiFiClientCheck_Test()
{
	String PostLine = "";     // make a String to hold incoming POST-Data
	String currentLine = "";  // make a String to hold incoming data from the client
	char charBuffer;          // Character read buffer
	int t, u, v, pos1, pos2;  // loop counter

	WiFiClient client = G_AP_Webserver.available();  // listen for incoming clients
	if (client) // if you get a client,
	{
		#ifdef Debug_On     
			Serial.println("* New Access Point webclient");
		#endif

		while (client.connected()) // loop while the client's connected
		{
			if (client.available()) // if there's bytes to read from the client,
			{
				processRequest(client);
				break;
//				charBuffer = client.read(); // read a byte, then
//
//#ifdef Debug_On  
//				// print it out the serial monitor
//				Serial.write(charBuffer);
//#endif
//
//				if (charBuffer == '\n') // if the byte is a newline character
//				{
//					// if the current line is blank, you got two newline characters in a row.
//					// that's the end of the client HTTP request, so send a response:
//					if (currentLine.length() == 0)
//					{
//						
//					}
//					else // if you got a newline, then clear currentLine:
//					{
//						currentLine = "";
//					}
//				}
//				else if (charBuffer != '\r') // if you got anything else but a carriage return character,
//				{
//					currentLine += charBuffer;      // add it to the end of the currentLine
//				}
			} // end loop client data avaialbe    
		} // end while loop client connected

		// close the connection:
		client.stop();
		#ifdef Debug_On     
			Serial.println("* AP webclient disconnected");
		#endif
	} // end If Client
}

void EasyWiFi::processRequest(WiFiClient client) {
	// Read the request from the client
	String request = client.readStringUntil('\r');
	client.flush();

	#ifdef Debug_On     
		Serial.println("Process request");
	#endif

	// Handle the request
	if (request.indexOf("/list_networks") != -1)
	{
		#ifdef Debug_On     
			Serial.println("* Send network list");
		#endif
		// Send the list of Wi-Fi networks as a web page
		sendNetworkList(client);
	}
	else if (request.indexOf("/enterPassword") != -1)
	{
		#ifdef Debug_On     
			Serial.println("* sendEnterWifiPasswordPage");
		#endif
		// Process the network selection and password entry
		sendEnterWifiPasswordPage(client, request);
	}
	else if (request.indexOf("POST /connect") != -1)
	{
		#ifdef Debug_On     
			Serial.println("* handleProvidedWifiCredentials");
		#endif
		// Process the connection form submission
		handleProvidedWifiCredentials(client, request);
	}
	else
	{
		#ifdef Debug_On     
			Serial.println("* Send default page");
		#endif
		// Send the default web page
		sendStartPage(client);
	}
}

void EasyWiFi::handleProvidedWifiCredentials(WiFiClient client, String request) {
	// Extract the network SSID and password from the request body
	String requestBody = request.substring(request.indexOf("\r\n\r\n") + 4);
	String networkName = getValueFromRequest(requestBody, "network");
	String password = getValueFromRequest(requestBody, "password");

	// Attempt to connect to the network
	Serial.print("* Entered Wifi PW: ");
	Serial.println(password.c_str());

	Serial.print("* Entered Wifi SSID: ");
	Serial.println(networkName.c_str());


	// Connect to the network using the provided SSID and password
	boolean connected = connectToNetwork(networkName, password);

	//G_SSID = networkName;
	//TryToConnectToWifiWithCredentials()

	// Send the response back to the client
	client.println("HTTP/1.1 200 OK");
	client.println("Content-Type: text/html");
	client.println();

	client.println("<html>");
	client.println("<head><title>Connection Status</title></head>");
	client.println("<meta charset='UTF-8'>");
	client.println("<meta name='viewport' content='width=device-width,initial-scale=1'>");
	client.println("<body>");

	if (connected) {
		client.println("<h2>Network Connection Successful</h2>");
		// Additional actions or information for a successful connection
	}
	else {
		client.println("<h2>Network Connection Failed</h2>");
		client.println("<p>Failed to connect to network: " + networkName + "</p>");
		client.println("<button onclick=\"location.href='/networklist'\">Select network and try again</button>");
	}

	client.println("</body>");
	client.println("</html>");
	client.println();
}

String EasyWiFi::getValueFromRequest(String requestBody, String key) {
	String value = "";
	int keyIndex = requestBody.indexOf(key + "=");
	if (keyIndex != -1) {
		int valueIndex = keyIndex + key.length() + 1;
		int endIndex = requestBody.indexOf("&", valueIndex);
		if (endIndex == -1) {
			endIndex = requestBody.length();
		}
		value = requestBody.substring(valueIndex, endIndex);
		value = urlDecode(value);
	}
	return value;
}

String EasyWiFi::urlDecode(String str) {
	String decodedString = "";
	char temp[3] = { 0 };
	unsigned int len = str.length();
	unsigned int i = 0;
	while (i < len) {
		char decodedChar;
		if (str.charAt(i) == '%') {
			temp[0] = str.charAt(i + 1);
			temp[1] = str.charAt(i + 2);
			decodedChar = strtol(temp, NULL, 16);
			i += 2;
		}
		else if (str.charAt(i) == '+') {
			decodedChar = ' ';
		}
		else {
			decodedChar = str.charAt(i);
		}
		i++;
		decodedString += decodedChar;
	}
	return decodedString;
}

boolean EasyWiFi::connectToNetwork(String networkName, String password) {
	int attempts = 4;
	int delayInSeconds = 2;
	boolean connected = false;

	for (int i = 0; i < attempts; i++)
	{
		WiFi.begin(networkName.c_str(), password.c_str());
		delay(delayInSeconds * 1000);
		if (WiFi.status() == WL_CONNECTED)
		{
			connected = true;
			break;
		}
	}

	return connected;
}

void EasyWiFi::sendStartPage(WiFiClient client) {
	// Send the HTTP header
	client.println("HTTP/1.1 200 OK");
	client.println("Content-type:text/html");
	client.println();

	// Send the default web page
	client.println("<html>");
	client.println("<head><title>Welcome</title></head>");
	client.println("<meta charset='UTF-8'>");
	client.println("<meta name='viewport' content='width=device-width,initial-scale=1'>");
	client.println("<body>");
	client.println("<h1>Welcome to the Arduino IoT Web Server</h1>");
	client.println("<p>Please select your network:</p>");
	client.println("<a href=\"/list_networks\">Network Selection</a>");
	client.println("</body>");
	client.println("</html>");
	// The HTTP response ends with another blank line:
	client.println();
}

void EasyWiFi::sendNetworkList(WiFiClient client) {
	// Send the HTTP header
	client.println("HTTP/1.1 200 OK");
	client.println("Content-Type: text/html");
	client.println();

	// Send the HTML page
	client.println("<html>");
	client.println("<head><title>Select your network</title></head>");
	client.println("<meta charset='UTF-8'>");
	client.println("<meta name='viewport' content='width=device-width,initial-scale=1'>");
	client.println("<body>");
	client.println("<h2>Select your network:</h2>");

	// Generate a button for each network
	for (int i = 0; i < G_SSID_Counter; i++)
	{
		String networkName = G_SSID_List[i];
		client.print("<form action=\"/enterPassword?network=" + networkName + "\" method=\"post\">");
		client.print("<input type=\"submit\" value=\"");
		client.print(i+1);
		client.print(". ");
		client.print(networkName);
		client.println("\"/>");
		client.println("</form>");
	}

	client.println("</body>");
	client.println("</html>");
	client.print("<meta http-equiv=\"refresh\" content=\"20;url=http://"); client.print(WiFi.localIP()); client.println("\">");
	// The HTTP response ends with another blank line:
	client.println();
}

void EasyWiFi::sendEnterWifiPasswordPage(WiFiClient client, String request)
{
	// Extract the selected network from the request
	String networkName = request.substring(request.indexOf("/enterPassword?network=") + 23);
	networkName = networkName.substring(0, networkName.indexOf(" "));

	// Send the HTTP header
	client.println("HTTP/1.1 200 OK");
	client.println("Content-Type: text/html");
	client.println();

	// Send the HTML page with password entry form
	client.println("<html>");
	client.println("<meta charset='UTF-8'>");
	client.println("<meta name='viewport' content='width=device-width,initial-scale=1'>");
	client.println("<head><title>Enter Wi-Fi Password</title></head>");
	client.println("<body>");
	client.print("<h2>Network: ");
	client.print(networkName);
	client.println("</h2>");
	// Input fields
	client.println("<form id=\"connectForm\" onsubmit=\"submitForm(event)\" method=\"post\">");
	client.println("Password: <input id=\"passwordField\" type=\"password\" name=\"password\" /><br/>");
	client.println("Show password: <input id=\"showPasswordCheckbox\" type=\"checkbox\" onchange=\"togglePasswordVisibility()\" /><br/>");
	client.println("<input type=\"submit\" value=\"Connect\"/>");
	client.println("</form>");

	// JavaScript section
	client.println("<script>");
	// Function to toggle password visibility
	client.println("function togglePasswordVisibility() {");
	client.println("  var passwordField = document.getElementById('passwordField');");
	client.println("  var showPasswordCheckbox = document.getElementById('showPasswordCheckbox');");
	client.println("  if (showPasswordCheckbox.checked) {");
	client.println("    passwordField.type = 'text';");
	client.println("  } else {");
	client.println("    passwordField.type = 'password';");
	client.println("  }");
	client.println("}");
	// Function for handling the password sending
	client.println("function submitForm(event) {");
	client.println("  event.preventDefault();");
	client.println("  var password = document.getElementById('passwordField').value;");
	client.println("  var xhr = new XMLHttpRequest();");
	client.println("  xhr.open('POST', '/connect', true);");
	client.println("  xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');");
	client.println("  xhr.onreadystatechange = function() {");
	client.println("    if (xhr.readyState === 4 && xhr.status === 200) {");
	client.println("      // Handle the server response if needed");
	client.println("      console.log(xhr.responseText);");
	client.println("    }");
	client.println("  };");
	client.println("  var params = 'network=" + networkName + "&password=' + encodeURIComponent(password);");
	client.println("  xhr.send(params);");
	client.println("}");
	client.println("</script>");

	client.println("</body>");
	client.println("</html>");
	// The HTTP response ends with another blank line:
	client.println();
}

// SERIALPRINT Wifi Status - only for debug
void EasyWiFi::PrintWiFiStatus()
{
#ifdef Debug_On
	// print the SSID of the network you're attached to:
	Serial.print("* SSID: ");
	Serial.print(WiFi.SSID());
	// print your WiFi shield's IP address:
	IPAddress ip = WiFi.localIP();
	Serial.print(" - IP Address: ");
	Serial.print(ip);
	// print your WiFi gateway:
	IPAddress ip2 = WiFi.gatewayIP();
	Serial.print(" - IP Gateway: ");
	Serial.print(ip2);
	// print the received signal strength:
	long rssi = WiFi.RSSI();
	Serial.print("- Rssi: ");
	Serial.print(rssi);
	Serial.println(" dBm");
#endif
}

bool EasyWiFi::IsWifiNotConnectedOrReachable(int wifiStatus)
{
	return (wifiStatus != WL_CONNECTED) || (WiFi.RSSI() <= -90) || (WiFi.RSSI() == 0);
}

int EasyWiFi::TryToConnectToWifiWithCredentials()
{
	int connectionAttempts = 0;
	int wifiStatus = WiFi.status();
	while (IsWifiNotConnectedOrReachable(wifiStatus) && connectionAttempts < MAX_CONNECT) // attempt to connect to WiFi network 3 times
	{
		#ifdef Debug_On
			Serial.print("* Attempt#"); Serial.print(connectionAttempts); Serial.print(" to connect to Network: "); Serial.println(G_SSID); // print the network name (SSID);
		#endif
		wifiStatus = WiFi.begin(G_SSID, G_PASS);     // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
		delay(2000);                                 // wait 2 seconds for connection:
		connectionAttempts++;                        // try-counter
	}
	return connectionAttempts;
}

void EasyWiFi::UpdateDeviceConnectedStatus()
{
	// Check AP status - new client on or off?
	if (G_AP_Status != WiFi.status())
	{
		G_AP_Status = WiFi.status();        // it has changed update the variable
		if (G_AP_Status == WL_AP_CONNECTED) // a device has connected to the AP
		{
			#ifdef Debug_On                     
				Serial.println("Device connected to AP\n");
			#endif                 
			SetNINA_LED(CYAN); // Client on AP : CYAN
			G_DNS_RequestCounter = 0; // reset DNS counter
		}
		else // a device has disconnected from the AP, and we are back in listening mode
		{
			#ifdef Debug_On                  
				Serial.println("Device disconnected from AP\n");
			#endif                    
		}
	} // end if loop changed G_AP_Status  
}


