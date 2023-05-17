#include "CredentialsHandler.h"

#define CREDENTIAL_FILE "/fs/WifiCredentials"

#define Debug_On       // Debug option  -serial print

int SEED = 4;

// Set Seed of the Cypher, should be positive
void CredentialsHandler::SetSeed(int seed)
{
	if (seed >= 0)
	{
		SEED = seed;
	}
}

/* Read credentials ID,pass to Flash file , Comma separated style*/
byte CredentialsHandler::Read_Credentials(char* buf1, char* buf2)
{
	int u, t, currentReadPosition = 0;
	char buffer[68], comma = 1, zero = 0;
	char bufc[68];
	WiFiStorageFile file = WiFiStorage.open(CREDENTIAL_FILE);
	if (file) // check if file is valid/exists
	{
		file.seek(0); // start filestream from the beginning / read file from the beginning
		if (file.available())
		{
			// read file buffer into memory, max size is 64 bytes for 2 char-strings
			currentReadPosition = file.read(buffer, 68);  // read lenght of 68 chars into the buffer
		}

		if (currentReadPosition != 0) // check if, it actually read something
		{
			t = 0;
			u = 0;
			while (buffer[t] != comma) // read ID until comma
			{
				bufc[u++] = buffer[t++];
				if (u > 31)
					break;
			}
			bufc[u] = 0;
			SimpleDecypher(bufc, buf1);
			u = 0;
			t++;                // move to second part: pass
			while (buffer[t] != zero) // read till zero
			{
				bufc[u++] = buffer[t++];
				if (u > 31)
					break;
			}
			bufc[u] = 0;
			SimpleDecypher(bufc, buf2);
		}
		#ifdef Debug_On
			Serial.print("* Read Credentials : ");
			Serial.println(currentReadPosition);
		#endif    
		file.close();
		return(currentReadPosition);
	}
	else
	{
		#ifdef Debug_On
			Serial.println("* Cant read Credentials :");
		#endif    
		file.close();
		return(0);
	}
}

/* Write credentials ID,pass to Flash file , Comma separated style*/
byte CredentialsHandler::Write_Credentials(char* buf1, int size1, char* buf2, int size2)
{
	int c = 0;
	char comma = 1, zero = 0;
	char buf[32];
	WiFiStorageFile file = WiFiStorage.open(CREDENTIAL_FILE);
	if (file)
	{
		file.erase();     // erase content bnefore writing
	}
	SimpleCypher(buf1, buf);
	c = c + file.write(buf, size1);
	file.write(&comma, 1);
	c++;
	SimpleCypher(buf2, buf);
	c = c + file.write(buf, size2);
	file.write(&zero, 1);
	c++;
	if (c != 0)
	{
		#ifdef Debug_On
			Serial.print("* Written Credentials : ");
			Serial.println(c);
		#endif
		file.close();
		return(c);
	}
	else
	{
		#ifdef Debug_On
			Serial.println("* Cant write Credentials");
		#endif  
		file.close();
		return(0);
	}
}

/* Erase credentials in flkash file */
byte CredentialsHandler::Erase_Credentials()
{
	char empty[16] = "0empty0o0empty0";
	WiFiStorageFile file = WiFiStorage.open(CREDENTIAL_FILE);
	if (file)
	{
		file.seek(0);
		file.write(empty, 16); //overwrite flash
		file.erase();
		#ifdef Debug_On
			Serial.println("* Erased Credentialsfile : ");
		#endif  
		file.close(); return(1);
	}
	else
	{
		#ifdef Debug_On
			Serial.println("* Could not erased Credentialsfile : ");
		#endif  
		file.close();
		return(0);
	}
}

/* Check credentials file */
byte CredentialsHandler::Check_Credentials()
{
	WiFiStorageFile file = WiFiStorage.open(CREDENTIAL_FILE);
	if (file)
	{
		#ifdef Debug_On
			Serial.println("* Found Credentialsfile : ");
		#endif  
		file.close();
		return(1);
	}
	else
	{
		#ifdef Debug_On
			Serial.println("* Could not find Credentialsfile : ");
		#endif  
		file.close();
		return(0);
	}
}

/* Simple DeCyphering the text code */
void CredentialsHandler::SimpleDecypher(char* textin, char* textout)
{
	int c, t = 0;
	while (textin[t] != 0)
	{
		textout[t] = textin[t] - SEED % 17 + t % 7;
		t++;
	}
	textout[t] = 0;
	#ifdef Debug_On
	// Serial.print("* Decyphered ");Serial.print(t);Serial.print(" - ");Serial.println(textout);
	#endif
}

/* Simple Cyphering the text code */
void CredentialsHandler::SimpleCypher(char* textin, char* textout)
{
	int c, t = 0;
	while (textin[t] != 0)
	{
		textout[t] = textin[t] + SEED % 17 - t % 7;
		t++;
	}
	textout[t] = 0;
	#ifdef Debug_On
	// Serial.print("* Cyphered ");Serial.print(t);Serial.print(" - ");Serial.println(textout);
	#endif
}

