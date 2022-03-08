/*************************************************************************************************************************************************************************
  Nitin William - VU3GAO September/2021 - Version 1.5
  Easy Bitx HF Mono Band Rigs(80m, 40m, 20m) VFO / BFO sketch with Si5351 and Arduino Nano with 10Mhz IF.
  Hardware VU3SUA VFO/BFO Board with 16X2 LCD
  Code derived from J. CesarSound - ver 1.0 - Dec/2020.
  https://groups.io/g/BITX20/topic/bitx_40_dds_vfo/4104090?p=
  https://blog.wokwi.com/5-ways-to-blink-an-led-with-arduino/
  https://maxpromer.github.io/LCD-Character-Creator/
  https://www.riyas.org/2016/12/a-simple-si5351-vfo-and-bfo-with-s-meter.html
 *************************************************************************************************************************************************************************/

//Libraries
#include <Wire.h>                 //Arduino library
#include <EEPROMex.h>             //https://thijs.elenbaas.net/2012/07/extended-eeprom-library-for-arduino
#include <Rotary.h>               //Ben Buxton https://github.com/brianlow/Rotary
#include <si5351.h>               //Etherkit https://github.com/etherkit/Si5351Arduino
#include <EasyButton.h>           //https://easybtn.earias.me/docs/introduction
#include <LiquidCrystal.h>        //Arduino library


#define fMax  14500000UL          //7200000UL / 14500000UL
#define fMin  14000000UL          //7000000UL / 14000000UL
#define ddsCal 359000
#define LCD_RS  5
#define LCD_E   6
#define LCD_D4  7
#define LCD_D5  8
#define LCD_D6  9
#define LCD_D7  10
#define encoderButtonPin 11
#define bfoAddress 8              //EEPROM location for BFO value


unsigned long vfo = 14150000UL;   //Enter your initial frequency at startup, ex: 7000000 = 7MHz, 14000000 = 14MHz
unsigned long bfo = 9996850UL;    //BFO frequency for 10Mhz filter Bw = 3.2Khz
unsigned long opsfreq = 0UL;      //Operating freq to be generated by Si5351
unsigned long offset = 3200UL;    //IF Offset
unsigned long freqold, fstep;
unsigned long bfoold;
int stp;                          //Frequency step counter
byte tower[8] =
{
  B11111,
  B10101,
  B01110,
  B00100,
  B00100,
  B00100,
  B00100,
  B00000
};
byte blank[8] =
{
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
};
bool  setupmode = false;


Rotary r = Rotary(3, 2);          // Encoder defined for Interrupt Pin 2,3
LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
Si5351 si5351(0x60);              //Si5351 I2C Address 0x60
EasyButton encoderButton (encoderButtonPin, true); //Create encoderButton Object as input with internal pullup resistors


ISR(PCINT2_vect)
{
  char result = r.process();
  if (result == DIR_CW)
    {
      if (! setupmode )
      set_frequency(1);
      else
      set_bfofrequency(1);
    }
  else if (result == DIR_CCW)
    {
      if (! setupmode )
      set_frequency(-1);
      else
      set_bfofrequency(-1);
    }
}


void set_frequency(short dir)
{
  if (dir == 1) vfo = vfo + fstep;
  if (vfo >= fMax) vfo = fMax;                            //Upper tuning limit
  if (dir == -1) vfo = vfo - fstep;
  if (vfo < fMin) vfo = fMin;                             //lower tuning limit
}


void set_bfofrequency(short dir)
{
  if (dir == 1) bfo = bfo + 50;
  if (bfo >= 10003500) bfo = 10003500;                            
  if (dir == -1) bfo = bfo - 50;
  if (bfo < 9996500) bfo = 9996500;
}


void setup()
{
  Serial.begin(57600);
  encoderButton.begin();
  Wire.begin();
  r.begin(true);                                         //Enable the Arduino's internal weak pull-ups for the rotary's pins external pullup
  lcd.begin(16, 2);
  delay (20);
  lcd.createChar(0, tower);
  lcd.createChar(1, blank);
  lcd.clear();
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, ddsCal);       //Initialize Si5351, with 25Mhz Xtal
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_6MA);  //Output current 2MA, 4MA, 6MA or 8MA
  si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_6MA);  //Output current 2MA, 4MA, 6MA or 8MA
  si5351.output_enable(SI5351_CLK0, 1);                  //1 - Enable / 0 - Disable CLK
  si5351.output_enable(SI5351_CLK1, 0);
  si5351.output_enable(SI5351_CLK2, 1);
  si5351.update_status();
  cli();
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();                                                // Enable all interrupts
  display_banner();
  bfoold = EEPROM.readLong(bfoAddress);
  if (bfoold < 9996500 || bfoold > 10003500 )
    {
      EEPROM.updateLong (bfoAddress,bfo);
    }
  else
    {
      bfo = bfoold;
    }
  si5351.set_freq(bfo * 100, SI5351_CLK2);              //Set bfo
  stp = 4;
  setsteps();
  encoderButton.onPressed(setsteps);
  encoderButton.onPressedFor(2000, set_bfo);
  tunegen();
  display_freq();
  display_radix();
}


void loop()
{
  encoderButton.read();
  if (freqold != vfo)
  {
    tunegen();
    freqold = vfo;
  }
  display_freq();
  display_radix();
  flash_heart();
}


void setsteps()
{
  switch (stp)
  {
    case 1: stp = 2; fstep = 1; break;
    case 2: stp = 3; fstep = 10; break;
    case 3: stp = 4; fstep = 100; break;
    case 4: stp = 5; fstep = 1000; break;
    case 5: stp = 1; fstep = 10000; break;
  }
}


void tunegen()
{
  if (vfo >= 14000000)
  {
    lcd.setCursor(2, 1);
    lcd.print("USB");
    opsfreq =  vfo - bfo;                                //Low side LO injection, no side band inversion
    si5351.set_freq(opsfreq * 100, SI5351_CLK0);         //Update operating frequency
  }
  else
  {
    lcd.setCursor(2, 1);
    lcd.print("LSB");
    opsfreq =  vfo + bfo;                                //High side LO injection, Side band inversion
    si5351.set_freq(opsfreq * 100, SI5351_CLK0);         //Update operating frequency
  }
}


void display_freq()
{
  uint16_t f;
  lcd.setCursor(2, 0);
  f = vfo / 1000000;
  if (f < 10)
    lcd.print('0');
  lcd.print(f);
  lcd.print('.');
  f = (vfo % 1000000) / 1000;
  if (f < 100)
    lcd.print('0');
  if (f < 10)
    lcd.print('0');
  lcd.print(f);
  lcd.print('.');
  f = vfo % 1000;
  if (f < 100)
    lcd.print('0');
  if (f < 10)
    lcd.print('0');
  lcd.print(f);
  lcd.print("Hz");
}


void display_radix()
{
  lcd.setCursor(9, 1);
  if (stp == 2) lcd.print("001Hz"); if (stp == 3) lcd.print("010Hz"); if (stp == 4) lcd.print("100Hz");
  if (stp == 5) lcd.print("01KHz"); if (stp == 1) lcd.print("10KHz");
  lcd.setCursor(2, 1);
}


void set_bfo(short dir)
{
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("BFO  SETUP");
  delay (1000);
  do 
    {
      setupmode = true;
      encoderButton.read();
      lcd.setCursor(4, 1);
      if (bfo < 10000000 )lcd.print("0");
      lcd.print(bfo);
      si5351.set_freq(bfo * 100, SI5351_CLK2);
    } while (!encoderButton.isPressed());                             
  si5351.set_freq(bfo * 100, SI5351_CLK2);
  lcd.setCursor(3, 0);
  lcd.print("Saving BFO");
  EEPROM.updateLong (bfoAddress,bfo);
  delay (2000);
  lcd.clear();
  if ( stp == 1 ) stp = 6;
  stp = stp - 1;
  setupmode = false;
}


void display_banner()
{
  lcd.setCursor (3, 0);
  lcd.print("Easy Bitx");
  lcd.setCursor (3, 1);
  if (fMin >= 3000000 && fMin <= 4000000)
    lcd.print("80m  Band");
  if (fMin >= 7000000 && fMin <= 7500000)
    lcd.print("40m  Band");
  if (fMin >= 14000000 && fMin <= 14500000)
    lcd.print("20m  Band");
  delay (3000);
  lcd.clear();
}


void flash_heart()
{
  lcd.setCursor (0, 0);
  lcd.write (byte((millis() / 1000) % 2 ));        //Logic give 0 or 1 as paramtere to function byte()
}
