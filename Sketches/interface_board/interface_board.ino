// Primo - Cubetto Playset
//    Bare-bones code for the Interface

// Sends a sequence of movement commands by radio, when the user-button is
// pressed.

// This code includes a mechanism for Interface/Cubetto Robot to pair.
// When a Cubetto Robot is powered-on, it is unpaired and will accept input from
// any Interface.  When an Interface is powered on, it generates a 32-bit random
// number to use as its session Unique ID (UID).
// Every radio message that an Interface sends includes this UID.
// A Cubetto Robot, on receiving its first radio message, records this UID,
// and subsequently ignores messages from any other UID.
// A devices's UID is forgotten when it is powered off.

// Normal usage of Interface/Cubetto Robot:
// 1. Power-on Cubetto Robot.
// 2. Send a message from the Interface.
// 3. These devices are now paired.
// (4). Repeat 1 & 2 for any other Interfaces & Cubetto Robots.
// (5). Power-off a Cubetto Robot to un-pair (the Interface is unaware of
//      pairings, so no need to power-off).

////////////////////////////////////////////////////////////////////////////////

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include <MCP23S17.h>

#include "Primo.h"


// define magnetic switches
#define PRIMO_MAGNET_NONE     0
#define PRIMO_MAGNET_FORWARD  1
#define PRIMO_MAGNET_RIGHT    2
#define PRIMO_MAGNET_LEFT     3
#define PRIMO_MAGNET_FUNCTION 4
#define PRIMO_MAGNET_ERR      5

#define PRIMO_LED_ON  0
#define PRIMO_LED_OFF 1

void writeLed(uint8_t ledNumber, uint8_t ledStatus);
char check_button(char button_to_check);

// define SS pins for GPIO expanders
#define PRIMO_GPIOEXP1_SS_PIN 9
#define PRIMO_GPIOEXP2_SS_PIN 10
#define PRIMO_GPIOEXP3_SS_PIN 11
#define PRIMO_GPIOEXP4_SS_PIN 12

// Instantiate Mcp23s17 objects
MCP23S17 gpioExp1(&SPI, PRIMO_GPIOEXP1_SS_PIN, 0);
MCP23S17 gpioExp2(&SPI, PRIMO_GPIOEXP2_SS_PIN, 0);
MCP23S17 gpioExp3(&SPI, PRIMO_GPIOEXP3_SS_PIN, 0);
MCP23S17 gpioExp4(&SPI, PRIMO_GPIOEXP4_SS_PIN, 0);

// Remove // comments from following line to enable debug tracing.
#define PRIMO_DEBUG_MODE 1

#ifdef PRIMO_DEBUG_MODE
#define debugPrintf printf
#else
#define debugPrintf(...) ((void) 0)
#endif

//
// Hardware configuration
//

// The Olimex 32u4 Leanoardo board user-button is defined here, because the original Leonardo
// board doesn't have a button by default and there is no pin definition for port E2 in the pins_arduino.h header
#define PRIMO_BBIT (PIND & B00000001) != 0    // Check if the button has been pressed 
#define PRIMO_BUTTONINPUT DDRD &= B11111110   // Initialize the port
// A better solution would be to have the user-button acting through an interrupt,
// as this would allow the Atmel uC (and the nRF24l01) to sleep between button-presses, and conserve battery power.

// Set up nRF24L01 radio on SPI bus plus pins 7 (CE) & 8 (CSN).
RF24 radio(7, 8);


//
// Topology
//

// Interrupt handler, check the radio because we got an IRQ
void checkRadio(void);

////////////////////////////////////////////////////////////////////////////////

void setup (void)
{
  // Initialize the user-button.
  PRIMO_BUTTONINPUT;


  //
  // Print preamble
  //

  Serial.begin(57600);
  printf_begin();

  //while (Serial.read() == -1)
  debugPrintf("Cubetto Playset - Interface - Version %s\n\r", PRIMO_CUBETTO_PLAYSET_VERSION);


  //
  // Setup and configure rf radio
  //

  radio.begin();

  // We will be using the Ack Payload feature, so please enable it
  radio.enableAckPayload();

  // Open pipes to other nodes for communication
  radio.openWritingPipe(pipe);

  // Dump the configuration of the rf unit for debugging
  radio.printDetails();

  // Attach interrupt handler to interrupt #1 (using pin 2)
  // on BOTH the sender and receiver
  attachInterrupt(1, checkRadio, FALLING);


  // Set up all the GPIO expanders
  gpioExp1.begin();
  gpioExp2.begin();
  gpioExp3.begin();
  gpioExp4.begin();
  
  gpioExp1.pinMode(3, OUTPUT);
  gpioExp1.pinMode(7, OUTPUT);
  gpioExp1.pinMode(11, OUTPUT);
  gpioExp1.pinMode(15, OUTPUT);
  gpioExp2.pinMode(3, OUTPUT);
  gpioExp2.pinMode(7, OUTPUT);
  gpioExp2.pinMode(11, OUTPUT);
  gpioExp2.pinMode(15, OUTPUT);
  gpioExp3.pinMode(3, OUTPUT);
  gpioExp3.pinMode(7, OUTPUT);
  gpioExp3.pinMode(11, OUTPUT);
  gpioExp3.pinMode(15, OUTPUT);
  gpioExp4.pinMode(3, OUTPUT);
  gpioExp4.pinMode(7, OUTPUT);
  gpioExp4.pinMode(11, OUTPUT);
  gpioExp4.pinMode(15, OUTPUT);
  
  // This code always sends the same movement commands.
  initialise_packet();

  for (int ledLoop = 0; ledLoop < 3; ++ledLoop)
  {
    // Switch all LEDs on, delay for 100ms, then off
    if (ledLoop) delay(100);
    switchAllLedsOn(); 
    delay(100);
    switchAllLedsOff(); 
  }
}

////////////////////////////////////////////////////////////////////////////////

static char current_element;
static char function_element;
static char button;

int hall;
int terminated;
int i;
int movement_delay;

char led_fn_terminate;

#define PRIMO_MAX_BUTTON 28

////////////////////////////////////////////////////////////////////////////////

void loop (void)
{
  // Loop until the user-button pressed.
  //debug_printf("wait for !PRIMO_BBIT");
  //while(!PRIMO_BBIT)
  //{
  //  printf("PIND = %x\n\r", PIND);
  //  delay(1000);
  //}
  //debug_printf("!PRIMO_BBIT finished");

  switchAllLedsOff(); 

  while (PRIMO_BBIT)
  {
    for (button = 1; button <= 16; button++)
    {
      switch (check_button(button))
      {
        case PRIMO_MAGNET_FORWARD:
          writeLed(button, PRIMO_LED_ON);
          break;

        case PRIMO_MAGNET_RIGHT:
          writeLed(button, PRIMO_LED_ON);
          break;

        case PRIMO_MAGNET_LEFT:
          writeLed(button, PRIMO_LED_ON);
          break;     

        case PRIMO_MAGNET_FUNCTION:
          writeLed(button, PRIMO_LED_ON);
          break;

        case PRIMO_MAGNET_NONE:
          writeLed(button, PRIMO_LED_OFF);
          break;

        default:
          break;
      }
    }
  }

  //debug_printf("Now sending packet\n\r");
  //radio.startWrite(packet, NRF24L01_MAX_PACKET_SIZE);
  //debug_printf("Finished sending\n\r");

  // Loop until the user-button is released - put LEDs on as magnets are fitted
  while(!PRIMO_BBIT) {}
 
  //
  // button is released, now calculate the packet to send
  // cycle through all the magnetic buttons, and fill in the packet appropriately
  //


  current_element = 8; // set to first movement element of the packet
  terminated = 0; // set at the first empty position to indicate end of the sequence#
  debugPrintf("hall1 = %X\r\n", gpioExp1.readPort());
  debugPrintf("hall2 = %X\r\n", gpioExp2.readPort());
  debugPrintf("hall3 = %X\r\n", gpioExp3.readPort());
  debugPrintf("hall4 = %X\r\n", gpioExp4.readPort());

  for (button = 1; button <= PRIMO_MAX_BUTTON; button++)
  {
    if (!terminated)
    {
      switch (check_button(button))
      {
        case PRIMO_MAGNET_FORWARD:
          packet[current_element++] = PRIMO_COMMAND_FORWARD;
          break;

        case PRIMO_MAGNET_RIGHT:
          packet[current_element++] = PRIMO_COMMAND_RIGHT;
          break;

        case PRIMO_MAGNET_LEFT:
          packet[current_element++] = PRIMO_COMMAND_LEFT; 
          break;     

        case PRIMO_MAGNET_FUNCTION:
          for (function_element = 1; function_element <= 4; function_element++)
          {
            switch (check_button(12 + function_element))
            {
              case PRIMO_MAGNET_FORWARD:
                packet[current_element++] = PRIMO_COMMAND_FORWARD;
                break;

              case PRIMO_MAGNET_RIGHT:
                packet[current_element++] = PRIMO_COMMAND_RIGHT;
                break;

              case PRIMO_MAGNET_LEFT:
                packet[current_element++] = PRIMO_COMMAND_LEFT; 
                break;

              default:
                break;
            }
          }
          break;

        case PRIMO_MAGNET_NONE:
          terminated = 1;
          break;

        default:
          break;
      }
    }
    else   // terminated
    {
      packet[current_element++] = PRIMO_COMMAND_STOP;
    }
  }

  // we have filled up the packet with commands, add stops to the end
  while (current_element < PRIMO_MAX_BUTTON)
    packet[current_element++] = PRIMO_COMMAND_STOP;

  debugPrintf("Packet: ");

  for (i = 0; i < 24; i++)
  {
    debugPrintf("%x ", packet[i]);
  }


  //TODO - why do I have to send twice? 

  debugPrintf("\n\rNow sending packet\n\r");
  radio.startWrite(packet, PRIMO_NRF24L01_MAX_PACKET_SIZE);
  debugPrintf("Finished sending\n\r");

  debugPrintf("\n\rNow sending packet\n\r");
  radio.startWrite(packet, PRIMO_NRF24L01_MAX_PACKET_SIZE);
  debugPrintf("Finished sending\n\r");

  // start lighting LEDs while we wait for the packet to be processed
  //writeLed(1, PRIMO_LED_ON);
  //delay (500);
  //writeLed(1, PRIMO_LED_OFF);

  terminated = 0;   // set at the first empty position to indicate end of the sequence
  led_fn_terminate = 0;

  for (button = 1; button <= 12; button++)
  {
    if (!terminated)
    {
      switch (check_button(button))
      {
        case PRIMO_MAGNET_FORWARD:
          writeLed(button, PRIMO_LED_OFF);
          movement_delay = 4500; // delay moving forward
          break;

        case PRIMO_MAGNET_RIGHT:
          writeLed(button, PRIMO_LED_OFF);
          movement_delay = 3000; // delay moving right
          break;

        case PRIMO_MAGNET_LEFT:
          writeLed(button, PRIMO_LED_OFF);
          movement_delay = 3000; // delay moving left
          break;     

        case PRIMO_MAGNET_FUNCTION:
          writeLed(button, PRIMO_LED_OFF);
          led_fn_terminate = 0;

          for (function_element = 1; function_element <= 4; function_element++)
          {
            if (!led_fn_terminate)
            {
              if (check_button(12 + function_element) == PRIMO_MAGNET_NONE)
                led_fn_terminate = 1;
              else
              {
                writeLed(12 + function_element, PRIMO_LED_OFF);
                if (check_button(12 + function_element) == PRIMO_MAGNET_FORWARD)
                  delay (4500);
                else
                  delay (3000);   // TODO handle delays during functions properly
              }
            }
          }

          // turn function lights back on after function executed
          for (function_element = 1; function_element <= 4; function_element++)
          {
            if (check_button(12 + function_element) != PRIMO_MAGNET_NONE)
            writeLed(12 + function_element, PRIMO_LED_ON);
          }

          movement_delay = 0;
          break;

        case PRIMO_MAGNET_NONE:
          terminated = 1;
          break;

        default:
          break;
      }

      delay(movement_delay);
    }
  }
      
  switchAllLedsOff(); 

  //delay(1000);   // For user-button de-bounce etc.
}

////////////////////////////////////////////////////////////////////////////////

void initialise_packet (void)
{
  long primo_id = PRIMO_ID;

  // Set the random number that will be used to uniquely identify THIS primo.
  // Note that random() actually returns a pseudo-random sequence.
  // randomSeed() ensures that each device initialises its sequence
  // to a fairly random noise source
  randomSeed(analogRead(0));
  sessionId = random();
  
  // Set the universal Primo ID, and the unique ID for this primo into the packet.
  // These values can be re-used in every packet sent.
  // (The UID could be used to set the packet address in the radio, but this would 
  // make it necessary to un-pair Primo/Cubetto at BOTH ends).
  memcpy(&packet[0], (const char*) &primo_id, 4);
  memcpy(&packet[4], (const char*) &sessionId, 4);

  // Test command to execute a sequence of 16 movement commands,
  // to produce a figure-of-8 pattern.
  packet[8] = PRIMO_COMMAND_FORWARD;
  packet[9] = PRIMO_COMMAND_LEFT;
  packet[10] = PRIMO_COMMAND_FORWARD;
  packet[11] = PRIMO_COMMAND_LEFT;
  packet[12] = PRIMO_COMMAND_FORWARD;
  packet[13] = PRIMO_COMMAND_LEFT;
  packet[14] = PRIMO_COMMAND_FORWARD;
  packet[15] = PRIMO_COMMAND_RIGHT;
  packet[16] = PRIMO_COMMAND_FORWARD;
  packet[17] = PRIMO_COMMAND_RIGHT;
  packet[18] = PRIMO_COMMAND_FORWARD;
  packet[19] = PRIMO_COMMAND_RIGHT;
  packet[20] = PRIMO_COMMAND_FORWARD;
  packet[21] = PRIMO_COMMAND_RIGHT;
  packet[22] = PRIMO_COMMAND_FORWARD;
  packet[23] = PRIMO_COMMAND_LEFT;

  //packet[8] = PRIMO_COMMAND_FORWARD;
  //packet[9] = PRIMO_COMMAND_FORWARD;
  //packet[10] = PRIMO_COMMAND_FORWARD;
  //packet[11] = PRIMO_COMMAND_FORWARD;
  //packet[12] = PRIMO_COMMAND_FORWARD;
  //packet[13] = PRIMO_COMMAND_FORWARD;
  //packet[14] = PRIMO_COMMAND_FORWARD;
  //packet[15] = PRIMO_COMMAND_FORWARD;
  //packet[16] = PRIMO_COMMAND_FORWARD;
  //packet[17] = PRIMO_COMMAND_FORWARD;
  //packet[18] = PRIMO_COMMAND_FORWARD;
  //packet[19] = PRIMO_COMMAND_FORWARD;
  //packet[20] = PRIMO_COMMAND_FORWARD;
  //packet[21] = PRIMO_COMMAND_FORWARD;
  //packet[22] = PRIMO_COMMAND_FORWARD;
  //packet[23] = PRIMO_COMMAND_FORWARD;

  // Ensure any unused positions are empty,
  // as Cubetto doesn't know if this is the end of the list
  // or just a gap before more movement instructions.
  packet[24] = PRIMO_COMMAND_STOP; 
  packet[25] = PRIMO_COMMAND_STOP;
  packet[26] = PRIMO_COMMAND_STOP;
  packet[27] = PRIMO_COMMAND_STOP;
  packet[28] = PRIMO_COMMAND_STOP;
  packet[29] = PRIMO_COMMAND_STOP;
  packet[30] = PRIMO_COMMAND_STOP;
  packet[31] = PRIMO_COMMAND_STOP;
}

////////////////////////////////////////////////////////////////////////////////

char check_button (char button_to_check)
{
  // takes a button number (1-12 for standard buttons, 13-16 for the function buttons)
  
  // returns the function for that button - PRIMO_MAGNET_FORWARD, PRIMO_MAGNET_LEFT, PRIMO_MAGNET_RIGHT, PRIMO_MAGNET_FUNCTION or PRIMO_MAGNET_NONE
  
  // simplest way to process is in a case - only 16 possibilities!
  int hall_response;
  char button, t_button;
  char ret_val;
  char invert_magnet; // used for line 2, as the magnets are inserted 'upside down'
  char twisted; // one of the sensors is twisted - swap a couple of bits

  // debug_printf("Checking button\r\n");

  invert_magnet = 0;
  twisted = 0;

  switch (button_to_check)
  {
    case 1:
      hall_response = gpioExp1.readPort();
      button = (hall_response & 0x0F);
      break;

    case 2:
      hall_response = gpioExp1.readPort();
      button = (hall_response >> 4) & 0x0F;
      break;

    case 3:
      hall_response = gpioExp1.readPort();
      button = (hall_response >> 8) & 0x0F;
      break;

    case 4:
      hall_response = gpioExp1.readPort();
      button = (hall_response >> 12) & 0x0F;
      break;

    case 5:
      hall_response = gpioExp2.readPort();
      button = (hall_response) & 0x0F;
      invert_magnet = 1;
      break;

    case 6:
      hall_response = gpioExp2.readPort();
      button = (hall_response >> 4) & 0x0F;
      invert_magnet = 1;
      break;

    case 7:
      hall_response = gpioExp2.readPort();
      button = (hall_response >> 8) & 0x0F;
      invert_magnet = 1;
      break;

    case 8:
      hall_response = gpioExp2.readPort();
      button = (hall_response >> 12) & 0x0F;
      invert_magnet = 1;
      twisted = 1;
      break;

    case 9:
      hall_response = gpioExp3.readPort();
      button = (hall_response) & 0x0F;
      break;

    case 10:
      hall_response = gpioExp3.readPort();
      button = (hall_response >> 4) & 0x0F;
      break;

    case 11:
      hall_response = gpioExp3.readPort();
      button = (hall_response >> 8) & 0x0F;
      break;

    case 12:
      hall_response = gpioExp3.readPort();
      button = (hall_response >> 12) & 0x0F;
      break;

    case 13:
      hall_response = gpioExp4.readPort();
      button = (hall_response) & 0x0F;
      break;

    case 14:
      hall_response = gpioExp4.readPort();
      button = (hall_response >> 4) & 0x0F;
      break;

    case 15:
      hall_response = gpioExp4.readPort();
      button = (hall_response >> 8) & 0x0F;
      break;

    case 16:
      hall_response = gpioExp4.readPort();
      button = (hall_response >> 12) & 0x0F;
      break;

    default:
      hall_response = 0;
      button = 0;
      break;
    }

    // only bottom 3 bits have hall data
    button = button & 0x07;

    // untwist bits
    if (twisted)
    {
      t_button = ((button & 0x01) + ((button & 0x02) << 1) + ((button & 0x04) >> 1));
      //debug_printf("button %X, t_button is %X\n\r", button, t_button);
      button = t_button; 
    }

    debugPrintf("read button %X, hall = %X, button is %X\n\r", button_to_check, hall_response, button);

    // convert to button codes
    switch (button)
    {
      case 2:
        ret_val = invert_magnet ? PRIMO_MAGNET_FUNCTION : PRIMO_MAGNET_FUNCTION;
        break;
      case 3:
        ret_val = invert_magnet ? PRIMO_MAGNET_RIGHT : PRIMO_MAGNET_FORWARD;
        break;
      case 4:
        ret_val = invert_magnet ? PRIMO_MAGNET_FUNCTION : PRIMO_MAGNET_NONE;
        break;
      case 5:
        ret_val = invert_magnet ? PRIMO_MAGNET_LEFT : PRIMO_MAGNET_LEFT;
        break;
      case 6:
        ret_val = invert_magnet ? PRIMO_MAGNET_FORWARD : PRIMO_MAGNET_RIGHT;
        break;
      case 7:
        ret_val = PRIMO_MAGNET_NONE;
        break;
      default:
        ret_val = PRIMO_MAGNET_NONE;
        break;
    }

  debugPrintf("found button %X\n\r", ret_val);

  return ret_val;
}

////////////////////////////////////////////////////////////////////////////////

void writeLed (uint8_t ledNumber, uint8_t ledStatus)
{
  // ledStatus = PRIMO_LED_ON to turn on, PRIMO_LED_OFF for off
  
  //debug_printf("LED %x - %x\r\n",led_number,onoff);

  switch (ledNumber)
  {
    case 1:
      gpioExp1.digitalWrite(3, ledStatus);
      break;

    case 2:
      gpioExp1.digitalWrite(7, ledStatus);
      break;

    case 3:
      gpioExp1.digitalWrite(11, ledStatus);
      break;

    case 4:
      gpioExp1.digitalWrite(15, ledStatus);
      break;

    case 5:
      gpioExp2.digitalWrite(3, ledStatus);
      break;

    case 6:
      gpioExp2.digitalWrite(7, ledStatus);
      break;

    case 7:
      gpioExp2.digitalWrite(11, ledStatus);
      break;

    case 8:
      gpioExp2.digitalWrite(15, ledStatus);
      break;

    case 9:
      gpioExp3.digitalWrite(3, ledStatus);
      break;

    case 10:
      gpioExp3.digitalWrite(7, ledStatus);
      break;

    case 11:
      gpioExp3.digitalWrite(11, ledStatus);
      break;

    case 12:
      gpioExp3.digitalWrite(15, ledStatus);
      break;

    case 13:
      gpioExp4.digitalWrite(3, ledStatus);
      break;

    case 14:
      gpioExp4.digitalWrite(7, ledStatus);
      break;

    case 15:
      gpioExp4.digitalWrite(11, ledStatus);
      break;

    case 16:
      gpioExp4.digitalWrite(15, ledStatus);
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////

void switchAllLedsOn()
{
  for (int i = 0; i < 16;)
    writeLed(++i, PRIMO_LED_ON);
}

////////////////////////////////////////////////////////////////////////////////

void switchAllLedsOff()
{
  for (int i = 0; i < 16;)
    writeLed(++i, PRIMO_LED_OFF);
}

