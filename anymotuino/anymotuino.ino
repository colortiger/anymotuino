/*
 File: anymotuino.ino
 Abstract: Open Source code of the Arduino Anymote code
 
 Version: 1.0
 
 Disclaimer: IMPORTANT:  This Color Tiger software is supplied to you by Color Tiger
 Inc. ("Color Tiger") in consideration of your agreement to the following
 terms, and your use, installation, modification or redistribution of
 this Color Tiger software constitutes acceptance of these terms.  If you do
 not agree with these terms, please do not use, install, modify or
 redistribute this Color Tiger software.
 
 In consideration of your agreement to abide by the following terms, and
 subject to these terms, Color Tiger grants you a personal, non-exclusive
 license, under Color Tiger's copyrights in this original Color Tiger software (the
 "Color Tiger Software"), to use, reproduce, modify and redistribute the Color Tiger
 Software, with or without modifications, in source and/or binary forms;
 provided that if you redistribute the Color Tiger Software in its entirety and
 without modifications, you must retain this notice and the following
 text and disclaimers in all such redistributions of the Color Tiger Software.
 Neither the name, trademarks, service marks or logos of Color Tiger Inc. may
 be used to endorse or promote products derived from the Color Tiger Software
 without specific prior written permission from Color Tiger.  Except as
 expressly stated in this notice, no other rights or licenses, express or
 implied, are granted by Color Tiger herein, including but not limited to any
 patent rights that may be infringed by your derivative works or by other
 works in which the Color Tiger Software may be incorporated.
 
 The Software is provided by Color Tiger on an "AS IS" basis.  COLOR TIGER
 MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
 THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
 FOR A PARTICULAR PURPOSE, REGARDING THE COLOR TIGER SOFTWARE OR ITS USE AND
 OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
 
 IN NO EVENT SHALL COLOR TIGER BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
 OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
 MODIFICATION AND/OR DISTRIBUTION OF THE COLOR TIGER SOFTWARE, HOWEVER CAUSED
 AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
 STRICT LIABILITY OR OTHERWISE, EVEN IF COLOR TIGER HAS BEEN ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 */
 
#include <SPI.h>
#include <Boards.h>

#define USECPERTICK 26 // we're defining the microseconds per tick to work with 38KHz codes (the most used frequency) - a tick of 26us = 38KHz

// IMPORTANT! You may get this error on compile on the latest Arduino IDE: error: 'TKD2' was not declared in this scope
// The IRremote library is included in the RobotIRremote library shipped with the latest versions of the Arduino IDE
// which causes a conflict (the included version of IRremote is pretty old). Until the Arduino IDE devs fix this
// issue, you can get back to a successful build by deleting the RobotIRremote libary from the Arduino libraries folder,
// then close and reopen the Arduino IDE and rebuild.
#include <IRremote.h>
#include <IRremoteInt.h>
#include <SoftwareSerial.h>

//  --- config ---

// uncomment this to get debug info over the serial monitor
#define DEBUG_LOGS  

#define CODE_BUF_SIZE 200 // the IR code array buffer size
#define FREQ_MULTIPLIER 250 // to save bandwidth, the freq is divided by this value when transmitted OTA
int RECV_PIN = 6;
int BLE_RX_PIN = 8;
int BLE_TX_PIN = 7;

// --- commands ---
#define COMMAND_SEND 1 // the send command
#define COMMAND_READ 4 // the record from IR command

// --- constants ---
#define MODE_WAITING 0  // waiting for next command, idle
#define MODE_SEND 1     // reading a code from BLE to send over IR
#define MODE_TRANSMIT 2 // transmitting over IR
#define MODE_RECORD 3   // recording a code from IR to send over BLE
#define MODE_RETURN 4   // sending a recorded code back to BLE

// IR objects
IRsend irsend;
IRrecv irrecv(RECV_PIN);
SoftwareSerial bt(BLE_RX_PIN, BLE_TX_PIN);

decode_results results; // the results from the code decoding

int mode; // the current mode

struct {
  unsigned int code[CODE_BUF_SIZE]; // the code ticks buffer
  int len; // length of the code
  unsigned int freq; // in kHz
  bool complete; // true if the code read has completed
  int repeat; // the repeat count of the IR signal
  unsigned char firstByte; // first byte, in case it's a multibyte number
} pendingCode;


// =================
// ==== helpers ====
// =================

void resetPendingCode() {
  pendingCode.len = 0;
  pendingCode.freq = 0;
  pendingCode.complete = false;
  pendingCode.repeat = -1;
  pendingCode.firstByte = 0;
}

void goToWaitingMode() {
  mode = MODE_WAITING;
  resetPendingCode();
  
  cli();
  TIMER_CONFIG_NORMAL();

  //Timer2 Overflow Interrupt Enable
  TIMER_DISABLE_INTR;

  TIMER_RESET;
  sei();  // enable interrupts

  results.rawlen = 0;
}

// sends a code to the device via the IR output
void sendCode(uint16_t code[], int count, int repeat) {
  if (repeat == 0) {
    repeat = 1;
  }
  
  Serial.println(repeat);
  
  for (int r = 0; r < repeat; r++) {
   
    digitalWrite(13, HIGH);
   
    irsend.sendRaw(code, count, pendingCode.freq);

#ifdef DEBUG_LOGS
    Serial.print("Transmitted ");
    Serial.print(count);
    Serial.print(" raw codes");
    Serial.println();
#endif
    
//    delay(10); // small delay between codes
    digitalWrite(13, LOW);
  }
}

void writeCodeToBLE() {
  bt.print((char)(38000/FREQ_MULTIPLIER)); // send the frequency (divided by 250 as the standard imposes)
  Serial.print((char)(38000/FREQ_MULTIPLIER));
  Serial.print(",");
  for (int i = 0; i < results.rawlen; i++) {
    int val = results.rawbuf[i];
    if (val > 0x7f) {
      int b1 = (val >> 7) & 0x7f; // skip the last bit
      int b2 = (val & 0x7f);
      char c1 = b1;
      char c2 = b2 & 0x7f;
      c1 = ~c1 + 1;
      bt.print((char)c1);
      bt.print((char)c2);
      Serial.print((int)c1);
      Serial.print(",");
      Serial.print((int)c2);
      Serial.print(",");
    } else {
      bt.print((char)val);
      Serial.print((int)val);
      Serial.print(",");
    }
  }
  bt.print((char)0);
  Serial.print((int)0);
#ifdef DEBUG_LOGS
  Serial.println("sent code to requester");
#endif
}


// =================
// ==== set up =====
// =================

void setup() {
  // Enable serial debug
  Serial.begin(57600);

  bt.begin(9600);
  
  resetPendingCode();
  
  mode = MODE_WAITING;

#ifdef DEBUG_LOGS
  Serial.println("Started up.");
#endif
}


// =================
// === main loop ===
// =================

void loop() {
  if (bt.overflow() && mode == MODE_SEND) {
    while (bt.read() >= 0) {
      ; // consume the stream
    }
#ifdef DEBUG_LOGS
    Serial.println("\nbuffer overflow; resetting...");
#endif
    mode = MODE_WAITING;
    resetPendingCode();
  }
  
//  digitalWrite(13, LOW);
    
  if (MODE_SEND == mode) {
    // read more bytes from the stream
    if (bt.available() > 0) {
      int data = bt.read();
      
      if (pendingCode.freq == 0) {
        // frequency time
        pendingCode.freq = data * FREQ_MULTIPLIER/1000; // transform it to kHz
#ifdef DEBUG_LOGS
        Serial.print("freq=");
        Serial.println(pendingCode.freq);
#endif
      } else if (data != 0) {
        // byte came in, sort it out
        if (!pendingCode.complete) {
          // continue
          if (data > 0x7f && 0 == pendingCode.firstByte) {
            // first part of a multi-byte int
            pendingCode.firstByte = 256 - data; // convert it back to positive
          } else {
            if (pendingCode.firstByte != 0) {
              unsigned int num = (unsigned int)pendingCode.firstByte << 7;
              num += data;
              pendingCode.code[pendingCode.len++] = num * USECPERTICK;
              pendingCode.firstByte = 0;
            } else {
              pendingCode.code[pendingCode.len++] = (unsigned int)data * USECPERTICK;
            }
          }
        } else {
          // even though it's not zero, it might still be the repeat count if the separator went through already
          pendingCode.repeat = data;
        }
      } else {
        if (!pendingCode.complete) {
          // 0 came in, acts as separator
          if (pendingCode.len % 2 != 0) {
            // add the last space if necessary
            pendingCode.code[pendingCode.len++] = 100;
          }
          pendingCode.complete = true;
        } else {
          // code already complete, 1 repeat
          pendingCode.repeat = 1;
        }
      }
      
      if (pendingCode.complete && pendingCode.repeat > 0) {
        // get out of this mode
        mode = MODE_TRANSMIT;
      }
    }

  } // send mode
  // handle the other functional cases
  else if (MODE_TRANSMIT == mode)  {
    // got a code waiting to be sent via ir, take care of it now
    sendCode(pendingCode.code, pendingCode.len, pendingCode.repeat);
    
    // reset the pending code
    goToWaitingMode();
  } else if (MODE_RECORD == mode) {
    
    if (irrecv.decode(&results)) {
      // get into code return mode to make sure nothing stands in our way
      mode = MODE_RETURN;

#ifdef DEBUG_LOGS
      Serial.print("Code recorded with length: ");
      Serial.println(results.rawlen);
#endif

      // pass it along and be done with it
      writeCodeToBLE();
      
      irrecv.resume(); // resume

      // return to idle
      goToWaitingMode();
    }
    
    if (bt.available()) {
#ifdef DEBUG_LOGS
      Serial.println("Command received while in record mode, cancelling...");
#endif

      goToWaitingMode();
      irrecv.resume(); // resume
    }

  } else if (MODE_WAITING == mode) {
    
    // waiting
    digitalWrite(13, HIGH);
    
    if (bt.available() > 0) {
      // a command came in, handle it
      unsigned int command = (unsigned int)bt.read();
#ifdef DEBUG_LOGS
      Serial.print("Received command type: ");
      Serial.println(command);
#endif

      if (COMMAND_SEND == command) {
        mode = MODE_SEND;
      } else if (COMMAND_READ == command) {
        mode = MODE_RECORD;
        irrecv.enableIRIn(); // Re-enable receiver
      }
    }
    
  } 
 
}
