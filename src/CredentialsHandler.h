// CredentialsHandler.h

#ifndef _CREDENTIALSHANDLER_h
#define _CREDENTIALSHANDLER_h

#include "arduino.h"
#include <WiFiNINA.h>
#include <WiFiUdp.h>

class CredentialsHandler
{
public:
    static void SetSeed(int value);
    static byte Check_Credentials();
    static byte Erase_Credentials();
    static byte Write_Credentials(char* buf1, int size1, char* buf2, int size2);
    static byte Read_Credentials(char* buf1, char* buf2);

private:
    static void SimpleDecypher(char* textin, char* textout);
    static void SimpleCypher(char* textin, char* textout);
};

#endif

