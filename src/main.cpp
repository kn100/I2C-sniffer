/**
 * A cleaned up, tidied i2c sniffer, heavily based on work by @WhitehawkTailor
 */

#include <Arduino.h>

#define PIN_SDA 12 
#define PIN_SCL 13

#define I2C_IDLE 0
#define I2C_TRX 2

static volatile byte i2cStatus = I2C_IDLE; // Status of the I2C BUS
static uint32_t lastStartMillis = 0;	   // stoe the last time
static volatile byte dataBuffer[100000];   // Array for storing data of the I2C communication
static volatile uint16_t bufferPoiW = 0;   // points to the first empty position in the dataBufer to write
static uint16_t bufferPoiR = 0;			   // points to the position where to start read from
static volatile byte bitCount = 0;		   // counter of bit appeared on the BUS
static volatile uint16_t byteCount = 0;	   // counter of bytes were writen in one communication.
static volatile byte i2cBitD = 0;		   // Container of the actual SDA bit
static volatile byte i2cBitD2 = 0;		   // Container of the actual SDA bit
static volatile byte i2cBitC = 0;		   // Container of the actual SDA bit
static volatile byte i2cClk = 0;		   // Container of the actual SCL bit
static volatile byte i2cCase = 0;		   // Container of the last ACK value
static volatile uint16_t falseStart = 0;   // Counter of false start events
static volatile uint16_t sclUpCnt = 0;	   // Auxiliary variable to count rising SCL
static volatile uint16_t sdaUpCnt = 0;	   // Auxiliary variable to count rising SDA
static volatile uint16_t sdaDownCnt = 0;   // Auxiliary variable to count falling SDA

void IRAM_ATTR i2cTriggerOnRaisingSCL()
{
	sclUpCnt++;

	if (i2cStatus == I2C_IDLE)
	{
		// Unclear to me what a false start is. Maybe return here.
		falseStart++;
	}

	i2cBitC = digitalRead(PIN_SDA);

	// decide where we are and what to do with incoming data
	i2cCase = 0; // normal case

	if (bitCount == 8) // ACK case
		i2cCase = 1;

	if (bitCount == 7 && byteCount == 0) // R/W if the first address byte
		i2cCase = 2;

	bitCount++;

	switch (i2cCase)
	{
	case 0:										  // normal case
		dataBuffer[bufferPoiW++] = '0' + i2cBitC; // 48
		break;									  // end of case 0 general
	case 1:										  // ACK
		if (i2cBitC)							  // 1 NACK SDA HIGH
		{
			dataBuffer[bufferPoiW++] = '-'; // 45
		}
		else // 0 ACK SDA LOW
		{
			dataBuffer[bufferPoiW++] = '+'; // 43
		}
		byteCount++;
		bitCount = 0;
		break; // end of case 1 ACK
	case 2:
		if (i2cBitC)
		{
			dataBuffer[bufferPoiW++] = 'R'; // 82
		}
		else
		{
			dataBuffer[bufferPoiW++] = 'W'; // 87
		}
		break; // end of case 2 R/W

	} // end of switch
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
	// make sure that the SDA is in stable state
	do
	{
		i2cBitD = digitalRead(PIN_SDA);
		i2cBitD2 = digitalRead(PIN_SDA);
	} while (i2cBitD != i2cBitD2);

	if (i2cBitD) // RISING if SDA is HIGH (1)
	{
		i2cClk = digitalRead(PIN_SCL);
		if (i2cStatus = !I2C_IDLE && i2cClk == 1) // If SCL still HIGH then it is a STOP sign
		{
			// i2cStatus = I2C_STOP;
			i2cStatus = I2C_IDLE;
			bitCount = 0;
			byteCount = 0;
			bufferPoiW--;
			dataBuffer[bufferPoiW++] = 's';	 // 115
			dataBuffer[bufferPoiW++] = '\n'; // 10
		}
		sdaUpCnt++;
	}
	else // FALLING if SDA is LOW
	{
		i2cClk = digitalRead(PIN_SCL);
		if (i2cStatus == I2C_IDLE && i2cClk) // If SCL still HIGH than this is a START
		{
			i2cStatus = I2C_TRX;
			// lastStartMillis = millis();//takes too long in an interrupt handler and caused timeout panic and CPU restart
			bitCount = 0;
			byteCount = 0;
			dataBuffer[bufferPoiW++] = 'S'; // 83 STOP
											// i2cStatus = START;
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

	Serial.printf("\nSCL up: %d SDA up: %d SDA down: %d false start: %d\n", sclUpCnt, sdaUpCnt, sdaDownCnt, falseStart);

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

#ifdef I2CTEST
	pinMode(PIN_SCL, OUTPUT);
	pinMode(PIN_SDA, OUTPUT);
#else
	pinMode(PIN_SCL, INPUT_PULLUP);
	pinMode(PIN_SDA, INPUT_PULLUP);
	resetI2cVariable();

	attachInterrupt(PIN_SCL, i2cTriggerOnRaisingSCL, RISING); // trigger for reading data from SDA
	attachInterrupt(PIN_SDA, i2cTriggerOnChangeSDA, CHANGE);  // for I2C START and STOP
#endif
	Serial.begin(115200);
}

void loop()
{

#ifdef I2CTEST
	digitalWrite(PIN_SCL, HIGH); // 13 SARGA
	digitalWrite(PIN_SDA, HIGH); // 12 KEK
	delay(500);
	digitalWrite(PIN_SCL, HIGH); // 13 SARGA
	digitalWrite(PIN_SDA, LOW);	 // 12 KEK
	delay(500);
#else

	// if it is in IDLE, then write out the databuffer to the serial consol
	if (i2cStatus == I2C_IDLE)
	{
		processDataBuffer();
		Serial.print("\rStart delay    ");
		delay(5000);
		Serial.print("\rEnd delay    ");
		delay(500);
	}

#endif
}