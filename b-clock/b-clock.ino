// a bluetooth-enabled alarm clock with NeoPixels

#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <Adafruit_NeoPixel.h>
#include <RTClib.h>

#define LED_PIN    30
#define LED_COUNT 4*8

#define STANDARD_DELAY 32 // milliseconds

BLEDfu  bledfu;  // OTA DFU service
BLEDis  bledis;  // device information
BLEUart bleuart; // uart over ble
BLEBas  blebas;  // battery
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
RTC_DS3231 rtc;


// GLOBAL variables
char display = 'B';
DateTime alarm = DateTime( 0 );
int8_t utc_offset = -8;
int8_t brightness = 3;

// TODO: add a verbose flag to enable/disable most of the Serial printing


// setup() function -- runs once at startup --------------------------------

void setup() {
  pinMode(LED_RED, OUTPUT);
  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();            // Turn OFF all pixels ASAP
  Serial.begin(9600);      // Start UART for debugging, etc.
  while (!Serial) delay(10);   // for nrf52840 with native usb
  Serial.println("Bluefruit Feather nRF52832 b-clock");
  Serial.println("**********************************\n");
  Serial.println("Please use Adafruit's Bluefruit LE app to connect in UART mode\n");

  Bluefruit.autoConnLed(true);
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin();
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  Bluefruit.setName("b-clock");
  // To be consistent OTA DFU should be added first if it exists
  bledfu.begin();

  // Configure and Start Device Information Service
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("b-clock based on Bluefruit Feather nRF52832");
  bledis.begin();
  bleuart.begin();
  blebas.begin();
  blebas.write(100); // TODO: check actual battery voltage
  startAdv();

  post();                  // Power On Self Test
}

// post() function -- power-on self-test

void post() {
  digitalWrite(LED_RED, HIGH);
  Serial.println("b-clock power-on self-test");
  strip.setBrightness(brightness); // Set BRIGHTNESS low (max = 255)
  rainbow(1);
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power -- setting to sketch compilation time");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    serialPrintDateTime(rtc.now());
  }
  alarm = rtc.now() + TimeSpan(0,0,0,5);
  digitalWrite(LED_RED, LOW);
}


void startAdv(void) {
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include bleuart 128-bit uuid
  Bluefruit.Advertising.addService(bleuart);

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds  
}

// loop() function -- runs repeatedly as long as board is on ---------------

void loop() {
  DateTime now = rtc.now();
  serialPrintDateTime(now);
  serialPrintTemperature();

  if( Serial.available() ) {
    digitalWrite(LED_RED, HIGH);
    String cmd = Serial.readString();
    cmd.trim();
    Serial.print("rx command: `");
    Serial.print(cmd);
    Serial.println("`");
    parseCommand(cmd);
    digitalWrite(LED_RED, LOW);
  }

  if( bleuart.available() ) {
    digitalWrite(LED_RED, HIGH);
    
    char c_cmd[64] = {'\0'};
    uint8_t na = bleuart.available();
    uint8_t nr = bleuart.read(c_cmd, ( na < 64 ) ? na : 64 );
    String cmd = String(c_cmd);
    
    cmd.trim();
    Serial.print("BLE RX: `");
    Serial.print(cmd);
    Serial.println("`");
    
    parseCommand(cmd);
    digitalWrite(LED_RED, LOW);
  }
  
  if (now < alarm) {
    strip.setBrightness(brightness); // Set BRIGHTNESS low (max = 255)
    countdown(alarm);
  } else if ( now.unixtime() - alarm.unixtime() < LED_COUNT ) {
    colorWipe(strip.Color( 0, 0, 0), STANDARD_DELAY); // off
    strip.setBrightness(3*brightness);
    colorWipe(strip.Color( 255, 140, 0), STANDARD_DELAY); // dark orange
  } else if ( now.unixtime() - alarm.unixtime() < 60 ) {
    colorWipe(strip.Color( 0, 0, 0), 0); // off
    strip.setBrightness(9*brightness);
    colorWipe(strip.Color( 255, 0, 0), 0); // red
  } else {
    strip.setBrightness(brightness); // Set BRIGHTNESS low (max = 255)
    switch( display ) {
      case 'B': // binary
        binary(now);
        break;
      case 'R':
        rainbow(1);
        break;
      case 'T': // strandtest
        strandtest();
        break;
      default:
        Serial.print("unrecognised default display type");
        Serial.println(display);
        rainbow(1);
    }
  }
}

// countdown
void countdown( DateTime t ) {
  Serial.print("counting down to: ");
  serialPrintDateTime(t);
  colorWipe(strip.Color(  0,   0, 0), STANDARD_DELAY); // off  
  colorWipe(strip.Color(  0,   127, 127), STANDARD_DELAY); // TODO constant
}

// binary
void binary( DateTime t ) {
  uint8_t R = t.hour()*255/24;
  uint8_t G = t.minute()*255/60;
  uint8_t B = t.second()*255/60;
  Serial.print("displaying (pseudo) binary color representation of: ");
  char hms[] = "hh:mm:ss";
  Serial.print(t.toString(hms));
  Serial.print(" R,G,B: ");
  Serial.print( R );
  Serial.print( "," );
  Serial.print( G );
  Serial.print( "," );
  Serial.println( B );
  colorWipe(strip.Color(R, G, B), STANDARD_DELAY);
}

// parseCommand
void parseCommand(String cmd) {
  Serial.print("parsing command: `");
  Serial.print(cmd);
  Serial.println("`");

  char f = cmd.charAt(0);
  String a = cmd.substring(2);
  // TODO: these ^ two lines are brittle
  
  Serial.print("function: ");
  Serial.print(f);
  Serial.print("\t argument: ");
  Serial.print(a);
  Serial.println("");

  switch( f ) {
    case 'z': // snooze
      alarm = rtc.now() + TimeSpan(a.toInt() * 60);
      bleuart.print(rtc.now().timestamp(DateTime::TIMESTAMP_FULL));
      bleuart.print("> alarm set for: ");
      bleuart.println(alarm.timestamp(DateTime::TIMESTAMP_FULL));
      break;
    case '+': // add to existing alarm
      alarm = alarm + TimeSpan(a.toInt() * 60);
      bleuart.print(rtc.now().timestamp(DateTime::TIMESTAMP_FULL));
      bleuart.print("> alarm set for: ");
      bleuart.println(alarm.timestamp(DateTime::TIMESTAMP_FULL));
      break;
    case 'E': // set Epoch time
      rtc.adjust(DateTime(a.toInt()));
      bleuart.print(rtc.now().timestamp(DateTime::TIMESTAMP_FULL));
      bleuart.println("> set by Unix epoch");
      break;
    case 'U': // set UTC offset
      rtc.adjust(DateTime(rtc.now().unixtime() - utc_offset * 3600));
      utc_offset = a.toInt();
      rtc.adjust(DateTime(rtc.now().unixtime() + utc_offset * 3600));
      bleuart.print("set UTC offset to ");
      bleuart.print(utc_offset);
      bleuart.println(" hours");
      break;
    case 'D': // set (string) date e.g.: `Nov 15 2019`
      rtc.adjust(DateTime(a.c_str(), rtc.now().timestamp(DateTime::TIMESTAMP_TIME).c_str()));
      bleuart.print(rtc.now().timestamp(DateTime::TIMESTAMP_FULL));
      bleuart.println("> set local date");
      bleuart.println("SET TIME BEFORE DATE DUE TO BUG");
      
      break;
    case 'T': // set (string) time e.g: `14:31:42`
      rtc.adjust(DateTime(rtc.now().timestamp(DateTime::TIMESTAMP_DATE).c_str(), a.c_str()));
      bleuart.print(rtc.now().timestamp(DateTime::TIMESTAMP_FULL));
      bleuart.println("> set local time");
      break;      
    case 'd':
      a.toUpperCase();
      display = a.charAt(0); // set default display type
      break;
    case 'b':
      brightness = a.toInt(); // set GLOBAL (persistent) brightness
      strip.setBrightness(brightness); // Set BRIGHTNESS (max = 255)
      break;
    case '?':
    case 'H':
    case 'h':
      usage();
      break;
    default:
      Serial.print("unrecognized function: `");
      Serial.print(f);
      Serial.println('`');
      usage();
  }
}

// usage -- print command hints to both wired and BLE UARTs

void usage() {
  bleuart.println("?,h,H");
  bleuart.println("\tprint this help dialog");
  
  bleuart.println("z minutes\t e.g.: `z 5`");
  bleuart.println("\t set alarm 5 minutes from now");

  bleuart.println("+ minutes\t e.g.: `+ 5`");
  bleuart.println("\t add 5 minutes to existing alarm");

  bleuart.println("b level\t e.g.: `b 255`");
  bleuart.println("\t set brightness level");

  bleuart.println("D Date String\t e.g.;`D Nov 15 2019`");
  bleuart.println("\t set date (local timezone)");

  bleuart.println("T Time String\t e.g.;`T 13:37:01`");
  bleuart.println("\t set time (local timezone)");

  bleuart.println("U hours\t e.g.;`U 8`");
  bleuart.println("\t set UTC offset for local timezone");
  bleuart.println("\t or daylight savings time");

  bleuart.println("E seconds\t e.g.;`E 1573796625`");
  bleuart.println("\t set time by Unix epoch seconds");

  bleuart.println("d letter\t e.g.: `d r`");
  bleuart.println("\t set default display type");
  bleuart.println("\t\t b: color for HH:MM:SS");
  bleuart.println("\t\t r: rainbow");
  bleuart.println("\t\t t: test pattern (long)");
    
  
}

// serialPrintDateTime

void serialPrintDateTime( DateTime ts ) {
  char iso8601[] = "YYYY-MM-DD hh:mm:ss";
  Serial.print(ts.toString(iso8601));
  Serial.print(" (UTC ");
  Serial.print(utc_offset);
  Serial.print(")\t Unix epoch seconds: ");
  Serial.println(ts.unixtime() - utc_offset * 3600);
}

void serialPrintTemperature() {
  Serial.print("Temperature: ");
  Serial.print(rtc.getTemperature());
  Serial.println(" C");
}

// strandtest() function -- keep for testing

void strandtest() {
  // Fill along the length of the strip in various colors...
  colorWipe(strip.Color(255,   0,   0), 50); // Red
  colorWipe(strip.Color(  0, 255,   0), 50); // Green
  colorWipe(strip.Color(  0,   0, 255), 50); // Blue

  // Do a theater marquee effect in various colors...
  theaterChase(strip.Color(127, 127, 127), 50); // White, half brightness
  theaterChase(strip.Color(127,   0,   0), 50); // Red, half brightness
  theaterChase(strip.Color(  0,   0, 127), 50); // Blue, half brightness

  rainbow(10);             // Flowing rainbow cycle along the whole strip
  theaterChaseRainbow(50); // Rainbow-enhanced theaterChase variant
}


// Some functions of our own for creating animated effects -----------------

// Fill strip pixels one after another with a color. Strip is NOT cleared
// first; anything there will be covered pixel by pixel. Pass in color
// (as a single 'packed' 32-bit value, which you can get by calling
// strip.Color(red, green, blue) as shown in the loop() function above),
// and a delay time (in milliseconds) between pixels.
void colorWipe(uint32_t color, int wait) {
  for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
    strip.setPixelColor(i, color);         //  Set pixel's color (in RAM)
    strip.show();                          //  Update strip to match
    delay(wait);                           //  Pause for a moment
  }
}

// Theater-marquee-style chasing lights. Pass in a color (32-bit value,
// a la strip.Color(r,g,b) as mentioned above), and a delay time (in ms)
// between frames.
void theaterChase(uint32_t color, int wait) {
  for(int a=0; a<10; a++) {  // Repeat 10 times...
    for(int b=0; b<3; b++) { //  'b' counts from 0 to 2...
      strip.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in steps of 3...
      for(int c=b; c<strip.numPixels(); c += 3) {
        strip.setPixelColor(c, color); // Set pixel 'c' to value 'color'
      }
      strip.show(); // Update strip with new contents
      delay(wait);  // Pause for a moment
    }
  }
}

// Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
void rainbow(int wait) {
  // Hue of first pixel runs 5 complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to 5*65536. Adding 256 to firstPixelHue each time
  // means we'll make 5*65536/256 = 1280 passes through this outer loop:
  for(long firstPixelHue = 0; firstPixelHue < 5*65536; firstPixelHue += 256) {
    for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the single-argument hue variant. The result
      // is passed through strip.gamma32() to provide 'truer' colors
      // before assigning to each pixel:
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show(); // Update strip with new contents
    delay(wait);  // Pause for a moment
  }
}

// Rainbow-enhanced theater marquee. Pass delay time (in ms) between frames.
void theaterChaseRainbow(int wait) {
  int firstPixelHue = 0;     // First pixel starts at red (hue 0)
  for(int a=0; a<30; a++) {  // Repeat 30 times...
    for(int b=0; b<3; b++) { //  'b' counts from 0 to 2...
      strip.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in increments of 3...
      for(int c=b; c<strip.numPixels(); c += 3) {
        // hue of pixel 'c' is offset by an amount to make one full
        // revolution of the color wheel (range 65536) along the length
        // of the strip (strip.numPixels() steps):
        int      hue   = firstPixelHue + c * 65536L / strip.numPixels();
        uint32_t color = strip.gamma32(strip.ColorHSV(hue)); // hue -> RGB
        strip.setPixelColor(c, color); // Set pixel 'c' to value 'color'
      }
      strip.show();                // Update strip with new contents
      delay(wait);                 // Pause for a moment
      firstPixelHue += 65536 / 90; // One cycle of color wheel over 90 frames
    }
  }
}
