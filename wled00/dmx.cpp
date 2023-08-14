#include "wled.h"

/*
 * Support for DMX input and output via MAX485.
 * Change the output pin in src/dependencies/ESPDMX.cpp, if needed (ESP8266)
 * Change the output pin in src/dependencies/SparkFunDMX.cpp, if needed (ESP32)
 * ESP8266 Library from:
 * https://github.com/Rickgg/ESP-Dmx
 * ESP32 Library from:
 * https://github.com/sparkfun/SparkFunDMX
 */

#ifdef WLED_ENABLE_DMX

void handleDMX()
{
  // don't act, when in DMX Proxy mode
  if (e131ProxyUniverse != 0) return;

  uint8_t brightness = strip.getBrightness();

  bool calc_brightness = true;

   // check if no shutter channel is set
   for (unsigned i = 0; i < DMXChannels; i++)
   {
     if (DMXFixtureMap[i] == 5) calc_brightness = false;
   }

  uint16_t len = strip.getLengthTotal();
  for (int i = DMXStartLED; i < len; i++) {        // uses the amount of LEDs as fixture count

    uint32_t in = strip.getPixelColor(i);     // get the colors for the individual fixtures as suggested by Aircoookie in issue #462
    byte w = W(in);
    byte r = R(in);
    byte g = G(in);
    byte b = B(in);

    int DMXFixtureStart = DMXStart + (DMXGap * (i - DMXStartLED));
    for (int j = 0; j < DMXChannels; j++) {
      int DMXAddr = DMXFixtureStart + j;
      switch (DMXFixtureMap[j]) {
        case 0:        // Set this channel to 0. Good way to tell strobe- and fade-functions to fuck right off.
          dmx.write(DMXAddr, 0);
          break;
        case 1:        // Red
          dmx.write(DMXAddr, calc_brightness ? (r * brightness) / 255 : r);
          break;
        case 2:        // Green
          dmx.write(DMXAddr, calc_brightness ? (g * brightness) / 255 : g);
          break;
        case 3:        // Blue
          dmx.write(DMXAddr, calc_brightness ? (b * brightness) / 255 : b);
          break;
        case 4:        // White
          dmx.write(DMXAddr, calc_brightness ? (w * brightness) / 255 : w);
          break;
        case 5:        // Shutter channel. Controls the brightness.
          dmx.write(DMXAddr, brightness);
          break;
        case 6:        // Sets this channel to 255. Like 0, but more wholesome.
          dmx.write(DMXAddr, 255);
          break;
      }
    }
  }

  dmx.update();        // update the DMX bus
}

void initDMX() {
 #if defined(ESP8266) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2)
  dmx.init(512);        // initialize with bus length
 #else
  dmx.initWrite(512);  // initialize with bus length
 #endif
}
#endif


#ifdef WLED_ENABLE_DMX_INPUT

#include <esp_dmx.h>


static dmx_port_t dmxInputPort = 2; //TODO make this configurable
bool dmxInputInitialized = false; //true once initDmx finished successfully

void initDMX() {

  if(dmxInputReceivePin > 0 && dmxInputEnablePin > 0 && dmxInputTransmitPin > 0) 
  {

    const managed_pin_type pins[] = {
      {dmxInputTransmitPin, false}, //these are not used as gpio pins, this isOutput is always false.
      {dmxInputReceivePin, false},
      {dmxInputEnablePin, false}      
    };
    const bool pinsAllocated = pinManager.allocateMultiplePins(pins, 3, PinOwner::DMX_INPUT);
    if(!pinsAllocated)
    {
      USER_PRINTF("Error: Failed to allocate pins for DMX_INPUT. Pins already in use:\n");
      USER_PRINTF("rx in use by: %s\n", pinManager.getPinOwnerText(dmxInputReceivePin).c_str());
      USER_PRINTF("tx in use by: %s\n", pinManager.getPinOwnerText(dmxInputTransmitPin).c_str());
      USER_PRINTF("en in use by: %s\n", pinManager.getPinOwnerText(dmxInputEnablePin).c_str());
      return;
    }


    dmx_config_t config{                                             
        255,                          /*alloc_size*/             
        0,                            /*model_id*/               
        RDM_PRODUCT_CATEGORY_FIXTURE, /*product_category*/      
        VERSION,                      /*software_version_id*/    
        "undefined",                  /*software_version_label*/ 
        1,                            /*current_personality*/    
        {{15, "WLED Effect Mode"}},   /*personalities*/          
        1,                            /*personality_count*/      
        1,                            /*dmx_start_address*/      
    };
    const std::string versionString = "WLED_V" + std::to_string(VERSION);
    strncpy(config.software_version_label, versionString.c_str(), 32);
    config.software_version_label[32] = '\0';//zero termination in case our string was longer than 32 chars

    if(!dmx_driver_install(dmxInputPort, &config, DMX_INTR_FLAGS_DEFAULT))
    {
      USER_PRINTF("Error: Failed to install dmx driver\n");
      return;
    }
    
    USER_PRINTF("Listening for DMX on pin %u\n", dmxInputReceivePin);
    USER_PRINTF("Sending DMX on pin %u\n", dmxInputTransmitPin);
    USER_PRINTF("DMX enable pin is: %u\n", dmxInputEnablePin);
    dmx_set_pin(dmxInputPort, dmxInputTransmitPin, dmxInputReceivePin, dmxInputEnablePin);

    dmxInputInitialized = true;
  }
  else 
  {
    USER_PRINTLN("DMX input disabled due to dmxInputReceivePin, dmxInputEnablePin or dmxInputTransmitPin not set");
    return;
  }

}
  
bool dmxIsConnected = false;
unsigned long dmxLastUpdate = 0;

void handleDMXInput() {
  if(!dmxInputInitialized) {
    return;
  }
  byte dmxdata[DMX_PACKET_SIZE];
  dmx_packet_t packet;
  unsigned long now = millis();
  if (dmx_receive(dmxInputPort, &packet, 0)) {

    /* We should check to make sure that there weren't any DMX errors. */
    if (!packet.err) {
      /* If this is the first DMX data we've received, lets log it! */
      if (!dmxIsConnected) {
        USER_PRINTLN("DMX is connected!");
        dmxIsConnected = true;
      }

      dmx_read(dmxInputPort, dmxdata, packet.size);
      handleDMXData(1, 512, dmxdata, REALTIME_MODE_DMX, 0);
      dmxLastUpdate = now;

    } else {
      /* Oops! A DMX error occurred! Don't worry, this can happen when you first
        connect or disconnect your DMX devices. If you are consistently getting
        DMX errors, then something may have gone wrong with your code or
        something is seriously wrong with your DMX transmitter. */
     DEBUG_PRINT("A DMX error occurred - ");
     DEBUG_PRINTLN(packet.err);
    }
  }
  else if (dmxIsConnected && (now - dmxLastUpdate > 5000)) {
    dmxIsConnected = false;
    USER_PRINTLN("DMX was disconnected.");
  }
}
#else
void initDMX();
#endif
