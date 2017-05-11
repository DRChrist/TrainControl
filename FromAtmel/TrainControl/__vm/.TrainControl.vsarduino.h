/* 
	Editor: http://www.visualmicro.com
			visual micro and the arduino ide ignore this code during compilation. this code is automatically maintained by visualmicro, manual changes to this file will be overwritten
			the contents of the Visual Micro sketch sub folder can be deleted prior to publishing a project
			all non-arduino files created by visual micro and all visual studio project or solution files can be freely deleted and are not required to compile a sketch (do not delete your own code!).
			note: debugger breakpoints are stored in '.sln' or '.asln' files, knowledge of last uploaded breakpoints is stored in the upload.vmps.xml file. Both files are required to continue a previous debug session without needing to compile and upload again
	
	Hardware: Arduino/Genuino Uno, Platform=avr, Package=arduino
*/

#define __AVR_ATmega328p__
#define __AVR_ATmega328P__
#define ARDUINO 10801
#define ARDUINO_MAIN
#define F_CPU 16000000L
#define __AVR__
#define F_CPU 16000000L
#define ARDUINO 10801
#define ARDUINO_AVR_UNO
#define ARDUINO_ARCH_AVR

void SetupTimer2();
void sendOneTimeMessage();
void printMessage(Message msg);
void assemble_dcc_msg();
void setLightBytes(int address, char colour);
int getAddress(unsigned char adr);
void setAddress(unsigned char adr);
void parseInput(char * input);
void headlights(char * input);
void buildAnyMessage(char * input);
void hornOff(char * input);
void soundHorn(char * input);
void bells(char * input);
void sendLightCmd(char * input);
void switchOffMsg(char byteOne, char byteTwo);
void sendSwitchCmd(char * input);
//
//

#include "pins_arduino.h" 
#include "arduino.h"
#include "TrainControl.ino"
