

/*
 *@author:        Quinton Pratt  
 *@date:          20 July 2014
 *@script:        Portable FM Radio
 *@version:       1.0
 *mcu:            ATMEGA328P-PU with Arduino Uno Bootloader
 *@email:
 *@url:
 *@copyright:     Quinton Pratt 2014
 *@license:       GNU General Public License Version 3 or later
 *@disclaimer:    THERE IS NO WARRANTY FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW. 
 *                EXCEPT WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES 
 *                PROVIDE THE PROGRAM “AS IS” WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, 
 *                INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 *                FOR A PARTICULAR PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE 
 *                PROGRAM IS WITH YOU. SHOULD THE PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL 
 *                NECESSARY SERVICING, REPAIR OR CORRECTION.
 *@liability:     IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING WILL ANY COPYRIGHT 
 *                HOLDER, OR ANY OTHER PARTY WHO MODIFIES AND/OR CONVEYS THE PROGRAM AS PERMITTED ABOVE, BE 
 *                LIABLE TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL 
 *                DAMAGES ARISING OUT OF THE USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED 
 *                TO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR THIRD 
 *                PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER PROGRAMS), EVEN IF SUCH HOLDER 
 *                OR OTHER PARTY HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 *@attributions:  Si4703 Module code taken from Sparkfun -
 *                https://www.sparkfun.com/datasheets/BreakoutBoards/Si4703_Example.pde
 *
 *                ShiftLCD Code and schematic used from Chris Parish
 *                http://cjparish.blogspot.com/2010/01/controlling-lcd-display-with-shift.html 
 *
 *@files:         Code, eagle and project files can be found at the following github repository:
 *                https://github.com/lach11/portable-fm-player
 *
 *comments:       
 *circuit:        
 *ATMEGA328P        ->          Connection
 *D0                ->          NC
 *D1                ->          DOWN BTN
 *D2                ->          UP BTN
 *D3                ->          SELECT BTN
 *D4                ->          MENU BTN
 *D5                ->          NC
 *D6                ->          74HC595 SHIFT CLOCK PIN
 *D7                ->          74HC595 LATCH PIN
 *D8                ->          74HC595 DATA PIN
 *D9                ->          SI7403 RESET PIN
 *D10               ->          NC
 *D11               ->          NC
 *D12               ->          NC
 *D13               ->          NC
 *A0                ->          NC
 *A1                ->          NC
 *A2                ->          NC
 *A3                ->          NC
 *A4                ->          SI4703 SDA PIN
 *A5                ->          SI4703 SCL PIN
*/

/***************************************************************************************************************
INCLUDES 
***************************************************************************************************************/
#include <Wire.h> //for Si4703
#include <ShiftLCD.h> //for shift register lcd backpack
#include <EEPROM.h>

/***************************************************************************************************************
DEFINES AND MACROS 
***************************************************************************************************************/

//general
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define LOOP_DELAY      150
#define MENU_ROLLOVER   6
#define SPLASH_DELAY    2000

//user buttons
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define MENU_BTN        4
#define SEL_BTN         3
#define UP_BTN          2
#define DOWN_BTN        1

//shift register
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define DATA_PIN        8
#define CLOCK_PIN       6
#define LATCH_PIN       7

//Si4703
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define RST_PIN         9 //mcu pin connected to Si4703 rst pin
#define SDIO            A4
#define SCLK            A5

#define FAIL            0
#define SUCCESS         1
#define SI4703          0x10 //0b._001.0000 = I2C address of Si4703 - note that the Wire function assumes non-left-shifted I2C address, not 0b.0010.000W
#define I2C_FAIL_MAX    10 //This is the number of attempts we will try to contact the device before erroring out

#define IN_EUROPE //Use this define to setup European FM reception. I wuz there for a day during testing (TEI 2011).

#define SEEK_DOWN       0 //Direction used for seeking. Default is down
#define SEEK_UP         1

//Define the register names
#define DEVICEID        0x00
#define CHIPID          0x01
#define POWERCFG        0x02
#define CHANNEL         0x03
#define SYSCONFIG1      0x04
#define SYSCONFIG2      0x05
#define STATUSRSSI      0x0A
#define READCHAN        0x0B
#define RDSA            0x0C
#define RDSB            0x0D
#define RDSC            0x0E
#define RDSD            0x0F

//Register 0x02 - POWERCFG
#define SMUTE           15
#define DMUTE           14
#define SKMODE          10
#define SEEKUP          9
#define SEEK            8

//Register 0x03 - CHANNEL
#define TUNE            15

//Register 0x04 - SYSCONFIG1
#define RDS             12
#define DE              11

//Register 0x05 - SYSCONFIG2
#define SPACE1          5
#define SPACE0          4

//Register 0x0A - STATUSRSSI
#define RDSR            15
#define STC             14
#define SFBL            13
#define AFCRL           12
#define RDSS            11
#define STEREO          8

/***************************************************************************************************************
PROTOTYPE  DECLARATIONS
***************************************************************************************************************/
void io_init(void);

/***************************************************************************************************************
OBJECT INSTANCES
***************************************************************************************************************/
ShiftLCD lcd(DATA_PIN,CLOCK_PIN,LATCH_PIN);

/***************************************************************************************************************
GENERAL VARIABLES
***************************************************************************************************************/
char printBuffer[50];
uint16_t si4703_registers[16]; //There are 16 registers, each 16 bits large
int currentChannel = 1063;
char option;
char vol = 15;
byte current_vol;

/* menu variables */
int menu_mode = 1;
int saved_menu_mode;
int button_return_val;

/* backlight variables */
int backlightMode = 0;
int lightSeconds = 30; 
int channelDisplay;

/* fm station presets */
int raroFM[4] = {899, 919, 930, 1011}; //rarotonga fm stations
int presetTracker = 0;
/***************************************************************************************************************
SETUP ROUTINES 
***************************************************************************************************************/

void setup() {
  lcd.begin(8,2); //initialise 8x2 hd44780 lcd char display
  io_init(); //initialise port pins for button inputs
  si4703_init(); //initialise Si4703 FM module
  gotoChannel(currentChannel);
  currentChannel = readChannel();
  splash_screen(); //show splash screen at start up
} //end setup()

/*********************************************************************************************************
 *@function:       io_init()
 *@about:          Set DDR for tact switches
 *@parameters:     NIL  
 *@returns:        NIL
*********************************************************************************************************/

void io_init() {
  pinMode(MENU_BTN, INPUT); //set menu tact switch line to input
  pinMode(SEL_BTN, INPUT); //set select tact switch line to input
  pinMode(UP_BTN, INPUT); //set up tact switch line to input
  pinMode(DOWN_BTN, INPUT); //set down tact switch line to input
} //end io_init()


/***************************************************************************************************************
MAIN
***************************************************************************************************************/

void loop() {

  //////////////////////////////////////////////////////////////////////////
  
  /* if user presses menu button increment menu mode counter to update display */
  if(digitalRead(MENU_BTN) == HIGH) { //if menu change button pressed
    menu_mode++; //increment menu tracker
    //backlight_init();
    if(menu_mode == MENU_ROLLOVER) menu_mode = 1; //if top menu int exceeded then reset to 1
  } //end if
  //check
  //////////////////////////////////////////////////////////////////////////
  
  /* read any user input and process */
  button_return_val = register_button_press(); //check for btn press
  //check
  //////////////////////////////////////////////////////////////////////////
  
  /* if the up or down button have been press then action input */
  if(button_return_val != 0)  user_input_actions(button_return_val); //process user input
  
  //////////////////////////////////////////////////////////////////////////
  
  /* if menu has changed or btn has been pressed, update display */
  if((menu_mode != saved_menu_mode) || (button_return_val != 0)) render_display();

  //////////////////////////////////////////////////////////////////////////
  
  saved_menu_mode = menu_mode;
  //backlight_decision();
  
  delay(LOOP_DELAY);
  
   //////////////////////////////////////////////////////////////////////////
  
} //end loop()

/***************************************************************************************************************
**************************************************************************************************************
**************************************************************************************************************
ACCESSORY FUNCTIONS 
**************************************************************************************************************
**************************************************************************************************************
***************************************************************************************************************/

/***************************************************************************************************************
BUTTON PROCESSING 
***************************************************************************************************************/

/*********************************************************************************************************
 *@function:       register)button_press()
 *@about:          Polls tact switches for user input and returns an int to indicate the activated btn
 *@parameters:     NIL  
 *@returns:        int
*********************************************************************************************************/

int register_button_press() {
  if(digitalRead(UP_BTN) == HIGH) { //if up button pressed
    return 1;
  } else if(digitalRead(DOWN_BTN) == HIGH) { //if down button pressed
    return 2;
  } else if(digitalRead(SEL_BTN) == HIGH) { //if select buton pressed
    return 3;
  } else {
    return 0; //nothing pressed
  } //end if
} //end function


/*********************************************************************************************************
 *@function:       user_input_actions()
 *@about:          
 *@parameters:     NIL  
 *@returns:        NIL
*********************************************************************************************************/

void user_input_actions(int button_num) {
 
  switch(menu_mode) { //case user button presses according to current menu status
   
    case 1: //main menu 
      //in main menu the menu button toggles menu screens
      //the select button seeks up
      //the up and down buttons control volume up / down respectively
      if(button_num==1) volume_up(); //if up button pressed the increase volume
      if(button_num==2) volume_down(); //if down button pressed then decrease volume
      if(button_num==3) seek(SEEK_UP); //if select button pressed then seek up
      break;
     
    case 2: //seek Menu
      if(button_num==1) seek(SEEK_UP); //if up button pressed then tune up 
      if(button_num==2) seek(SEEK_DOWN); //if down btn pressed then tune down
      break;
      
    case 3: //manual tuning menu
      if(button_num==1) tune_up(); //if up btn pressed then tune up
      if(button_num==2) tune_down(); //if down btn pressed then tune down
      break;
    
    case 4: //volume control menu
      if(button_num==1) volume_up();
      if(button_num==2) volume_down();
      break;
      
    case 5: //presets menu
      if(button_num==1) presetTracker++;
      if(presetTracker>3) presetTracker=0;
      if(button_num==2) presetTracker--;
      if(presetTracker<0) presetTracker=3;
      if(button_num==3) gotoChannel(raroFM[presetTracker]);
    
    default: break;
   
  } //end switch
 
} //end function


/***************************************************************************************************************
DISPLAY RENDERING
***************************************************************************************************************/

/*********************************************************************************************************
 *@function:       render_display()
 *@about:          
 *@parameters:     NIL  
 *@returns:        NIL
*********************************************************************************************************/

void render_display() {
 
  lcd.clear();
  
  channelDisplay = readChannel();
  
  switch(menu_mode) {
   
    case 1: //main menu -> current time | current temp | current status
      lcd.setCursor(0,0);
      lcd.print("Station");
      lcd.setCursor(0,1);
      lcd.print(channelDisplay/10);
      lcd.print(".");
      lcd.print(channelDisplay%10);
      lcd.print("MHz");
      break;
     
    case 2: //seek menu
      lcd.setCursor(0,0);
      lcd.print("Seek");
      lcd.setCursor(0,1);
      lcd.print(channelDisplay/10);
      lcd.print(".");
      lcd.print(channelDisplay%10);
      lcd.print("MHz");
      break;
      
    case 3: //manual tuning menu
      lcd.setCursor(0,0);
      lcd.print("Tune");
      lcd.setCursor(0,1);
      lcd.print(channelDisplay/10);
      lcd.print(".");
      lcd.print(channelDisplay%10);
      lcd.print("MHz");
      break;
    
    case 4: //volume menu
      lcd.setCursor(0,0);
      lcd.print("Volume");
      lcd.setCursor(0,1);
      current_vol = si4703_registers[SYSCONFIG2] & 0x000F; //get current volume (0 to 16)
      if(current_vol<10) {
        lcd.print(current_vol);
        lcd.print(" ");
      } else {
        lcd.print(current_vol);
      } //end if
      break;
    
    case 5: //presets menu
      lcd.setCursor(0,0);
      lcd.print("Preset ");
      lcd.print(presetTracker+1);
      lcd.setCursor(0,1);
      lcd.print(raroFM[presetTracker]/10);
      lcd.print(".");
      lcd.print(raroFM[presetTracker]%10);
      lcd.print("MHz");
      break;
      
    default: break;
   
  } //end switch
 
} //end function


/*********************************************************************************************************
 *@function:       splash_screen()
 *@about:          
 *@parameters:     NIL  
 *@returns:        NIL
*********************************************************************************************************/

void splash_screen() {
  lcd.clear();
  lcd.setCursor(1,0); //go to start of first row
  lcd.print("QMP FM");
  lcd.setCursor(1,1); //go to start of second row
  lcd.print("Player");
  delay(SPLASH_DELAY);
} //end splash_screen()


/***************************************************************************************************************
SI4703 FM MODULE FUNCTIONS
***************************************************************************************************************/

/*********************************************************************************************************
 *@function:       gotoChannel()
 *@about:          
 *@parameters:     NIL  
 *@returns:        NIL
*********************************************************************************************************/

void gotoChannel(int newChannel){
  //Freq(MHz) = 0.200(in USA) * Channel + 87.5MHz
  //97.3 = 0.2 * Chan + 87.5
  //9.8 / 0.2 = 49
  newChannel *= 10; //973 * 10 = 9730
  newChannel -= 8750; //9730 - 8750 = 980

#ifdef IN_EUROPE
    newChannel /= 10; //980 / 10 = 98
#else
  newChannel /= 20; //980 / 20 = 49
#endif

  //These steps come from AN230 page 20 rev 0.5
  si4703_readRegisters();
  si4703_registers[CHANNEL] &= 0xFE00; //Clear out the channel bits
  si4703_registers[CHANNEL] |= newChannel; //Mask in the new channel
  si4703_registers[CHANNEL] |= (1<<TUNE); //Set the TUNE bit to start
  si4703_updateRegisters();

  //delay(60); //Wait 60ms - you can use or skip this delay

  //Poll to see if STC is set
  while(1) {
    si4703_readRegisters();
    if( (si4703_registers[STATUSRSSI] & (1<<STC)) != 0) break; //Tuning complete!
    lcd.setCursor(0,1);
    lcd.print("Tuning..");
    
  } //end while

  si4703_readRegisters();
  si4703_registers[CHANNEL] &= ~(1<<TUNE); //Clear the tune after a tune has completed
  si4703_updateRegisters();

  //Wait for the si4703 to clear the STC as well
  while(1) {
    si4703_readRegisters();
    if( (si4703_registers[STATUSRSSI] & (1<<STC)) == 0) break; //Tuning complete!
    lcd.setCursor(0,1);
    lcd.print("Waiting!");
  } //end while
  lcd.setCursor(0,1);
  lcd.print(readChannel());
} //end gotoChannel()

/*********************************************************************************************************
 *@function:       readChannel()
 *@about:          
 *@parameters:     NIL  
 *@returns:        NIL
*********************************************************************************************************/

int readChannel(void) {
  si4703_readRegisters();
  int channel = si4703_registers[READCHAN] & 0x03FF; //Mask out everything but the lower 10 bits

#ifdef IN_EUROPE
  //Freq(MHz) = 0.100(in Europe) * Channel + 87.5MHz
  //X = 0.1 * Chan + 87.5
  channel *= 1; //98 * 1 = 98 - I know this line is silly, but it makes the code look uniform
#else
  //Freq(MHz) = 0.200(in USA) * Channel + 87.5MHz
  //X = 0.2 * Chan + 87.5
  channel *= 2; //49 * 2 = 98
#endif

  channel += 875; //98 + 875 = 973
  return(channel);
} //end readChannel


/*********************************************************************************************************
 *@function:       ()
 *@about:          
 *@parameters:     NIL  
 *@returns:        NIL
*********************************************************************************************************/

byte seek(byte seekDirection){
  si4703_readRegisters();

  //Set seek mode wrap bit
  //si4703_registers[POWERCFG] |= (1<<SKMODE); //Allow wrap
  si4703_registers[POWERCFG] &= ~(1<<SKMODE); //Disallow wrap - if you disallow wrap, you may want to tune to 87.5 first

  if(seekDirection == SEEK_DOWN) si4703_registers[POWERCFG] &= ~(1<<SEEKUP); //Seek down is the default upon reset
  else si4703_registers[POWERCFG] |= 1<<SEEKUP; //Set the bit to seek up

  si4703_registers[POWERCFG] |= (1<<SEEK); //Start seek

  si4703_updateRegisters(); //Seeking will now start

  //Poll to see if STC is set
  while(1) {
    si4703_readRegisters();
    if((si4703_registers[STATUSRSSI] & (1<<STC)) != 0) break; //Tuning complete!
    lcd.setCursor(0,1);
    channelDisplay = readChannel();
    lcd.print(channelDisplay/10);
    lcd.print(".");
    lcd.print(channelDisplay%10);
    lcd.print("MHz");
    delay(50);
  } //end while

  si4703_readRegisters();
  int valueSFBL = si4703_registers[STATUSRSSI] & (1<<SFBL); //Store the value of SFBL
  si4703_registers[POWERCFG] &= ~(1<<SEEK); //Clear the seek bit after seek has completed
  si4703_updateRegisters();

  //Wait for the si4703 to clear the STC as well
  while(1) {
    si4703_readRegisters();
    if( (si4703_registers[STATUSRSSI] & (1<<STC)) == 0) break; //Tuning complete!
    //Serial.println("Waiting...");
  } //end while

  if(valueSFBL) { //The bit was set indicating we hit a band limit or failed to find a station
    //Serial.println("Seek limit hit"); //Hit limit of band during seek
    return(FAIL);
  } //end if
 
  //Serial.println("Seek complete"); //Tuning complete!
  lcd.setCursor(0,1);
  lcd.print(readChannel());
  return(SUCCESS);
} //end seek()


/*********************************************************************************************************
 *@function:       si4703_init()
 *@about:          
 *@parameters:     NIL  
 *@returns:        NIL
*********************************************************************************************************/

void si4703_init(void) {
  //Serial.println("Initializing I2C and Si4703");
  
  pinMode(RST_PIN, OUTPUT);
  pinMode(SDIO, OUTPUT); //SDIO is connected to A4 for I2C
  digitalWrite(SDIO, LOW); //A low SDIO indicates a 2-wire interface
  digitalWrite(RST_PIN, LOW); //Put Si4703 into reset
  delay(1); //Some delays while we allow pins to settle
  digitalWrite(RST_PIN, HIGH); //Bring Si4703 out of reset with SDIO set to low and SEN pulled high with on-board resistor
  delay(1); //Allow Si4703 to come out of reset

  Wire.begin(); //Now that the unit is reset and I2C inteface mode, we need to begin I2C

  si4703_readRegisters(); //Read the current register set
  si4703_registers[0x07] = 0x8100; //Enable the oscillator, from AN230 page 9, rev 0.61 (works)
  si4703_updateRegisters(); //Update

  delay(500); //Wait for clock to settle - from AN230 page 9

  si4703_readRegisters(); //Read the current register set
  si4703_registers[POWERCFG] = 0x4001; //Enable the IC
  //  si4703_registers[POWERCFG] |= (1<<SMUTE) | (1<<DMUTE); //Disable Mute, disable softmute
  si4703_registers[SYSCONFIG1] |= (1<<RDS); //Enable RDS

#ifdef IN_EUROPE
    si4703_registers[SYSCONFIG1] |= (1<<DE); //50kHz Europe setup
  si4703_registers[SYSCONFIG2] |= (1<<SPACE0); //100kHz channel spacing for Europe
#else
  si4703_registers[SYSCONFIG2] &= ~(1<<SPACE1 | 1<<SPACE0) ; //Force 200kHz channel spacing for USA
#endif

  si4703_registers[SYSCONFIG2] &= 0xFFF0; //Clear volume bits
  si4703_registers[SYSCONFIG2] |= 0x0001; //Set volume to lowest
  si4703_updateRegisters(); //Update

  delay(110); //Max powerup time, from datasheet page 13
} //end si4703_init()


/*********************************************************************************************************
 *@function:       si4703_updateRegisters()
 *@about:          
 *@parameters:     NIL  
 *@returns:        NIL
*********************************************************************************************************/

byte si4703_updateRegisters(void) {

  Wire.beginTransmission(SI4703);
  //A write command automatically begins with register 0x02 so no need to send a write-to address
  //First we send the 0x02 to 0x07 control registers
  //In general, we should not write to registers 0x08 and 0x09
  for(int regSpot = 0x02 ; regSpot < 0x08 ; regSpot++) {
    byte high_byte = si4703_registers[regSpot] >> 8;
    byte low_byte = si4703_registers[regSpot] & 0x00FF;
    Wire.write(high_byte); //Upper 8 bits
    Wire.write(low_byte); //Lower 8 bits
  } //end for

  //End this transmission
  byte ack = Wire.endTransmission();
  if(ack != 0) { //We have a problem! 
    //Serial.print("Write Fail:"); //No ACK!
    //Serial.println(ack, DEC); //I2C error: 0 = success, 1 = data too long, 2 = rx NACK on address, 3 = rx NACK on data, 4 = other error
    return(FAIL);
  } //end if

  return(SUCCESS);
} //end si4703_updateRegisters()


/*********************************************************************************************************
 *@function:       si4703_readRegisters()
 *@about:          
 *@parameters:     NIL  
 *@returns:        NIL
*********************************************************************************************************/

void si4703_readRegisters(void){

  //Si4703 begins reading from register upper register of 0x0A and reads to 0x0F, then loops to 0x00.
  Wire.requestFrom(SI4703, 32); //We want to read the entire register set from 0x0A to 0x09 = 32 bytes.

  while(Wire.available() < 32) ; //Wait for 16 words/32 bytes to come back from slave I2C device
  //We may want some time-out error here

  //Remember, register 0x0A comes in first so we have to shuffle the array around a bit
  for(int x = 0x0A ; ; x++) { //Read in these 32 bytes
    if(x == 0x10) x = 0; //Loop back to zero
    si4703_registers[x] = Wire.read() << 8;
    si4703_registers[x] |= Wire.read();
    if(x == 0x09) break; //We're done!
  } //end for
} //end si4703_readRegisters()


/*********************************************************************************************************
 *@function:       ()
 *@about:          
 *@parameters:     NIL  
 *@returns:        NIL
*********************************************************************************************************/

void si4703_printRegisters(void) {
  //Read back the registers
  si4703_readRegisters();

  //Print the response array for debugging
  for(int x = 0 ; x < 16 ; x++) {
    sprintf(printBuffer, "Reg 0x%02X = 0x%04X", x, si4703_registers[x]);
    //Serial.println(printBuffer);
  } //end for
} //end si4703_printRegisters()


/*********************************************************************************************************
 *@function:       volume_up()
 *@about:          
 *@parameters:     NIL  
 *@returns:        NIL
*********************************************************************************************************/

void volume_up() {
  si4703_readRegisters(); //Read the current register set
  current_vol = si4703_registers[SYSCONFIG2] & 0x000F; //Read the current volume level
  if(current_vol < 16) current_vol++; //Limit max volume to 0x000F
  si4703_registers[SYSCONFIG2] &= 0xFFF0; //Clear volume bits
  si4703_registers[SYSCONFIG2] |= current_vol; //Set new volume
  si4703_updateRegisters(); //Update
} //end volume_up


/*********************************************************************************************************
 *@function:       volume_down()
 *@about:          
 *@parameters:     NIL  
 *@returns:        NIL
*********************************************************************************************************/

void volume_down() {
  si4703_readRegisters(); //Read the current register set
  current_vol = si4703_registers[SYSCONFIG2] & 0x000F; //Read the current volume level
  if(current_vol > 0) current_vol--; //You can't go lower than zero
  si4703_registers[SYSCONFIG2] &= 0xFFF0; //Clear volume bits
  si4703_registers[SYSCONFIG2] |= current_vol; //Set new volume
  si4703_updateRegisters(); //Update
} //end volume_down()


/*********************************************************************************************************
 *@function:       tune_up()
 *@about:          
 *@parameters:     NIL  
 *@returns:        NIL
*********************************************************************************************************/

void tune_up() {
  currentChannel = readChannel();
#ifdef IN_EUROPE
      currentChannel += 1; //Increase channel by 100kHz
#else
      currentChannel += 2; //Increase channel by 200kHz
#endif
      if(currentChannel>1080) currentChannel=875;
      
      gotoChannel(currentChannel);
} //end tune_up()


/*********************************************************************************************************
 *@function:       tune_down()
 *@about:          
 *@parameters:     NIL  
 *@returns:        NIL
*********************************************************************************************************/

void tune_down() {
  currentChannel = readChannel();
#ifdef IN_EUROPE
      currentChannel -= 1; //Decreage channel by 100kHz
#else
      currentChannel -= 2; //Decrease channel by 200kHz
#endif
      if(currentChannel<875) currentChannel=1080;
      gotoChannel(currentChannel);
} //end tune_down()

/***************************************************************************************************************
BACKLIGHT FUNCTIONS 
***************************************************************************************************************/

/*********************************************************************************************************
 *@function:       backlight_init()
 *@about:          
 *@parameters:     NIL  
 *@returns:        NIL
*********************************************************************************************************/

void backlight_init() {
  
} //end backlight_init()


/*********************************************************************************************************
 *@function:       backlight_decision()
 *@about:          
 *@parameters:     NIL  
 *@returns:        NIL
*********************************************************************************************************/

void backlight_decision() {
  
} //end backlight_decision()

