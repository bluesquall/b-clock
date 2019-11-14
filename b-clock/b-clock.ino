// A basic everyday NeoPixel strip test program.

// NEOPIXEL BEST PRACTICES for most reliable operation:
// - Add 1000 uF CAPACITOR between NeoPixel strip's + and - connections.
// - MINIMIZE WIRING LENGTH between microcontroller board and first pixel.
// - NeoPixel strip's DATA-IN should pass through a 300-500 OHM RESISTOR.
// - AVOID connecting NeoPixels on a LIVE CIRCUIT. If you must, ALWAYS
//   connect GROUND (-) first, then +, then data.
// - When using a 3.3V microcontroller with a 5V-powered NeoPixel strip,
//   a LOGIC-LEVEL CONVERTER on the data line is STRONGLY RECOMMENDED.
// (Skipping these may work OK on your workbench but can fail in the field)

#include <Adafruit_NeoPixel.h>
#include <RTClib.h>

#define LED_PIN    30
#define LED_COUNT 4*8

// Declare our NeoPixel strip object:
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
// Declare the DS3231 Realtime Clock object
RTC_DS3231 rtc;
char display = 'B';
DateTime alarm;
int8_t utc_offset = -8;
// TODO: add a verbose flag to enable/disable most of the Serial printing


// setup() function -- runs once at startup --------------------------------

void setup() {
  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();            // Turn OFF all pixels ASAP
  Serial.begin(9600);      // Start UART for debugging, etc.
  while (!Serial) delay(10);   // for nrf52840 with native usb
  Serial.println("Bluefruit nRF52832 b-clock");
  Serial.println("**************************\n");
  post();                  // Power On Self Test
}

// post() function -- power-on self-test

void post() {
  Serial.println("b-clock power-on self-test");
  strip.setBrightness(8); // Set BRIGHTNESS low (max = 255)
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
}

// loop() function -- runs repeatedly as long as board is on ---------------

void loop() {
  digitalToggle(LED_RED);  // Toggle LED to show activity

  DateTime now = rtc.now();
  serialPrintDateTime(now);
  serialPrintTemperature();

  if( Serial.available() > 0 ) {
    String cmd = Serial.readString();
    cmd.trim();
    Serial.print("rx command: `");
    Serial.print(cmd);
    Serial.println("`");
    parseCommand(cmd);
  }

  
  if (now < alarm) {
    countdown(alarm);
  } else if ( now.unixtime() - alarm.unixtime() < LED_COUNT ) {
    strip.setBrightness(127);
    colorWipe(strip.Color( 0, 0, 0), 100); // off
    colorWipe(strip.Color( 255, 140, 0), 100); // dark orange
  } else if ( now.unixtime() - alarm.unixtime() < 60 ) {
    strip.setBrightness(255);
    colorWipe(strip.Color( 0, 0, 0), 0); // off
    colorWipe(strip.Color( 255, 0, 0), 0); // red
  } else {
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
  colorWipe(strip.Color(  0,   127, 127), 32); // TODO constant
  colorWipe(strip.Color(  0,   0, 0), 32); // off  
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
  colorWipe(strip.Color(R, G, B), 32);
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
      alarm = rtc.now() + TimeSpan(0,0,a.toInt(),0);
      break;
    case 'E': // set Epoch time
      rtc.adjust(DateTime(a.toInt()));
      break;
    case 'U': // set UTC offset
      rtc.adjust(DateTime(rtc.now().unixtime() - utc_offset * 3600));
      utc_offset = a.toInt();
      rtc.adjust(DateTime(rtc.now().unixtime() + utc_offset * 3600));
      break;
    case 'D': // set (string) date e.g.: `Nov 15 2019`
      rtc.adjust(DateTime(a.c_str(), rtc.now().timestamp(DateTime::TIMESTAMP_TIME).c_str()));
      break;
    case 'T': // set (string) time e.g: `14:31:42`
      rtc.adjust(DateTime(rtc.now().timestamp(DateTime::TIMESTAMP_DATE).c_str(), a.c_str()));
      break;      
    case 'd':
      a.toUpperCase();
      display = a.charAt(0); // set default display type
      break;
    case 'b':
      strip.setBrightness(a.toInt()); // Set BRIGHTNESS (max = 255)
      break;
    default:
      Serial.print("unrecognized function: `");
      Serial.print(f);
      Serial.println('`');
  }
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
