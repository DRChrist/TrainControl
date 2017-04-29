#define DCC_PIN    4  // Arduino pin for DCC out 

//Timer frequency is 2MHz for ( /8 prescale from 16MHz )
#define TIMER_SHORT 0x8D  // 58usec pulse length 141 255-141=114
#define TIMER_LONG  0x1B  // 116usec pulse length 27 255-27 =228

unsigned char last_timer=TIMER_SHORT;  // store last timer value
   
unsigned char flag = 0;  // used for short or long pulse
unsigned char every_second_isr = 0;  // pulse up or down
unsigned char oneTimeMsgReady = 0;

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

char inputBuffer[8];
unsigned char state = PREAMBLE;
unsigned char preamble_count = 16;
unsigned char outbyte = 0;
unsigned char cbit = 0x80;
String flChange = "flchange";

unsigned char greenOneAddress = 36; //this is the (fixed) address of the locomotive

unsigned char trainAddress;
unsigned char dirSpeedByte = 79;
int dir = 1; //forward
int headLight = 0;

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
          if(oneTimeMsgReady)
          {
            msgIndex = 0;
          }
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
            if(msgIndex == 0) msgIndex++;
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
      Serial.print('1');
    }
    else
    { // long pulse
      latency = TCNT2;
      TCNT2 = latency + TIMER_LONG;
      last_timer = TIMER_LONG;
      Serial.print('0'); 
    }
    if(jkp == 1)
    {
      Serial.println();
      jkp = 0;
    }
    if(jkp == 2)
    {
      Serial.print(" ");
      jkp = 0;
    }
  }
}

void assemble_dcc_msg()
{
  unsigned char data, checksum;
  data = dirSpeedByte;
  
  checksum = msg[1].data[0] ^ data;
  noInterrupts(); //make sure that only matching parts of the message are used in ISR
  msg[1].data[1] = data;
  msg[1].data[2] = checksum;
  interrupts();
}

void parseInput(char * input)
{  
  if(input[0] == 9)
  {
    dirSpeedByte = 1; //emergency brake
    setAddress(*(input + 2));
  }
  
  dir = input[0];

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
     case 10:
      buildSpeedByte(vStep9);
      break;
     case 15:
      buildSpeedByte(vStep15);
      break;
     case 20:
      buildSpeedByte(vStep19);
      break;
     case 28:
      buildSpeedByte(vStep27);
      break;
     default:
      buildSpeedByte(vStop);
      break;
  }
  if(*(input + 7) == 'f') headLight = 16; //because we add it directly to dirSpeedByte in buildSpeedByte
  
  setAddress(*(input + 5));
}

void setAddress(unsigned char adr)
{
  if(adr == '1')
  {
    trainAddress = greenOneAddress;
  }
  if(adr == '0')
  {
    trainAddress = 0; //for resetting and clearing train memory, and for general broadcast
  }
}

void buildSpeedByte(unsigned char vel)
{
  //takes five bits for the velocity and adds 0 1 and the direction
  if(dir)
  {
    dirSpeedByte = vel + 96 + headLight;
  }
  else
  {
    dirSpeedByte = vel + 64 + headLight;
  }
}

void configureCV29()
{
  msg[0].data[0] = 232;
  msg[0].data[1] = 28;
  msg[0].data[2] = 249;
  oneTimeMsgReady = 1;
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
    String input = Serial.readString(); //accepts input in this pattern: {d ss t} - d is direction, 1 forward, 0 for backwards
    if(flChange.equals(input)) configureCV29();
    input.toCharArray(inputBuffer, sizeof(inputBuffer));    //ss is to numbers for speed, refer to list of vSteps above
    parseInput(inputBuffer);                                //t is trainAddress. 1 is the green one.
  }
  assemble_dcc_msg();
 
}


