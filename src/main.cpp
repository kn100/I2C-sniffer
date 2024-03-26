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
// Bits that have appeared on the bus
static volatile byte bitCount = 0;
// counter of bytes were writen in one communication
static volatile uint16_t byteCount = 0;
// Counter of false start events
static volatile uint16_t falseStart = 0;

// Statistical variables
static volatile uint16_t sclUpCnt = 0;
static volatile uint16_t sdaUpCnt = 0;
static volatile uint16_t sdaDownCnt = 0;

void IRAM_ATTR i2cTriggerOnRaisingSCL()
{
	sclUpCnt++;
	// Unclear to me what a false start is. Maybe return here.
	if (i2cStatus == I2C_IDLE)
	{
		falseStart++;
	}

	byte i2cBitC = digitalRead(PIN_SDA);

	// decide where we are and what to do with incoming data
	byte i2cCase = 0; // normal case

	if (bitCount == 8) // ACK case
	{
		i2cCase = 1;
	}

	if (bitCount == 7 && byteCount == 0) // R/W if the first address byte
	{
		i2cCase = 2;
	}

	bitCount++;

	switch (i2cCase)
	{
	case 0: // Normal case
		dataBuffer[bufferPoiW++] = '0' + i2cBitC;
		break;
	case 1: // ACK case (happens at the end of an address or a data byte)
		dataBuffer[bufferPoiW++] = (i2cBitC ? '-' : '+');
		byteCount++;
		bitCount = 0;
		break;
	case 2: // Whether the primary wants to Read or Write to the secondary
		dataBuffer[bufferPoiW++] = (i2cBitC ? 'R' : 'W');
		break;
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

/**
 * This is for recognizing I2C START and STOP
 * This is called when the SDA line is changing
 * It is decided inside the function wheather it is a rising or falling change.
 * If SCL is on High then the falling change is a START and the rising is a STOP.
 * If SCL is LOW, then this is the action to set a data bit, so nothing to do.
 */
void IRAM_ATTR i2cTriggerOnChangeSDA()
{
	if (busStableRead(PIN_SCL, 2))
	{
		byte i2cClk = digitalRead(PIN_SCL);
		if (i2cStatus = !I2C_IDLE && i2cClk == 1) // If SCL still HIGH then it is a STOP sign
		{
			i2cStatus = I2C_IDLE;
			bitCount = 0;
			byteCount = 0;
			bufferPoiW--;
			dataBuffer[bufferPoiW++] = 's';
			dataBuffer[bufferPoiW++] = '\n';
		}
		sdaUpCnt++;
	}
	else // FALLING if SDA is LOW
	{
		byte i2cClk = digitalRead(PIN_SCL);
		if (i2cStatus == I2C_IDLE && i2cClk) // If SCL still HIGH than this is a START
		{
			i2cStatus = I2C_TRX;
			bitCount = 0;
			byteCount = 0;
			// Stop condition
			dataBuffer[bufferPoiW++] = 'S';
		}
		sdaDownCnt++;
	}
}

void resetI2cVariable()
{
	i2cStatus = I2C_IDLE;
	bufferPoiW = 0;
	bufferPoiR = 0;
	bitCount = 0;
	falseStart = 0;
}

void processDataBuffer()
{
	if (bufferPoiW == bufferPoiR)
		return;

	uint16_t pw = bufferPoiW;

	// Serial.printf("\nSCL up: %d SDA up: %d SDA down: %d false start: %d\n", sclUpCnt, sdaUpCnt, sdaDownCnt, falseStart);

	for (int i = bufferPoiR; i < pw; i++)
	{
		Serial.write(dataBuffer[i]);
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
		processDataBuffer();
	}
}