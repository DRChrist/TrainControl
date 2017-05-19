#include "Message.h"

#define DCC_PIN    4  // Arduino pin for DCC out

//Timer frequency is 2MHz for ( /8 prescale from 16MHz )
#define TIMER_SHORT 0x8D  // 58usec pulse length 141 255-141=114
#define TIMER_LONG  0x1B  // 116usec pulse length 27 255-27 =228

unsigned char last_timer = TIMER_SHORT;  // store last timer value

unsigned char flag = 0;  // used for short or long pulse
unsigned char every_second_isr = 0;  // pulse up or down
unsigned char oneTimeMsgReady = 0; //used for sending utility messages and messages for lights and switches. Might not work.

// definitions for state machine
#define PREAMBLE 0
#define SEPARATOR 1
#define SENDBYTE  2

unsigned char tableOfLights[28][4] = {
  11,   131,  252,  253,
  12,   131,  254,  255,
  21,   134,  248,  249,
  22,   134,  250,  251,
  31,   136,  252,  253,
  32,   136,  254,  255,
  41,   139,  248,  249,
  42,   139,  250,  251,
  51,   141,  252,  253,
  52,   141,  254,  255,
  61,   144,  248,  249,
  62,   144,  250,  251,
  81,   149,  248,  249,
  82,   149,  250,  251,
  91,   151,  252,  253,
  92,   151,  254,  255,
  101,  154,  248,  249,
  102,  154,  250,  251,
  111,  156,  252,  253,
  112,  156,  254,  255,
  121,  159,  248,  249,
  122,  159,  250,  251,
  131,  161,  252,  253,
  132,  161,  254,  255,
  141,  164,  248,  249,
  142,  164,  250,  251,
  151,  166,  252,  253,
  152,  166,  254,  255
};

char inputBuffer[13];
unsigned char state = PREAMBLE;
unsigned char preamble_count = 16;
unsigned char outbyte = 0;
unsigned char cbit = 0x80;

unsigned char greenOneAddress = 36; //this is the (fixed) address of the locomotive
unsigned char redOneAddress = 40;
unsigned char yellowOneAddress = 8;
unsigned char redTwoAddress = 0; //placeholder
unsigned char redTwoAddress2 = 0; //placeholder

unsigned char trainAddress = 8;
unsigned char trainAddress2 = 0; 
unsigned char dirSpeedByte = 0;
int dir = 0; //backwards

unsigned char lightByteOne = 0;
unsigned char lightByteTwo = 0;


#define MAXMSG  3

struct Message idle = { {0xFF, 0, 0xFF, 0, 0, 0, 0}, 3};

struct Message msg[MAXMSG] =
{
  { {0xFF, 0, 0xFF, 0, 0, 0, 0}, 3},  //used for sending one time messages
  { {greenOneAddress, dirSpeedByte, 0, 0, 0, 0, 0}, 3},    // loco msg must be filled later with speed an XOR data byte
  { {0xFF, 0, 0xFF, 0, 0, 0, 0}, 3} //trying to send this one every other time, see if it fixes addressing bug
};       


int msgIndex = 1;
int byteIndex = 0;

//Setup Timer2
//Configures the 8-Bit Timer2 to generate an interrupt at the specified frequency.
//Returns the time load value which must be loaded into TCNT2 inside your ISR routine.
void SetupTimer2()
{
  //Timer2 Settings: Timer Prescale /8, mode 0
  //Timer clock = 16MHz/8 = 2MHz or 0,5 usec
  // - page 206 ATmega328/p
  TCCR2A = 0;
  TCCR2B = 0<<CS22 | 1<<CS21 | 0<<CS20;
  //         bit 2     bit 1     bit0
  //            0         0         0       Timer/Counter stopped
  //            0         0         1       No Prescaling
  //            0         1         0       Prescaling by 8
  //            0         0         0       Prescaling by 32
  //            1         0         0       Prescaling by 64
  //            1         0         1       Prescaling by 128
  //            1         1         0       Prescaling by 256
  //            1         1         1       Prescaling by 1024

  //Timer2 Overflow Interrupt Enable - page 211 ATmega328/p
  TIMSK2 = 1 << TOIE2;
  //load the timer for its first cycle
  TCNT2 = TIMER_SHORT;
}

//Timer2 overflow interrupt vector handler
//Is called whenever there is an overflow in timer2
//ISR = Interrupt Service Routine
ISR(TIMER2_OVF_vect)
{
  //Capture the current timer value TCNT2. This is how much error we have
  //due to interrupt latency and the work in this function
  //Reload the timer and correct for latency.
  unsigned char latency;

  //for every second interrupt just toggle signal to create the HIGH-part of a full pulse (a 1 or a 0)
  if(every_second_isr)
  {
    digitalWrite(DCC_PIN, 1);
    every_second_isr = 0;

    //set timer to last value
    latency = TCNT2;
    TCNT2 = latency + last_timer;
  }
  else
  { // != every second interrupt, advance bit or state
    digitalWrite(DCC_PIN, 0);
    every_second_isr = 1;

    switch(state)
    {
      case PREAMBLE:
      flag = 1; //short pulse
      preamble_count--;
      if(preamble_count == 0)
      { //advance to next state
        state = SEPARATOR;
        //get next message
        if(oneTimeMsgReady) //seems to work
        {
          msgIndex = 0;
          oneTimeMsgReady = 0; //Reset variable to only send message once. Should it be sent twice?
        }
        byteIndex = 0; //start msg with byte 0
      }
      break;
      case SEPARATOR:
      flag = 0; // long pulse
      //then advance to next state
      state = SENDBYTE;
      // goto next byte
      cbit = 0x80; // send this bit next time first. 0x80 == ( 1 0 0 0 0 0 0 0 )
      outbyte = msg[msgIndex].data[byteIndex];
      break;
      case SENDBYTE:
      if(outbyte & cbit) //separate the bit cbit is at, and if it is a 1 (non-zero), this is true and it sends a 1 (a short pulse)
      {
        flag = 1; //send short pulse
      }
      else //it is a 0 and sends a 0 (a long pulse)
      {
        flag = 0; // send long pulse
      }
      cbit = cbit >> 1; //move cbit one to the right, to iterate through the byte, sending one bit at a time
      if(cbit == 0)
      { //last bit sent, is there a next byte?
        byteIndex++;
        if(byteIndex >= msg[msgIndex].len)
        {
          //this was already the XOR byte then advance to preamble
          state = PREAMBLE;
          preamble_count = 16;
          if(msgIndex == 2)
          {
            msgIndex = 1;
          }
      else
      {
      msgIndex++;
      }
        }
        else
        {
          //Send separator and advance to next byte
          state = SEPARATOR;
        }
      }
      break;
    }

    if(flag)
    { //if data == 1 then short pulse
      latency = TCNT2;
      TCNT2 = latency + TIMER_SHORT;
      last_timer = TIMER_SHORT;
    }
    else
    { // long pulse
      latency = TCNT2;
      TCNT2 = latency + TIMER_LONG;
      last_timer = TIMER_LONG;
    }
  }
}

//int charsToInt(char * arr)
//{
////  int number = 0;
//  int res = 0;
//  Serial.println(sizeof(arr));
//  for(unsigned int i = 0; i < sizeof(arr); i++)
//  {
////    int k = arr[i] - '0';
////    number = number + k * multiplier;
////    Serial.println(number);
////    multiplier = multiplier * 10;
//    res = res*10 + (arr[i] - '0');
//    Serial.println(res);
//  }
//  return res;
//}

void sendOneTimeMessage()
{
  oneTimeMsgReady = 1;
  printMessage(msg[0]);
}

void printMessage(Message msg)
{
  unsigned char cbit = 0x80;
  unsigned char printBit = 0;
  for(int i = 0; i < msg.len; i++)
  {
    Serial.print(msg.data[i]);
    Serial.print(" ");
  }
  Serial.print("   ");
  for(int i = 0;  i < msg.len; i++)
  {
    cbit = 0x80;
    while(cbit != 0)
    {
      printBit = msg.data[i] & cbit;
      if(printBit)
      {
        Serial.print('1');
      }
      else
      {
        Serial.print('0');
      }
      cbit = cbit >> 1;
    }
    Serial.print(" ");
  }
  Serial.println();
}

void assemble_dcc_msg()
{
  msg[1].len = 3;
  unsigned char data;
  unsigned char checksum = 0;
  data = dirSpeedByte;
  for(int i = 0; i < msg[1].len; i++)
  {
    checksum = checksum ^ msg[1].data[i];
  }
//  checksum = msg[1].data[0] ^ data;
  if(msg[1].len == 3)
  {
  noInterrupts(); //make sure that only matching parts of the message are used in ISR
  msg[1].data[0] = trainAddress;
  msg[1].data[1] = data;
  msg[1].data[2] = checksum;
  interrupts();
  }
  else
  {
    noInterrupts();
    msg[1].data[0] = trainAddress;
    msg[1].data[1] = trainAddress2;
    msg[1].data[2] = data;
    msg[1].data[3] = checksum;
    interrupts();
  }
  printMessage(msg[1]);
}

void setLightBytes(int address, char colour)
{
//  unsigned char adr = (char) address;
  for(int i = 0; i < 28; i++)
  {
    for(int j = 0; j < 4; j++)
    {
      if(tableOfLights[i][j] == address)
      {
        lightByteOne = tableOfLights[i][j + 1];
        if(colour == 'r') 
        {
          lightByteTwo = tableOfLights[i][j + 2];
        }
        else
        {
          lightByteTwo = tableOfLights[i][j + 2] + 1;
        }
      }
    }
  }
}


void setAddress(unsigned char adr) //alternative way to set address. May be more stable.
{
  if(adr == '1')
  {
    trainAddress = greenOneAddress;
  }
  else if(adr == '2')
  {
    trainAddress = redOneAddress;
  }
  else if(adr == '3')
  {
    trainAddress = yellowOneAddress;
  }
  else if(adr == '4')
  {
    trainAddress = redTwoAddress;
    trainAddress2 = redTwoAddress2;
    msg[1].len = 4;
  }
  else
  {
    trainAddress = 255;
  }
}

void parseInput(char * input)
{
  int speedNumber = 0;
  
  if(input[0] == '9')
  {
    dirSpeedByte = 1 + 64; //emergency brake
//    trainAddress = getAddress(*(input + 2));
    setAddress(input[2]);
    assemble_dcc_msg();
    return;
  }

  dir = input[0] - '0';
  
  if(input[3] == ' ')//if there is only one digit in speed input
  {
    speedNumber = input[2] - '0' + 1;  
//    trainAddress = getAddress(*(input + 4));
    setAddress(input[4]);
  }
  else//if there are two digits in speed input
  {
    char speedInput[2];
    speedInput[0] = *(input + 2);
    speedInput[1] = *(input + 3);

//    speedNumber = charsToInt(speedInput) + 1;
    speedNumber = atoi(speedInput);
//    String speedIn = String(speedInput);
//    speedNumber = speedIn.toInt() + 1;

//    trainAddress = getAddress(input[5]);
    setAddress(input[5]);
  }
  if(dir)
  {
     dirSpeedByte = speedNumber + 96;
  }
  else
  {
    dirSpeedByte = speedNumber + 64;
  }
  
  assemble_dcc_msg();
}

void headlights(char * input) 
{
  setAddress(input[2]);
  msg[0].data[0] = trainAddress;
  msg[0].data[1] = 144;
  msg[0].data[2] = msg[0].data[0] ^ msg[0].data[1];

  sendOneTimeMessage();
}

void buildAnyMessage(char * input)
{
  int j = 0;
  for(int i = 0; i < msg[0].len; i++)
  {
    char byteToBuild[3];
    byteToBuild[0] = input[j + 2];
    byteToBuild[1] = input[j + 3];
    byteToBuild[2] = input[j + 4];
//    String sByteToBuild = String(byteToBuild);
//    int iByteToBuild = sByteToBuild.toInt();
    int iByteToBuild = atoi(byteToBuild);
//    int iByteToBuild = charsToInt(byteToBuild);
    msg[0].data[i] = iByteToBuild;
    j = j + 2;
  }

  sendOneTimeMessage();
}

void hornOff(char * input)
{
  setAddress(input[2]);
  msg[0].data[0] = trainAddress;
  msg[0].data[1] = 128;
  msg[0].data[2] = msg[0].data[0] ^ msg[0].data[1];

  sendOneTimeMessage();
}

void soundHorn(char * input)
{
  setAddress(input[2]);
  msg[0].data[0] = trainAddress;
  msg[0].data[1] = 130;
  msg[0].data[2] = msg[0].data[0] ^ msg[0].data[1];

  sendOneTimeMessage();
  delay(3000);
  hornOff(input);
}

void bells(char * input)
{
  setAddress(input[2]);
  msg[0].data[0] = trainAddress;
  msg[0].data[1] = 136;
  msg[0].data[2] = msg[0].data[0] ^ msg[0].data[1];

  sendOneTimeMessage();
}

void sendLightCmd(char * input)
{
  char address[3];
  address[0] = input[2];
  address[1] = input[3];
  address[2] = input[4];
//  String sAddress = String(address);
//  int iAddress = sAddress.toInt();
  int iAddress = atoi(address);
//  int iAddress = charsToInt(address);
  setLightBytes(iAddress, input[6]);
  msg[0].data[0] = lightByteOne;
  msg[0].data[1] = lightByteTwo;
  msg[0].data[2] = msg[0].data[0] ^ msg[0].data[1];
  msg[0].len = 3;
  sendOneTimeMessage();
}

void switchOffMsg(char byteOne, char byteTwo)
{
  msg[0].data[0] = byteOne;
  msg[0].data[1] = byteTwo - 8;
  msg[0].data[2] = msg[0].data[0] ^ msg[0].data[1];
  sendOneTimeMessage();
} 

void sendSwitchCmd(char * input)
{
  int byteOne = 0;
  int byteTwo = 0;
  int reg = 0;
  int address = 0;
  char addressArray[3];
  addressArray[0] = input[2];
  addressArray[1] = input[3];
  addressArray[2] = input[4];
//  String sAddress = String(addressArray);
//  int iAddress = sAddress.toInt();
  int iAddress = atoi(addressArray);
//  int iAddress = charsToInt(addressArray);

  address = (iAddress/4) + 1;
  reg = (iAddress%4);
  if(reg == 0)
  {
    reg = 3;
    address = address - 1;
  }
  else
  {
    reg = reg - 1;
  }
  byteOne = (address & 63) + 128;
  msg[0].data[0] = byteOne;
  
  if(!(address & 64)) 
  {
    byteTwo = byteTwo + 16;
  }
  if(!(address & 128)) 
  {
    byteTwo = byteTwo + 32;
  }
  if(!(address & 256)) 
  {
    byteTwo = byteTwo + 64;
  }
  byteTwo = byteTwo + 128;
  byteTwo = byteTwo + (reg << 1);
  byteTwo = byteTwo + 8; //'on' bit
  if(input[6] == 's' || input[6] == 'l') byteTwo = byteTwo + 1; //output bit
  
  msg[0].data[1] = byteTwo;
  msg[0].data[2] = msg[0].data[0] ^ msg[0].data[1];

  sendOneTimeMessage();
  delay(200);

  switchOffMsg(byteOne, byteTwo);
  if(iAddress == 223 || iAddress == 231 || iAddress == 233)
  {
    tripleSwitchMsg(byteOne, byteTwo, input[6]);
  }
}

void tripleSwitchMsg(char byteOne, char byteTwo, char input)
{
  if(input == 'l')//turn left
  {
    byteTwo = byteTwo + 1;
    msg[0].data[0] = byteOne;
    msg[0].data[1] = byteTwo;
    msg[0].data[2] = msg[0].data[0] ^ msg[0].data[1];
    sendOneTimeMessage();
    delay(200);
    switchOffMsg(byteOne, byteTwo);
  }
  else //turn right or run straight
  {
    byteTwo = byteTwo + 2;
    msg[0].data[0] = byteOne;
    msg[0].data[1] = byteTwo;
    msg[0].data[2] = msg[0].data[0] ^ msg[0].data[1];
    sendOneTimeMessage();
    delay(200);
    switchOffMsg(byteOne, byteTwo);
  }
}

void setAllSignalsToGreen()
{
  for(int i = 0; i < 28; i++)
  {
    msg[0].data[0] = tableOfLights[i][1];
    msg[0].data[1] = tableOfLights[i][2] + 1;
    msg[0].data[2] = msg[0].data[0] ^ msg[0].data[1];
    sendOneTimeMessage();
    delay(100);
  }
}

void setAllSignalsToRed()
{
  for(int i = 0; i < 28; i++)
  {
    msg[0].data[0] = tableOfLights[i][1];
    msg[0].data[1] = tableOfLights[i][2];
    msg[0].data[2] = msg[0].data[0] ^ msg[0].data[1];
    sendOneTimeMessage();
    delay(100);
  }
}


void setup()
{
  Serial.begin(115200);
  //set the pins for DCC to "output"
  pinMode(DCC_PIN, OUTPUT); //this is for the DCC signal

  assemble_dcc_msg();
  //Start the timer
  SetupTimer2();
}

void loop()
{
  delay(200);
  while(Serial.available() > 0)
  {
    String input = Serial.readString(); //Protocol: {D SS T}/{D S T} - D is direction, 1 forward, 0 for backwards + extra functions, see below
    input.toCharArray(inputBuffer, sizeof(inputBuffer));    //S is for speed 0-14 //T is trainAddress. Refer to list at the top.
    if(inputBuffer[0] == 'f') 
    {
      headlights(inputBuffer);
    }
    else if(inputBuffer[0] == 'h')
    {
      soundHorn(inputBuffer);
    }
    else if(inputBuffer[0] == 'o')
    {
      hornOff(inputBuffer);
    }
    else if(inputBuffer[0] == 'b')
    {
      bells(inputBuffer);
    }
    else if(inputBuffer[0] == '8')
    {
      buildAnyMessage(inputBuffer); //for sending non-standard messages. Protocol: {8 111 222 333} 111, 222 and 333 must occupy the spaces, so 2 = 002
    }
    else if(inputBuffer[0] == '2')
    {
      sendLightCmd(inputBuffer); //for changing lights. Protocol: {2 AAA C} AAA being the address of the light (see chart) and C being either 'r' or 'g' 
    }
    else if(inputBuffer[0] == '3')
    {
      sendSwitchCmd(inputBuffer); //for changing swithces. Protocol: {3 AAA C} AAA is the address of the switch (see chart) C is 's' for straight (default is turn)
    }
    else if(inputBuffer[0] == 'g')
    {
      setAllSignalsToGreen();
    }
    else if(inputBuffer[0] == 'r')
    {
      setAllSignalsToRed();
    }
    else
    {
      parseInput(inputBuffer);
    }
  }  
}



