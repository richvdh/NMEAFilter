// NMEA Filter
//
// Listens for NMEA sentences, and passes them through, with the exception of
// AIS sentences matching a given MMSI, which are filtered out.

// the MMSI to be discarded
const uint32_t discardMmsi=235104537;

// main NMEA I/O: the hardware serial interface on pins 1(TX) / 2(RX) of the Nano Every.
Stream &nmeaPort = Serial1;

// diagnostic port: writes debug data.
// the USB serial interface.
Stream &diagnosticPort = Serial;


#define INPUT_BUF_SIZE 200
char input_buf[INPUT_BUF_SIZE]; // buffer for NMEA sentences
int input_index = 0;


// time to turn off the led
bool led_is_on = false;
unsigned long led_off_micros = 0;



void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(38400);
  Serial1.begin(38400);
}

void loop() {
  if( led_is_on && long(micros() - led_off_micros) >= 0) {
    digitalWrite(LED_BUILTIN, 0);
    led_is_on = false;
  }
  while (nmeaPort.available()) {
    char in_char = (char)nmeaPort.read();
    if (in_char == '\n') {
      input_buf[input_index] = '\0';
      handleCommand();
      input_index = 0;
    } else {
      // always leave room for a '\0'
      if(input_index < INPUT_BUF_SIZE-1) {
         input_buf[input_index++] = in_char;        
      }
    }
  } 
}

void handleCommand() {
  if ((input_buf[0] != '$' && input_buf[0] != '!') || input_index < 9) {
    diagnosticPort.print("Ignoring invalid input: ");
    diagnosticPort.println(input_buf);
    return;
  }

  if (!checkChecksum(input_buf)) {
    diagnosticPort.print("Ignoring input with bad checksum: ");
    diagnosticPort.println(input_buf);
    return;
  }

  diagnosticPort.println(input_buf);

  // if it's a !..VDO or !..VDM sentence, process it
  if (input_buf[0] == '!') {
    if (strncmp(input_buf+3, "VDO", 3) == 0 || strncmp(input_buf+3, "VDM", 3) == 0) {
      handleAISCommand();
      return;
    }
  }

  // otherwise, just copy it out.
  writeMessage(input_buf);
}


void writeMessage(const char *p) {
  nmeaPort.println(p);
}


// returns true if the checksum on the given (NUL-terminated) message is good
bool checkChecksum(const char *p) {
  uint8_t c = 0;
  unsigned long int v;
    
  // Initial $/! is omitted from checksum, if present ignore it.
  if (*p == '$' || *p == '!')
    ++p;
  
  while (*p != '\0' && *p != '*')
    c ^= *p++;

  if(*p == '\0') {
    // no checksum field
    return false;
  }

  // compare the calculated checksum to the checksum in the input.
  p++;
  v = strtoul(p, NULL, 16);
  return v == c;
}

const char *skipField(const char *p) {
  while(*p != '\0') {
    char c = *p++;
    if(c == ',')
      break;
  }
  return p;
}

// extract an NMEA field into a string. Returns a pointer to the
// start of the next field (or the '\0' if we're at EOL)
const char *readFieldToString(const char *p, String *buf) {
  while(*p != '\0') {
    char c = *p++;
    if(c == ',')
      break;
    *buf += c;
  }
  return p;
}
