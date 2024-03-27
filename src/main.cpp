/**
 * A cleaned up, tidied i2c sniffer, heavily based on work by @WhitehawkTailor
 */

#include <Arduino.h>

#define PIN_SDA 12
#define PIN_SCL 13

#define I2C_IDLE 0
#define I2C_TRX 2

// Status of the I2C BUS
static volatile byte i2cStatus = I2C_IDLE;
// Array for storing data we got. Arbitrarily huge.
static volatile byte dataBuffer[100000];
// points to the first empty position in the dataBuffer for bytes to be written
static volatile uint16_t bufferPoiW = 0;
// points to the position where we've read up until
static uint16_t bufferPoiR = 0;
// GREAT explanation: https://www.circuitbasics.com/basics-of-the-i2c-communication-protocol/
// TODO:
// Using a byte per bit seems wasteful, but we need some way to include start and stop conditions. Perhaps
// an array of bytes, where start and stop are implicit, and we just store the data bits?
// Parse data after the fact rather than during sampling. Will make shit easier to test, and allow
// easier garbage filtering. God I hate C++
// Ensure start and stop conditions are reliably detectable. Going to be infuriating to debug if not.

void IRAM_ATTR i2cTriggerOnRaisingSCL()
{
	if (i2cStatus == I2C_TRX) {
		// Only bytes received during a transmission are likely valid.
		dataBuffer[bufferPoiW++] = '0' + digitalRead(PIN_SDA);
	}
}

// Continuously polls busPin until it has numReadings stable readings, then returns the reading
bool IRAM_ATTR busStableRead(int busPin, int numReadings)
{
	int reading = digitalRead(busPin);
	for (int i = 0; i < numReadings; i++)
	{
		if (digitalRead(busPin) != reading)
		{
			i = 0;
		}
	}
	return reading;
}

void IRAM_ATTR i2cTriggerOnChangeSDA()
{
	// If clock is low, there's nothing to do.
	if (!digitalRead(PIN_SCL))
		return;

	bool sda = digitalRead(PIN_SDA);

	if (i2cStatus == I2C_IDLE && !sda)
	{
		// Clock was high, SDA changed to low, this indicates a start.
		i2cStatus = I2C_TRX;
		dataBuffer[bufferPoiW++] = 'S';
	}
	else if (i2cStatus == I2C_TRX && sda)
	{
		// Clock was high, SDA changed to high, this indicates a stop.
		i2cStatus = I2C_IDLE;
		dataBuffer[bufferPoiW++] = 's';
	}
}

void resetI2cVariable()
{
	i2cStatus = I2C_IDLE;
	bufferPoiW = 0;
	bufferPoiR = 0;
}

void processDataBuffer()
{
	if (bufferPoiW == bufferPoiR)
		return;

	uint16_t pw = bufferPoiW;

	for (int i = bufferPoiR; i < pw; i++)
	{
		byte data = dataBuffer[i];
		if (data == 'S')
		{
			Serial.print("S");
		}
		else if (data == 's')
		{
			Serial.print("T\n");
		}
		else
		{
			Serial.write(data);
		}
		//Serial.write(dataBuffer[i]);
		bufferPoiR++;
	}

	// if there is no I2C action in progress and there wasn't during the Serial.print then buffer was printed out completely and can be reset.
	if (i2cStatus == I2C_IDLE && pw == bufferPoiW)
	{
		bufferPoiW = 0;
		bufferPoiR = 0;
	}
}

void setup()
{
	pinMode(PIN_SCL, INPUT_PULLUP);
	pinMode(PIN_SDA, INPUT_PULLUP);
	resetI2cVariable();

	attachInterrupt(PIN_SCL, i2cTriggerOnRaisingSCL, RISING); // trigger for reading data from SDA
	attachInterrupt(PIN_SDA, i2cTriggerOnChangeSDA, CHANGE);  // for I2C START and STOP
	Serial.begin(115200);
}

void loop()
{
	// if it is in IDLE, then write out the databuffer to the serial consol
	if (i2cStatus == I2C_IDLE)
	{
		// Print out half second chunks for analysis
		Serial.println("START");
		processDataBuffer();
		Serial.println("END");
		delay(500);
	}
}