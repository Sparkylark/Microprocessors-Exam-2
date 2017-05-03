#include <hidef.h>      /* common defines and macros */
#include "derivative.h"      /* derivative-specific definitions */
#include "SetClk.h"

//anode representations of 0-9 and A-F
static char anodes[16] = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F,0x77,0x7C,0x39,0x5E,0x79,0x71};
//characters to be displayed (as indexes for the anodes array)
static char values[36] = {0x0,0x0,0x0,0x0,0x0,0xA,0x0,0xA,
                         0xA,0x0,0xA,0x0,0xF,0xE,0x4,0x5,
                         0xA,0xB,0xC,0xD,0xD,0xC,0xB,0xA,
                         0xF,0xE,0xE,0xD,0xD,0xE,0xA,0xD,
                         0xB,0xE,0xE,0xF};
static char cathodes[4] = {0xFE,0xFD,0xFB,0xF7};
int buttonPressed = 0;
int buttonCount = 0;
//static int RTIsirenCount[4] = {16,19,21,42};      //Number of RTI handler calls per tone half-cycle
static int isHi = 0;
static int RTIsirenLoCount[4] = {2,3,3,7}; //first half of tone period (since they can be asymmetrical)
static int RTIsirenHiCount[4] = {3,3,4,7}; //second half of tone period
static int toneTimecount[4] = {1953,7812,11719,15625}; //Count of interrupts until moving to the next tone
int refreshCount = 0;       //count of full cycles of 7-seg refreshing
int RTItimerCount = 0;      //count of RTI interrupts for use in refreshing 7-Segs
int sirenCounter = 0;       //counter of RTI interrupts - compared to RTIsirenCount
int toneTimeCounter = 0;     //counter of half-cycles - compared to toneHCcount
int whichTone = 0;          //selector for current tone
int repeatCount = 0;        //count of full siren repeats (stops at 2)
int currentDisplay = 0;     //next 7-segment display to refresh
int currentData = 0;        //Holds index of currently-displayed values in "values" array
int sysMode = 1;            //0 = Stop, 1 = Run

void main(void) {
  SetClk8();
  DDRH &= 0xFE;   //Set PH0 (SW5) to be an input
  PPSH = 0x00;    //Rising-Edge Polarity
  PIEH = 0x01;    //Enable PH0 interrupt
  DDRB |= 0x7F;   //Set 7-Seg Anodes to output
  DDRP |= 0x0F;   //Set 7-Seg Cathodes to output
  DDRT |= 0x20;   //Set PTT5 for output
  RTICTL = 0x10;  //Set RTI for approx. 128 us
  CRGINT |= 0x80; //Enable RTI
	asm("cli");
	while(1);
}

/*On 8 MHz crystal clock:
  RTI Period = 128 us
    1480 Hz = 2702 cycles per invert
      RTICTL = 0x12 gives 3072 cycles
    1245 Hz = 3212 cycles per invert
      Half-cycle repeats 2467 times for 1 sec elapsed
    1108 Hz = 3610 cycles per invert
      Half-cycle repeats 3348 times for 1.5 sec elapsed
     555 Hz = 7207 cycles per invert
      Half-cycle repeats 2232 times for 2 sec elapsed
*/

void interrupt 7 RTIHandler(){
  CRGFLG = 0x80;  //clear RTI flag
//this section handles button debouncing
if(buttonPressed == 1){
  if(buttonCount == 1200){
    if(sysMode == 1){     //go to stop mode and activate siren
      sysMode--;
      whichTone = 0;
      sirenCounter = 0;
      toneTimeCounter = 0;
      repeatCount = 0;
    }else{                //go to run mode and deactivate siren
      sysMode++;
    }
    buttonPressed = 0; 
  }else buttonCount++;
}
//this section handles siren generation
  if(sysMode == 0 && repeatCount < 2){
    if((isHi == 0 && sirenCounter >= RTIsirenLoCount[whichTone]) || (isHi == 1 && sirenCounter >= RTIsirenHiCount[whichTone])){    //modulate tone at correct frequency
      PTT ^= 0x20;
      sirenCounter = 1;
      if(isHi == 0) isHi++;
      else isHi--;
    }else{
      sirenCounter++;
    }
    if(++toneTimeCounter == toneTimecount[whichTone]){    //go to next tone after the correct amount of half-cycles
      toneTimeCounter=0;
      if(whichTone < 3){
        whichTone++;
      }else{
        whichTone=0;
        repeatCount++;
      }
    }          
  }
  //this section handles refreshing the 7-segment displays and updating the displayed values
  if(refreshCount < 217){    //repeat for approx. 1 second (244 was found to be the iteration count of mimimum error)
      if(RTItimerCount == 8){  //go to next refresh step after ~1 ms.
      RTItimerCount = 0;
      PTP = cathodes[currentDisplay];     //Place current cathode on PTP
      PORTB = anodes[values[currentData*4 + currentDisplay]];    //Place 7-seg representation of current digit on PTB
      if(currentDisplay == 1 || currentDisplay == 2){
        PORTB |= 0x80;	//activate the decimal point on the relevant displays to create a colon
      }
      if(currentDisplay == 3){
        currentDisplay=0;
        if(sysMode == 1){   //if in run mode
          refreshCount++;
        }
     }else
        currentDisplay++;
    }else{
      RTItimerCount++;
    }
  }else if(sysMode == 1){   //go to next character sequence
    refreshCount=0;
    if(currentData < 8){
      currentData++;
    }else{
      currentData=0;
    }
  }
}

void interrupt 25 PH0Handler(){
  PIFH = 0x01;
  buttonPressed = 1;
  buttonCount = 0;   
}      
