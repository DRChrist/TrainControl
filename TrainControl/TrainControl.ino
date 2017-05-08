#define DCC_PIN    4  // Arduino pin for DCC out

//Timer frequency is 2MHz for ( /8 prescale from 16MHz )
#define TIMER_SHORT 0x8D  // 58usec pulse length 141 255-141=114
#define TIMER_LONG  0x1B  // 116usec pulse length 27 255-27 =228

unsigned char last_timer = TIMER_SHORT;  // store last timer value

unsigned char flag = 0;  // used for short or long pulse
unsigned char every_second_isr = 0;  // pulse up or down
unsigned char oneTimeMsgReady = 0; //used for sending utility messages and messages for lights and switches. Might not work.
unsigned char newMessage = 1; //used for printing only new messages
unsigned char newStandardMessage = 0;

// definitions for state machine
#define PREAMBLE 0
#define SEPARATOR 1
#define SENDBYTE  2

unsigned char vStop = 0;
unsigned char vStep1 = 2;
unsigned char vStep5 = 4;
unsigned char vStep9 = 6;
unsigned char vStep15 = 9;
unsigned char vStep19 = 11;
unsigned char vStep27 = 15;

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

unsigned char trainAddress = 8;
unsigned char dirSpeedByte = 0;
int dir = 1; //forward

unsigned char lightByteOne = 0;
unsigned char lightByteTwo = 0;

int jkp = 0;

struct Message
{
  unsigned char data[7];
  unsigned char len;
};  

#define MAXMSG  2
// for the time being, use only two messages - the idle msg and the loco Speed msg

struct Message idle = { {0xFF, 0, 0xFF, 0, 0, 0, 0}, 3};

struct Message msg[MAXMSG] =
{
  { {0xFF, 0, 0xFF, 0, 0, 0, 0}, 3},  //idle msg
  { {greenOneAddress, dirSpeedByte, 0, 0, 0, 0, 0}, 3}   //locoMsg with 128 speed steps
};        // loco msg must be filled later with speed an XOR data byte


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
//          newMessage = 1;
//          Serial.print("one time msg sent");
        }
//        if(newStandardMessage)
//        {
//          newMessage = 1;
//        }
        byteIndex = 0; //start msg with byte 0
      }
      break;
      case SEPARATOR:
      flag = 0; // long pulse
      //then advance to next state
      state = SENDBYTE;
      // goto next byte
      jkp = 2;
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
        //Serial.print(" ");
        byteIndex++;
        jkp = 2;
        if(byteIndex >= msg[msgIndex].len)
        {
          //this was already the XOR byte then advance to preamble
          state = PREAMBLE;
          preamble_count = 16;
          jkp = 1;
          if(msgIndex == 0)
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
//      if(newMessage) Serial.print('1');
    }
    else
    { // long pulse
      latency = TCNT2;
      TCNT2 = latency + TIMER_LONG;
      last_timer = TIMER_LONG;
//      if(newMessage) Serial.print('0');
    }
//    if(jkp == 1 )//&& newMessage)
//    {
//      Serial.println();
//      jkp = 0;
////      newMessage = 0;
////      newStandardMessage = 0;
//    }
//    if(jkp == 2 )//&& newMessage)
//    {
//      Serial.print(" ");
//      jkp = 0;
//    }
  }
}

void assemble_dcc_msg()
{
  unsigned char data, checksum;
  data = dirSpeedByte;
  
  checksum = msg[1].data[0] ^ data;
  noInterrupts(); //make sure that only matching parts of the message are used in ISR
  msg[1].data[0] = trainAddress;
  msg[1].data[1] = data;
  msg[1].data[2] = checksum;
  newStandardMessage = 1;
  interrupts();
}


unsigned char getLightByteOne(int address)
{
  unsigned char adr = (char) address;
  for(int i = 0; i < 28; i++)
  {
    for(int j = 0; j < 4; j++)
    {
      if(tableOfLights[i][j] == adr)
      {
        return tableOfLights[i][j + 1];
      }
    }
  }
  return '0';
}

unsigned char getLightByteTwo(int address, char colour)
{
  unsigned char adr = (char) address;
  for(int i = 0; i < 28; i++)
  {
    for(int j = 0; j < 4; j++)
    {
      if(tableOfLights[i][j] == adr)
      {
        if(colour == 'r') return tableOfLights[i][j + 2];
        if(colour == 'g') return tableOfLights[i][j + 3];
      }
    }
  }
  return '0';
}

void setLightBytes(int address, char colour)
{
  unsigned char adr = (char) address;
  for(int i = 0; i < 28; i++)
  {
    for(int j = 0; j < 4; j++)
    {
      if(tableOfLights[i][j] == adr)
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


void buildSpeedByte(unsigned char vel)
{
  //takes five bits for the velocity and adds 0 1 and the direction
  if(dir)
  {
    dirSpeedByte = vel + 96;
  }
  else
  {
    dirSpeedByte = vel + 64;
  }

}

int getAddress(unsigned char adr)
{
  if(adr == '1')
  {
    return greenOneAddress;
  }
  else if(adr == '0')
  {
    return 0; //for resetting and clearing train memory, and for general broadcast
  }
  else if(adr == '2')
  {
    return redOneAddress;
  }
  else if(adr == '3')
  {
    return yellowOneAddress;
  }
  else
  {
    return greenOneAddress; //to be changed later. Don't know what to set as default
  }
  
}

void parseInput(char * input)
{
  if(input[0] == '9')
  {
    dirSpeedByte = 1; //emergency brake
    trainAddress = getAddress(*(input + 2));
    Serial.print("EMERGENCY!");
    assemble_dcc_msg();
    return;
  }

  dir = input[0] - '0';

  char speedInput[2];
  speedInput[0] = *(input + 2);
  speedInput[1] = *(input + 3);

  String speedIn = String(speedInput);
  int speedInt = speedIn.toInt();

  switch(speedInt)
  {
    case 0:
    buildSpeedByte(vStop);
    break;
    case 1:
    buildSpeedByte(vStep1);
    break;
    case 5:
    buildSpeedByte(vStep5);
    break;
    case 9:
    buildSpeedByte(vStep9);
    break;
    case 15:
    buildSpeedByte(vStep15);
    break;
    case 19:
    buildSpeedByte(vStep19);
    break;
    case 27:
    buildSpeedByte(vStep27);
    break;
    default:
    buildSpeedByte(vStop);
    break;
  }
  trainAddress = getAddress(*(input + 4));

  assemble_dcc_msg();
}

void headlights(char * input) 
{
  msg[0].data[0] = getAddress(input[2]);
  msg[0].data[1] = 144;
  msg[0].data[2] = msg[0].data[0] ^ msg[0].data[1];

  oneTimeMsgReady = 1;
  newMessage = 1;
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
    String sByteToBuild = String(byteToBuild);
    int iByteToBuild = sByteToBuild.toInt();
    msg[0].data[i] = iByteToBuild;
    j = j + 2;
  }

  oneTimeMsgReady = 1;
  newMessage = 1;
}

void hornOff(char * input)
{
  msg[0].data[0] = getAddress(input[2]);
  msg[0].data[1] = 128;
  msg[0].data[2] = msg[0].data[0] ^ msg[0].data[1];

  oneTimeMsgReady = 1;
  newMessage = 1;
}

void soundHorn(char * input)
{
  msg[0].data[0] = getAddress(input[2]);
  msg[0].data[1] = 130;
  msg[0].data[2] = msg[0].data[0] ^ msg[0].data[1];

  oneTimeMsgReady = 1;
  newMessage = 1;
  delay(3000);
  hornOff(input);
}

void bells(char * input)
{
  msg[0].data[0] = getAddress(input[2]);
  msg[0].data[1] = 136;
  msg[0].data[2] = msg[0].data[0] ^ msg[0].data[1];

  oneTimeMsgReady = 1;
  newMessage = 1;
}

void sendLightCmd(char * input)
{
  char address[3];
  address[0] = input[2];
  address[1] = input[3];
  address[2] = input[4];
  String sAddress = String(address);
  int iAddress = sAddress.toInt();
  msg[0].data[0] = getLightByteOne(iAddress);
  msg[0].data[1] = getLightByteTwo(iAddress, input[6]);
  msg[0].data[2] = msg[0].data[0] ^ msg[0].data[1];
  msg[0].len = 3;
  oneTimeMsgReady = 1;
  newMessage = 1;
}

void altSendLightCmd(char * input)
{
  char address[3];
  address[0] = input[2];
  address[1] = input[3];
  address[2] = input[4];
  String sAddress = String(address);
  int iAddress = sAddress.toInt();
  setLightBytes(iAddress, input[6]);
  msg[0].data[0] = lightByteOne;
  msg[0].data[1] = lightByteTwo;
  msg[0].data[2] = msg[0].data[0] ^ msg[0].data[1];
  msg[0].len = 3;
  oneTimeMsgReady = 1;
  newMessage = 1;
}

//void altAltSendLightCmd(char * input)
//{
//  char address[3];
//  address[0] = input[2];
//  address[1] = input[3];
//  address[2] = input[4];
//  String sAddress = String(address);
//  int iAddress = sAddress.toInt();
//  msg[0].data[0] = (iAddress / 4) + 1 + 128;½
////  if(input[6] == 'r') This is not done. Check the docs to figure out how to write it
////  {
////    msg[0].data[1] = 128 + 64 + 32 + 16 + 8 +(iAddress % 4) - 1;
////  }
////  else
////  {
////    msg[0].data[1] = 128 + 64 + 32 + 16 + 8 +(iAddress % 4) - 1;
//  }
//  msg[0].data[2] = msg[0].data[0] ^ msg[0].data[1];
//  msg[0].len = 3;
//  oneTimeMsgReady = 1;
//  newMessage = 1;
//}

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
    String input = Serial.readString(); //accepts input in this pattern: {d ss t} - d is direction, 1 forward, 0 for backwards (d = 9 changes CV#29, d = 8 allows sending any message)
    input.toCharArray(inputBuffer, sizeof(inputBuffer));    //ss is to numbers for speed, refer to list of vSteps above
    //t is trainAddress. 1 is the green one.
    if(inputBuffer[0] == 'f')//f A 
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
      altSendLightCmd(inputBuffer);
    }
    else
    {
      parseInput(inputBuffer);
    }
  }
  //assemble_dcc_msg();
  
}


