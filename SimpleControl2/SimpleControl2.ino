//Protocol: Destination address, direction, speed 
byte *dataByte;
byte *fullPacket;
  
byte packet[41] = {1,1,1,1,1,1,1,1,1,1,1,1,1,0};
byte data[8];
byte allOff[8] = {1,0,0,0,0,0,0,0};
byte greenOneAddress[8] = {0,0,1,0,0,1,0,0}; 

byte * trainAddress;

byte vStop[5] = {0,0,0,0,0};    
byte vStep1[5] = {0,0,0,1,0};  
byte vStep5[5] = {0,0,1,0,0};   
byte vStep10[5] = {1,0,1,1,0}; 
byte vStep15[5] = {0,1,0,0,1};  
byte vStep20[5] = {1,1,0,1,1}; 
byte vStep28[5] = {1,1,1,1,1};  

byte speedByte[5];

byte forward = 1;
byte backwards = 0;
byte directionByte = 0;

int aPin = 4;

char inputBuffer[6];


void buildDataByte(byte dir, byte * v)
{
  data[0] = 0;
  data[1] = 1;
  data[2] = dir;
  for(int i = 0; i < 5; i++)
    {
        data[3+i] = *(v + i);
    }
//  Serial.println("Databyte build");
    buildPacket(greenOneAddress, data);
}

void buildPacket(byte * address, byte * data)
{
  int count = 14;
  
  for(int i = 0; i < 8; i++)
  {
    packet[count] = *(address + i);
    count++;
  }

  packet[count] = 0;
  count++;

  for(int i = 0; i < 8; i++)
  {
    packet[count] = *(data + i);
    count++;
  }

  packet[count] = 0;
  count++;

  byte checksum[8];
  for(int i = 0; i < 8; i++)
  {
      if(address[i] != data[i])
      {
          checksum[i] = 1;
      }
      else
      {
          checksum[i] = 0;
      }
  }

  for(int i = 0; i < 8; i++)
  {
    packet[count] = *(checksum + i);
    count++;
  }
  
  packet[count] = 1;
  count++;

//  Serial.println("PacketBuild");
  sendPacket(packet);
}

void sendPacket(byte * packet)
{

    for(int i = 0; i < 41; i++)
  {
    int j = packet[i];
    if(j == 1)
    {
      digitalWrite(aPin, HIGH);
      delayMicroseconds(58);
      digitalWrite(aPin, LOW);
      delayMicroseconds(58);
//      Serial.print("1,");
    }
    else
    {
      digitalWrite(aPin, HIGH);
      delayMicroseconds(116);
      digitalWrite(aPin, LOW);
      delayMicroseconds(116);
//      Serial.print("0,");
    }
  }
//  Serial.println("packet sent");
}

void parseInput(char * input)
{  
  directionByte = input[0];

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
      buildSpeedByte(vStep10);
      break;
     case 15:
      buildSpeedByte(vStep15);
      break;
     case 20:
      buildSpeedByte(vStep20);
      break;
     case 28:
      buildSpeedByte(vStep28);
      break;
     default:
      buildSpeedByte(vStop);
      break;
  }
  
  if(*(input + 5) == '1')
  {
    trainAddress = &greenOneAddress[0]; //Doesn't work
  }
  buildDataByte(directionByte, speedByte);
}

void buildSpeedByte(byte * vByte)
{
  for(int i = 0; i < 5; i++)
  {
    speedByte[i] = *(vByte +i);
  }
}

void setup() 
{
  pinMode(aPin, OUTPUT);
  Serial.begin(9600);
//  buildDataByte(backwards, vStep20);
//  buildPacket(trainAddress, dataByte);
//  sendPacket(fullPacket);
  Serial.println("Setup");
}

void loop() 
{ 
  while(Serial.available() > 0)
  {
    String input = Serial.readString();

    input.toCharArray(inputBuffer, sizeof(inputBuffer));
    parseInput(inputBuffer);
  }
  //idle signal
  digitalWrite(aPin, HIGH);
  delayMicroseconds(116);
  digitalWrite(aPin, LOW);
  delayMicroseconds(116);
}
