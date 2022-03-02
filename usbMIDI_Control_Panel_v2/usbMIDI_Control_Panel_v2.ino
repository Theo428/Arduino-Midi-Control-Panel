#include <PCF8574.h>
#include <Wire.h>
#include <USB-MIDI.h>
#include <FastLED_NeoPixel.h>

unsigned int updateMillis;

//Fader Constants
const int AnalogPins[] = {A0, A1, A2, A3, A4};
int AnalogCurrentValues[sizeof(AnalogPins) / sizeof(int)];
  int AnalogValues[sizeof(AnalogPins) / sizeof(int)];
const int ErrorBar = 4;

//Button Constants
const int PCF20Buttons[] = {0, 1, 2, 3, 4};
const int PCF21Buttons[] = {0, 1, 2, 3, 4, 5, 6, 7};

int PCF20LastState[sizeof(PCF20Buttons) / sizeof(int)];
int PCF21LastState[sizeof(PCF21Buttons) / sizeof(int)];

//LED Constants
const int LEDPin = 6;
const int NUMLEDS = (sizeof(PCF20Buttons) / sizeof(int)) + (sizeof(PCF21Buttons) / sizeof(int));

//Midi Constants
const int FaderVolumeIndicatorID = 10;
const int FaderMuteIndicatorID = 20;
const int FaderAssignIndicator = 40;

const int ControlButtonIndicatorID = 30;
const int ControlButtonAssigbnIndicatorID = 50;

bool FadersMuted[sizeof(PCF20Buttons) / sizeof(int)];
bool FadersAssigned[sizeof(PCF20Buttons) / sizeof(int)];

int FaderVolumeValue[sizeof(PCF20Buttons) / sizeof(int)];
long FaderVolumeUpdateMillis[sizeof(PCF20Buttons) / sizeof(int)];

int ControlButtonValues[sizeof(PCF21Buttons) / sizeof(int)];
int ControlButtonsAssigned[sizeof(PCF21Buttons) / sizeof(int)];

const int VolumeShowMillis = 1000;

PCF8574 pcf20(0x20);
PCF8574 pcf21(0x21);

FastLED_NeoPixel<NUMLEDS, LEDPin, NEO_GRB> strip;


USBMIDI_CREATE_DEFAULT_INSTANCE();

void setup() {
  Serial.begin(115200);
  pcf20.begin();
  pcf21.begin();
  MIDI.begin(3);

  strip.begin();

  for (int i = 0; i < sizeof(PCF20Buttons) / sizeof(int); i++)
  {
    PCF20LastState[i] = 0;
    FadersMuted[i] = false;
    FadersAssigned[i] = false;
    FaderVolumeValue[i] = 0;
    FaderVolumeUpdateMillis[i] = 0;
  }
  
  for (int i = 0; i < sizeof(PCF21Buttons) / sizeof(int); i++)
  {
    PCF21LastState[i] = 0;
    ControlButtonValues[i] = 0;
    ControlButtonsAssigned[i] = false;
  }
  
  for (int i = 0; i < sizeof(AnalogPins) / sizeof(int); i++)
  {
    AnalogCurrentValues[i] = 0;
    AnalogValues[i] = analogRead(AnalogPins[i]);
  }
}

void loop() {
  unsigned int currentMillis = millis();

  //update 20 times per second
  if(currentMillis - updateMillis >= 150)
  {
    updateMillis = currentMillis;
    
    //Read Analog values
    for (int i = 0; i < sizeof(AnalogPins) / sizeof(int); i++)
    {
      AnalogValues[i] = (0.5 * AnalogValues[i]) + (0.5 * analogRead(AnalogPins[i]));
      
      if (AnalogValues[i] - ErrorBar > AnalogCurrentValues[i] || AnalogValues[i] + ErrorBar < AnalogCurrentValues[i])
      {
        MIDI.sendControlChange(i, map(AnalogValues[i], 1023, 0, 127, 0), 1);
      }
      
      AnalogCurrentValues[i] = AnalogValues[i];
    }
  
    //Read Button Values
    for (int i = 0; i < sizeof(PCF20Buttons) / sizeof(int); i++)
    {
      int currentState = 1 - pcf20.readButton(PCF20Buttons[i]);
      if(currentState != PCF20LastState[i])
      {
        MIDI.sendControlChange(i, currentState, 2);
        PCF20LastState[i] = currentState;
      }
    }
    
    for (int i = 0; i < sizeof(PCF21Buttons) / sizeof(int); i++)
    {
      int currentState2 =  1 - pcf21.readButton(PCF21Buttons[i]);
      if(currentState2 != PCF21LastState[i])
      {
        MIDI.sendControlChange(i + 10, currentState2, 2);
        PCF21LastState[i] = currentState2;
      }
    }
    
    //update fader LEDs
    for(int i = 0; i < sizeof(PCF20Buttons) / sizeof(int); i++)
    {
      if(!FadersAssigned[i])
      {
        strip.setPixelColor(i, strip.Color(0, 0, 0));
      }
      else if((currentMillis - FaderVolumeUpdateMillis[i] <= VolumeShowMillis) || !FadersMuted[i])
      {
        int redValue = 0;
        int greenValue = 0;
        int blueValue = 0;

        if(FaderVolumeValue[i] < 64)
        {
          redValue    = map(FaderVolumeValue[i], 0, 64,   0,   0);
          greenValue  = map(FaderVolumeValue[i], 0, 64,   0, 255);
          blueValue   = map(FaderVolumeValue[i], 0, 64,   0,   0);
        }
        else
        {
          redValue    = map(FaderVolumeValue[i], 64, 127,   0, 255);
          greenValue  = map(FaderVolumeValue[i], 64, 127, 255, 90);
          blueValue   = map(FaderVolumeValue[i], 64, 127,   0,   0);
        }
        
        strip.setPixelColor(i, strip.Color(redValue, greenValue, blueValue));
      }
      else if(FadersMuted[i])
      {
        strip.setPixelColor(i, strip.Color(255, 0, 0));
      }
    }

    for(int i = 0; i < sizeof(PCF21Buttons) / sizeof(int); i++)
    {
      int IDOffset = sizeof(PCF20Buttons) / sizeof(int);

      switch(i)
      {
        default:
          if(!ControlButtonsAssigned[i])
            strip.setPixelColor(i + IDOffset, strip.Color(0, 0, 0)); 
          else if(ControlButtonValues[i] == 0)
            strip.setPixelColor(i + IDOffset, strip.Color(0, 255, 0));
          else if(ControlButtonValues[i] > 0)
            strip.setPixelColor(i + IDOffset, strip.Color(255, 0, 0));
          break;
      }
    }
    strip.show();
  }

  if (MIDI.read())
  {
    switch(MIDI.getType())      // Get the type of the message we caught
    {
      case midi::ControlChange:
          int MidiControl = MIDI.getData1(); //get control
          int MidiValue = MIDI.getData2(); //get value

          int controlID = String(MidiControl).charAt(0) - '0';
          

          if(MidiControl - FaderVolumeIndicatorID <= 9)
          {
            int LEDID = MidiControl - FaderVolumeIndicatorID;
            FaderVolumeValue[LEDID] = MidiValue;
            FaderVolumeUpdateMillis[LEDID] = currentMillis;
          }
          else if(MidiControl - FaderMuteIndicatorID <= 9)
          {
            int LEDID = MidiControl - FaderMuteIndicatorID;
            FadersMuted[LEDID] = !(MidiValue == 0);
            FaderVolumeUpdateMillis[LEDID] = currentMillis - VolumeShowMillis;
          }
          else if(MidiControl - ControlButtonIndicatorID <= 9)
          {
            int LEDID = MidiControl - ControlButtonIndicatorID;
            ControlButtonValues[LEDID] =  MidiValue;
          }
          else if(MidiControl - FaderAssignIndicator <= 9)
          {
            int LEDID = MidiControl - FaderAssignIndicator;
            FadersAssigned[LEDID] =  !(MidiValue == 0);
          }
          else if(MidiControl - ControlButtonAssigbnIndicatorID <= 9)
          {
            int LEDID = MidiControl - ControlButtonAssigbnIndicatorID;
            ControlButtonsAssigned[LEDID] =  !(MidiValue == 0);
          }
          
          break;
          
      default:
          break;
    }
  }
}
