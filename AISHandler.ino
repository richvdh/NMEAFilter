static int currentMsgNumber = -1;
static int skippingCurrentMsg = 0; // 1 if we have decided to skip the current message.

void handleAISCommand() {
  int k = 0;
  const char *p = input_buf;

  // field 1: NMEA sentence identifier
  p = skipField(p);

  // field 2: number of fragments in message
  String buf = "";
  p = readFieldToString(p, &buf);
  int fragCount = buf.toInt();
  buf="";
  
  // field 3: fragment number
  p = readFieldToString(p, &buf);
  int frag = buf.toInt();
  buf="";

  // field 4: msgid. This will be empty for a single-part message,
  // which gives msgid=1
  p = readFieldToString(p, &buf);
  int msgid = buf.toInt();
  buf="";

  // field 5: radio channel
  p = skipField(p);

  // field 6: payload
  const char *payload = p;

  diagnosticPort.print("msgid ");
  diagnosticPort.print(msgid, DEC);
  diagnosticPort.print(" ");
  diagnosticPort.print(frag, DEC);
  diagnosticPort.print("/");
  diagnosticPort.println(fragCount, DEC);
  
  // if we are in the middle of a message, and this is a different message, then
  // we've got out of sync. Warn about the residue, and reset.
  if (currentMsgNumber >= 0 && msgid != currentMsgNumber) {
    diagnosticPort.println("Incomplete message!");
    currentMsgNumber = -1;
    skippingCurrentMsg = 0;
  }

  // if this is not the first fragment of a message, just do whatever we decided before
  if (frag != 1) {
    if (!skippingCurrentMsg) {
      writeMessage(input_buf);
    }
    if(frag >= fragCount) {
      currentMsgNumber = -1;
      skippingCurrentMsg = 0;
    }
    return;
  }

  // it's the first (or only) fragment in a new message. Decode it.
  //
  // to make things easier, make sure we've got enough left in the buffer to decode what we need
  if (strlen(payload) < 6) {
    diagnosticPort.print("Truncated message?");
  } else {
    // the first byte is an (8-bit-armoured) 6-bit message type. We don't
    // care, but it's interesting.
    uint8_t msg_type = unarmor(payload[0]);
    
    diagnosticPort.print("AIS message type ");
    diagnosticPort.println(msg_type, DEC);

    // bits 8-37 are are the MMSI
    uint32_t mmsi = decodeUint(payload+1, 2, 30);
    diagnosticPort.print("MMSI: ");
    diagnosticPort.println(mmsi, DEC);

    if (mmsi == discardMmsi) {
      skippingCurrentMsg = 1;
    }
  }

  // copy through if we're not skipping
  if (skippingCurrentMsg) {
    diagnosticPort.println("Discarding AIS message from matching MMSI");
    
    // flash the LED
    digitalWrite(LED_BUILTIN, 1);
    led_is_on = true;
    led_off_micros = micros() + 1000;
  } else {
    writeMessage(input_buf);
  }

  // if there are more fragments, remember the message number
  if (frag < fragCount) {
    currentMsgNumber = msgid;
  } else {
    currentMsgNumber = -1;
    skippingCurrentMsg = 0;
  }
}


// decode an integer encoded in 6-bit nibbles as 8-bit ascii-armored characters.
// warning: won't work if all the bits fit in the same byte
static uint32_t decodeUint(const char *p, int bitoffset, int nbits) {
  uint32_t res;

  // first few bits
  res = unarmor(*p++);
  res &= (0xFF >> bitoffset);
  nbits -= (6-bitoffset);

  // all the middle bits
  while(nbits > 6) {
    uint8_t c = unarmor(*p++);
    res <<= 6;
    res |= c;
    nbits -= 6;
  }

  // final few bits
  if (nbits > 0) {
    res <<= nbits;
    res |= unarmor(*p) >> (6 - nbits);
  }
  
  return res;
}


// de-armor an 8-bit armored 6-bit nibble
static inline uint8_t unarmor(char c) {
  c -= 48;
  if (c > 40)
    c -= 8;
  return c;    
}
