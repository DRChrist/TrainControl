
#define DCC_PIN    4  // Arduino pin for DCC out 

//Timer frequency is 2MHz for ( /8 prescale from 16MHz )
#define TIMER_SHORT 0x8D  // 58usec pulse length 141 255-141=114
#define TIMER_LONG  0x1B  // 116usec pulse length 27 255-27 =228

unsigned char last_timer=TIMER_SHORT;  // store last timer value
   
unsigned char flag=0;  // used for short or long pulse
unsigned char every_second_isr = 0;  // pulse up or down

// definitions for state machine 
#define PREAMBLE 0    
#define SEPERATOR 1
#define SENDBYTE  2

unsigned char state= PREAMBLE;
unsigned char preamble_count = 16;
unsigned char outbyte = 0;
unsigned char cbit = 0x80;

// variables for throttle
int locoSpeed=116; //hex 74
int dir=1; //forward
int locoAdr=36;   // this is the (fixed) address of the loco
int jkp=0;
// buffer for command
struct Message 
{
   unsigned char data[7];
   unsigned char len;
};

#define MAXMSG  2
// for the time being, use only two messages - the idle msg and the loco Speed msg

struct Message msg[MAXMSG] = 
{ 
    { { 0xFF,     0, 0xFF, 0, 0, 0, 0}, 3},   // idle msg
    { { locoAdr, 0,  0, 0, 0, 0, 0}, 3}    // locoMsg with 128 speed steps
};               // loco msg must be filled later with speed and XOR data byte
                                
int msgIndex=0;  
int byteIndex=0;


//Setup Timer2.
//Configures the 8-Bit Timer2 to generate an interrupt at the specified frequency.
//Returns the time load value which must be loaded into TCNT2 inside your ISR routine.
void SetupTimer2()
{
  //Timer2 Settings: Timer Prescaler /8, mode 0
  //Timmer clock = 16MHz/8 = 2MHz oder 0,5usec
  // - page 206 ATmega328/P
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
 
  //Timer2 Overflow Interrupt Enable - page 211 ATmega328/P   
  TIMSK2 = 1<<TOIE2; 
  //load the timer for its first cycle
  TCNT2=TIMER_SHORT; 
}

//Timer2 overflow interrupt vector handler
ISR(TIMER2_OVF_vect) 
{
  //Capture the current timer value TCTN2. This is how much error we have
  //due to interrupt latency and the work in this function
  //Reload the timer and correct for latency.  
  unsigned char latency;
  
  // for every second interupt just toggle signal
  if (every_second_isr)  
  {
     digitalWrite(DCC_PIN,1);
     every_second_isr = 0;    
     
     // set timer to last value
     latency=TCNT2;
     TCNT2=latency+last_timer; 
     
  }
  else  
  {  // != every second interrupt, advance bit or state
     digitalWrite(DCC_PIN,0);
     every_second_isr = 1; 
     
     switch(state)  
     {
       case PREAMBLE:
           flag=1; // short pulse
           preamble_count--;
           if (preamble_count == 0)  
           {  // advance to next state
              state = SEPERATOR;
              // get next message
              msgIndex++;
              if (msgIndex >= MAXMSG)  
              {  
                msgIndex = 0; 
              }  
              byteIndex = 0; //start msg with byte 0
           }
           break;
        case SEPERATOR:
           flag=0; // long pulse
           // then advance to next state
           state = SENDBYTE;
           // goto next byte ...
           jkp=2;
           cbit = 0x80;  // send this bit next time first         
           outbyte = msg[msgIndex].data[byteIndex];
           break;
        case SENDBYTE:
           if (outbyte & cbit)  
           { 
              flag = 1;  // send short pulse
           }
           else  
           {
              flag = 0;  // send long pulse
           }
           cbit = cbit >> 1;
           if (cbit == 0)  
           {  // last bit sent, is there a next byte?
              //Serial.print(" ");
              byteIndex++;
              jkp=2;
              if (byteIndex >= msg[msgIndex].len)  
              {
                 // this was already the XOR byte then advance to preamble
                 state = PREAMBLE;
                 preamble_count = 16;
                 //Serial.println();
                 jkp=1;
              }
              else  
              {
                 // send separtor and advance to next byte
                 state = SEPERATOR ;
              }
           }
           break;
     }   
 
     if (flag)  
     {  // if data==1 then short pulse
        latency=TCNT2;
        TCNT2=latency+TIMER_SHORT;
        last_timer=TIMER_SHORT;
        Serial.print('1');
     }  
     else  
     {   // long pulse
        latency=TCNT2;
        TCNT2=latency+TIMER_LONG; 
        last_timer=TIMER_LONG;
        Serial.print('0');
     } 
    if(jkp==1)
    {
      Serial.println();
      jkp=0;
    } 
    if(jkp==2)
    {
      Serial.print(" ");
      jkp=0;
      
    }
  }
}

void setup(void) 
{
  Serial.begin(115200);
  //Set the pins for DCC to "output".
  pinMode(DCC_PIN,OUTPUT);   // this is for the DCC Signal
  
  assemble_dcc_msg();
  //Start the timer 
  SetupTimer2();   
}

void loop(void) 
{
  //locoSpeed=127; //hex 7F
  delay(200);
  assemble_dcc_msg();
}



void assemble_dcc_msg() 
{
   int i;
   unsigned char data, xdata;
   data = locoSpeed;
   
   xdata = msg[1].data[0] ^ data;
   noInterrupts();  // make sure that only "matching" parts of the message are used in ISR
   msg[1].data[1] = data;
   msg[1].data[2] = xdata;
   
   interrupts();
}

