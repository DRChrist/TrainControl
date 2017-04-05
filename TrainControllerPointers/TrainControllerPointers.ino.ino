byte packet[41] = {1,1,1,1,1,1,1,1,1,1,1,1,1,0};
byte data[8];
byte trainAddress[8] = {0,0,1,0,0,1,0,0}; byte *train = &trainAddress[0];

byte vStop[5] = {0,0,0,0,0};    byte *v0 = &vStop[0];
byte vStep1[5] = {0,0,0,1,0};   byte *v1 = &vStep1[0];
byte vStep5[5] = {0,0,1,0,0};   byte *v5 = &vStep5[0]; 
byte vStep10[5] = {1,0,1,1,0};  byte *v10 = &vStep10[0];
byte vStep15[5] = {0,1,0,0,1};  byte *v15 = &vStep15[0];
byte vStep20[5] = {1,1,0,1,1};  byte *v20 = &vStep20[0];
byte vStep28[5] = {1,1,1,1,1};  byte *v28 = &vStep28[0];

byte forward = 1;
byte backwards = 0;

int aPin = 10;


byte * buildDataByte(byte dir, byte * v)
{
  data[0] = 0;
  data[1] = 1;
  data[2] = dir;
  for(int i = 0; i < 5; i++)
    {
        data[3+i] = *(v + i);
    }

  return data;
}

byte * buildPacket(byte * address, byte * data)
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
    checksum[i] = address[i] ^ data[i];
  }

  for(int i = 0; i < 8; i++)
  {
    packet[count] = *(checksum + i);
    count++;
  }
  
  packet[count] = 1;
  count++;

  return packet;
}

void setup() 
{
  Serial.begin(9600);
  pinMode(aPin, OUTPUT);
}

void loop() 
{
  byte *forwardData;
  byte *forwardPacket;
  forwardData = buildDataByte(forward, v10);
  forwardPacket = buildPacket(train, forwardData);
  
  for(int i = 0; i < 41; i++)
  {
    int j = *(forwardPacket + i);
    if(j == 1)
    {
      digitalWrite(aPin, HIGH);
      delayMicroseconds(58);
      digitalWrite(aPin, LOW);
      delayMicroseconds(58);
    }
    else
    {
      digitalWrite(aPin, HIGH);
      delayMicroseconds(116);
      digitalWrite(aPin, LOW);
      delayMicroseconds(116 );
    }
  }

}
