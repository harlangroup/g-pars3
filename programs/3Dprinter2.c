#pragma config(Motor,  motorA,          extruderButton, tmotorEV3_Medium, PIDControl, encoder)
#pragma config(Motor,  motorB,          z_axis,        tmotorEV3_Large, PIDControl, encoder)
#pragma config(Motor,  motorC,          x_axis,        tmotorNXT, PIDControl, encoder)
#pragma config(Motor,  motorD,          y_axis,        tmotorNXT, PIDControl, encoder)
//*!!Code automatically generated by 'ROBOTC' configuration wizard               !!*//

//Opens debug stream
#pragma DebuggerWindows("debugStream");

// Comment out the line below if you want the motors to run
//#define DISABLE_MOTORS

// name of the file it'll be reading
//:filename
const char *fileName = "gcode.txt";

// You need some kind of value here that will never be used in your g-code
const float noParam = -255;

const long EOF = -255;

typedef enum tCmdType
{
	GCMD_NONE,
	GCMD_G1,
	GCMD_G92,
	GCMD_X,
	GCMD_Y,
	GCMD_Z,
	GCMD_E,
	GCMD_F,
} tCmdType;


#ifndef DISABLE_MOTORS
void waitForMotors();
#endif

float calcDeltaDistance(float &currentPosition, float newPosition);
float calcMotorDegrees(float travelDistance, long degToMM);
void moveMotorAxis(tMotor axis, float degrees);

void startSeq();
void endSeq();

tCmdType processesCommand(char *buff, int buffLen, float &cmdVal);
bool readNextCommand(char *cmd, int cmdLen, tCmdType &gcmd, float &x, float &y, float &z, float &e, float &f);
void executeCommand(tCmdType gcmd, float x, float y, float z, float e, float f);
long readLine(long fd, char *buffer, long buffLen);
void handleCommand_G1(float x, float y, float z, float e, float f);
void handleCommand_G92(float x, float y, float z, float e, float f);
long degBuff = 0;

//this is where you specify your starting position
//:startposition
float xAxisPosition = 0;

float yAxisPosition = 0;

float zAxisPosition = 0;

//this is where you specify the degrees to MM so the program can compensate properly
//:degreestomm
long XdegreesToMM = 8;

long YdegreesToMM = 8;

long ZdegreesToMM = 8;

task main(){
	clearDebugStream();
	startSeq();

	float x, y, z, e, f = 0.0;
	long fd = 0;
	char buffer[128];
	long lineLength = 0;

	tCmdType gcmd = GCMD_NONE;

	fd = fileOpenRead(fileName);

	if (fd < 0) // if file is not found/cannot open
	{
		writeDebugStreamLine("Could not open %s", fileName);
		return;
	}

	while (true)
	{
		lineLength = readLine(fd, buffer, 128);
		if (lineLength != EOF)
		{
			// We can ignore empty lines
			if (lineLength > 0)
			{
				// The readNextCommand will only return true if a valid command has been found
				// Comment handling is now done there.
				if (readNextCommand(buffer, lineLength, gcmd, x, y, z, e, f))
					executeCommand(gcmd, x, y, z, e, f);
				// Wipe the buffer by setting its contents to 0
			}
			memset(buffer, 0, sizeof(buffer));
		}
		else
		{
			// we're done
			writeDebugStreamLine("All done!");
			return;
		}
	}
}

#ifndef DISABLE_MOTORS
void waitForMotors(){
	while(getMotorRunning(x_axis) || getMotorRunning(y_axis) || getMotorRunning(z_axis)){
		sleep(1);
	}
}
#endif

// Calculate the distance (delta) from the current position to the new one
// and update the current position
float calcDeltaDistance(float &currentPosition, float newPosition){

	writeDebugStreamLine("calcDeltaDistance(%f, %f)", currentPosition, newPosition);

	float deltaPosition = newPosition - currentPosition;
	writeDebugStreamLine("deltaPosition: %f", deltaPosition);
	currentPosition = newPosition;
	writeDebugStreamLine("Updated currentPosition: %f", currentPosition);
	return deltaPosition;
}

// Calculate the degrees the motor has to turn, using provided gear size
float calcMotorDegrees(float travelDistance, long degToMM)
{
	writeDebugStreamLine("calcMotorDegrees(%f, %f)", travelDistance, degToMM);
	return travelDistance * degToMM;
}

// Wrapper to move the motor, provides additional debugging feedback
void moveMotorAxis(tMotor axis, float degrees)
{
	writeDebugStreamLine("moveMotorAxis: motor: %d, rawDegrees: %f", axis, degrees);
#ifndef DISABLE_MOTORS
	long motorSpeed = 9;
	long roundedDegrees = round(degrees);
	degBuff = degrees - roundedDegrees + degBuff;
	if (roundedDegrees < 0)
	{
		roundedDegrees = abs(roundedDegrees);
		motorSpeed = -9;
	}
	if (degBuff > 1 || degBuff < -1){
		int degBuffRounded = round(degBuff);
		roundedDegrees = roundedDegrees + degBuffRounded;
		degBuff = degBuff - degBuffRounded;
	}
	moveMotorTarget(axis, roundedDegrees, motorSpeed);
#endif
	return;
}

// We're passed a single command, like "G1" or "X12.456"
// We need to split it up and pick the value type (X, or Y, etc) and float value out of it.
tCmdType processesCommand(char *buff, int buffLen, float &cmdVal)
{
	cmdVal = noParam;
	int gcmdType = -1;

	// Anything less than 2 characters is bogus
	if (buffLen < 2)
		return GCMD_NONE;

	switch (buff[0])
	{
	case 'G':
		sscanf(buff, "G%d", &gcmdType);
		switch(gcmdType)
		{
			case 1: return GCMD_G1;
			case 92: return GCMD_G92;
			default: return GCMD_NONE;
		}
	case 'X':
		sscanf(buff, "X%f", &cmdVal); return GCMD_X;

	case 'Y':
		sscanf(buff, "Y%f", &cmdVal); return GCMD_Y;

	case 'Z':
		sscanf(buff, "Z%f", &cmdVal); return GCMD_Z;

	case 'E':
		sscanf(buff, "E%f", &cmdVal); return GCMD_E;

	case 'F':
		sscanf(buff, "F%f", &cmdVal); return GCMD_F;

	default: return GCMD_NONE;
	}
}


// Read and parse the next line from file and retrieve the various parameters, if present
bool readNextCommand(char *cmd, int cmdLen, tCmdType &gcmd, float &x, float &y, float &z, float &e, float &f)
{
	char currCmdBuff[16];
	tCmdType currCmd = GCMD_NONE;
	int currCmdBuffIndex = 0;
	float currCmdVal = 0;

	x = y = z = e = f = noParam;
	writeDebugStreamLine("\n----------    NEXT COMMAND   -------");
	writeDebugStreamLine("Processing: %s", cmd);

	// Clear the currCmdBuff
	memset(currCmdBuff, 0, sizeof(currCmdBuff));

	// Ignore if we're starting with a ";"
	if (cmd[0] == ';')
		return false;

	for (int i = 0; i < cmdLen; i++)
	{
		currCmdBuff[currCmdBuffIndex] = cmd[i];
		// We process a command whenever we see a space or the end of the string, which is always a 0 (NULL)
		if ((currCmdBuff[currCmdBuffIndex] == ' ') || (currCmdBuff[currCmdBuffIndex] == 0))
		{
			currCmd = processesCommand(currCmdBuff, currCmdBuffIndex + 1, currCmdVal);
			// writeDebugStreamLine("currCmd: %d, currCmdVal: %f", currCmd, currCmdVal);
			switch (currCmd)
			{
				case GCMD_NONE: gcmd = GCMD_NONE; return false;
				case GCMD_G1: gcmd = GCMD_G1; break;
				case GCMD_G92: gcmd = GCMD_G92; break;
				case GCMD_X: x = currCmdVal; break;
				case GCMD_Y: y = currCmdVal; break;
				case GCMD_Z: z = currCmdVal; break;
				case GCMD_E: e = currCmdVal; break;
				case GCMD_F: f = currCmdVal; break;
			}
			// Clear the currCmdBuff
			memset(currCmdBuff, 0, sizeof(currCmdBuff));

			// Reset the index
			currCmdBuffIndex = 0;
		}
		else
		{
			// Move to the next buffer entry
			currCmdBuffIndex++;
		}
	}

	return true;
}

// Use parameters gathered from command to move the motors, extrude, that sort of thing
void executeCommand(tCmdType gcmd, float x, float y, float z, float e, float f)
{
	// execute functions inside this algorithm
	switch (gcmd)
	{
		case GCMD_G1:
			handleCommand_G1(x, y, z, e, f);
			break;
		case GCMD_G92:
			handleCommand_G92(x, y, z, e, f);
			break;
		default:
			displayCenteredBigTextLine(1 , "error! :: gcmd value is unknown!");
			break;
	}

}

// Read the file, one line at a time
long readLine(long fd, char *buffer, long buffLen)
{
	long index = 0;
	char c;

	// Read the file one character at a time until there's nothing left
	// or we're at the end of the buffer
	while (fileReadData(fd, &c, 1) && (index < (buffLen - 1)))
	{
		//writeDebugStreamLine("c: %c (0x%02X)", c, c);
		switch (c)
		{
			// If the line ends in a newline character, that tells us we're at the end of that line
			// terminate the string with a \0 (a NULL)
		case '\r': break;
		case '\n': buffer[index] = 0; return index;
			// It's something other than a newline, so add it to the buffer and let's continue
		default: buffer[index] = c; index++;
		}
	}
	// Make sure the buffer is NULL terminated
	buffer[index] = 0;
	if (index == 0)
		return EOF;
	else
		return index;  // number of characters in the line
}

void handleCommand_G1(float x, float y, float z, float e, float f)
{
	writeDebugStreamLine("Handling G1 command");
	float motorDegrees; 	// Amount the motor has to move
	float deltaPosition;	// The difference between the current position and the one we want to move to

	if(x != noParam){
		writeDebugStreamLine("\n----------    X AXIS   -------------");
		deltaPosition = calcDeltaDistance(xAxisPosition, x);
		motorDegrees = calcMotorDegrees(deltaPosition, XdegreesToMM);
		moveMotorAxis(x_axis, motorDegrees);
	}

	if(y != noParam){
		writeDebugStreamLine("\n----------    Y AXIS   -------------");
		deltaPosition = calcDeltaDistance(yAxisPosition, y);
		motorDegrees = calcMotorDegrees(deltaPosition, YdegreesToMM);
		moveMotorAxis(y_axis, motorDegrees);
	}

	if(z != noParam){
		writeDebugStreamLine("\n----------    Z AXIS   -------------");
		deltaPosition = calcDeltaDistance(zAxisPosition, z);
		motorDegrees = calcMotorDegrees(deltaPosition, ZdegreesToMM);
		moveMotorAxis(z_axis, motorDegrees);
	}
#ifndef DISABLE_MOTORS
	waitForMotors();
#endif
}

void handleCommand_G92(float x, float y, float z, float e, float f)
{
	writeDebugStreamLine("Handling G92 command");
	if(x != noParam){
		writeDebugStreamLine("\n----------    X AXIS   -------------");
		xAxisPosition = x;
	}

	if(y != noParam){
		writeDebugStreamLine("\n----------    Y AXIS   -------------");
		yAxisPosition = y;
	}

	if(z != noParam){
		writeDebugStreamLine("\n----------    Z AXIS   -------------");
		zAxisPosition = z;
	}
#ifndef DISABLE_MOTORS
	waitForMotors();
#endif
}



void startSeq(){
	setLEDColor(ledRed);

	displayCenteredTextLine(2, "Made by Xander Soldaat");
	displayCenteredTextLine(4, "and Cyrus Cuenca");
	displayCenteredTextLine(6, "Version 1.0");
	displayCenteredTextLine(8, "http://github.com/cyruscuenca/g-pars3");

	playTone(554, 5);
	sleep(100);
	playTone(554, 5);
	sleep(100);
	playTone(554, 5);
	sleep(100);

	moveMotorTarget(extruderButton, 75, -100);
}

void endSeq(){
	setLEDColor(ledGreen);
	moveMotorTarget(extruderButton, 75, 100);
	playTone(554, 5);
	sleep(1000);
	playTone(554, 5);
	sleep(1000);
	playTone(554, 5);
	sleep(1000);
}
