// TODO / IDEA
// add crossfade between modes
// add includes, defines, prototypes, constants, globals, function comment headers
// collapse white mode into fixed mode, collapse wash mode into rainbow mode

#include "Particle.h"
#include "controller.h"
#include "effects.h"

// system directives
STARTUP(WiFi.selectAntenna(ANT_INTERNAL));
SYSTEM_THREAD(ENABLED);

// select KINET or ART-NET
// #define KINET
#define ARTNET

// KINET or ART-NET Interface IP Address
#define PDS_IP_ADDR 192,168,180,83

// Art-Net Protocol Support
#ifdef ARTNET
#define ARTNET_PORT 6454
#define ARTNET_HEADER_BYTES 18
#define ARTNET_LEVEL_BYTES 512
#define ARTNET_PACKET_BYTES (ARTNET_HEADER_BYTES + ARTNET_LEVEL_BYTES)
static const uint8_t artnet_header[ARTNET_HEADER_BYTES] = {
    'A', 'r', 't', '-', 'N', 'e', 't', 0x00,
    0x00, 0x50, 0x00, 0x0e,
    0x00, 0x00, 0x01, 0x00,
    0x02, 0x00
};
#endif

// Kinet Protocol Support
#ifdef KINET
#define KINET_PORT 6038
#define KINET_HEADER_BYTES 21
#define KINET_LEVELS_BYTES 512
#define KINET_PACKET_BYTES (KINET_HEADER_BYTES + KINET_LEVELS_BYTES)
static const uint8_t kinet_header[KINET_HEADER_BYTES] = {
    0x04, 0x01, 0xdc, 0x4a,
    0x01, 0x00,
    0x01, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00,
    0x00,
    0x00, 0x00,
    0xff, 0xff, 0xff, 0xff,
    0x00
};
#endif

// prototypes
void DebounceButtons (void);

// pushbutton debounce state machine states
#define NUM_BUTTONS 6
int buttons[NUM_BUTTONS] = { A4, A5, A2, A3, A0, A1 };
uint8_t buttonStates[NUM_BUTTONS] = { 0, 0, 0, 0, 0, 0 };
bool buttonDownEvents[NUM_BUTTONS] = { false, false, false, false, false, false };

// LEDs
#define NUM_LEDS 6
int leds[NUM_LEDS] = { D4, D5, D2, D3, D0, D1 };
#define LED_OFF 0
#define LED_ON 1

// globals
Timer timer (20, tick_50Hz, false);
IPAddress pdsIpAddr (PDS_IP_ADDR);
UDP udp;

int connected = false;
int wiFiReady = false;
int scene = 0;
int lastScene = 0;
uint8_t levels[CHANNELS];
int timer3s = 0;

#ifdef KINET
uint8_t udp_buffer[KINET_PACKET_BYTES];
#endif

#ifdef ARTNET
uint8_t udp_buffer[ARTNET_PACKET_BYTES];
#endif

Scene scenes[7] = {
    { MODE_BLACKOUT,    0x00, 0x00, 0x00,  0x00, 0x0000, 0x0000,    0,     0 },
    { MODE_FIXED,       0xff, 0x00, 0x00,  0x00, 0x0000, 0x0000,    0,     0 },
    { MODE_WASH,        0x00, 0x00, 0x00,  0x01, 0x0007, 0x0000,    0,     0 },
    { MODE_RAINBOW,     0x00, 0x00, 0x00,  0x01, 0x0100, 0x0060,    0,     0 },
    { MODE_RANDOM,      0x00, 0x00, 0x00,  0x00, 0x0000, 0x0000,  150,     0 },
    { MODE_LFSR_RANDOM, 0x00, 0x00, 0x00,  0x00, 0x0000, 0x0000,   50,     0 },
    { MODE_WHITE,       0x00, 0x00, 0x00,  0x00, 0x0000, 0x0000,    0,  0xff }
};


void setup()
{
    int i;

    delay (1000);
    Serial.begin (9600);
    Serial.printf ("\n\r\n\rHello, world!\n\r");
    Serial.printlnf("System version: %s", System.version().c_str());

    for (i = 0; i < NUM_BUTTONS; i++) {
        pinMode (buttons[i], INPUT_PULLUP);
    }
    for (i = 0; i < NUM_LEDS; i++) {
        pinMode (leds[i], OUTPUT);
        digitalWrite (leds[i], LED_OFF);
    }

    // start periodic task to update time displayed on nixie tubes
    timer.start();
}

void loop()
{
    if (!connected && Particle.connected ()) {
        connected = true;
        Serial.printf ("Connected to the Particle Cloud.\n\r");
    } else if (connected && !Particle.connected ()) {
        connected = false;
        Serial.printf ("Disconnected from the Particle Cloud.\n\r");
    }
}

void tick_50Hz (void)
{
    int i;

    timer3s++;
    if (timer3s >= 150) {
        timer3s = 0;
        if (WiFi.ready ()) {
            Serial.printf (".");
        } else if (WiFi.connecting ()) {
            Serial.printf ("C");
        } else if (!WiFi.ready ()) {
            Serial.printf ("X");
        }
    }

    // check WiFi status and re-init udp routines if needed
    if (!wiFiReady && WiFi.ready ()) {
        wiFiReady = true;
        Serial.printf ("Connected to WiFi.\n\r");
#ifdef KINET
        // udp.begin (KINET_PORT);
#endif
#ifdef ARTNET
        // udp.begin (ARTNET_PORT);
#endif
    } else if (wiFiReady && !WiFi.ready ()) {
        wiFiReady = false;
        Serial.printf ("Lost WiFi connection.\n\r");
    }

    if (wiFiReady) {
        // send levels
#ifdef KINET
        udp.begin (KINET_PORT);
        memset (udp_buffer, 0, KINET_PACKET_BYTES);
        memcpy (&udp_buffer[0], kinet_header, KINET_HEADER_BYTES);
        memcpy (&udp_buffer[KINET_HEADER_BYTES], levels, CHANNELS);
        udp.sendPacket (udp_buffer, KINET_PACKET_BYTES, pdsIpAddr, KINET_PORT);
        // Serial.printf ("KI: %02x %02x %02x\n\r", levels[24], levels[25], levels[26]);
        udp.stop ();
#endif

#ifdef ARTNET
        udp.begin (ARTNET_PORT);
        memset (udp_buffer, 0, ARTNET_PACKET_BYTES);
        memcpy (&udp_buffer[0], artnet_header, ARTNET_HEADER_BYTES);
        memcpy (&udp_buffer[ARTNET_HEADER_BYTES], levels, CHANNELS);
        udp.sendPacket (udp_buffer, ARTNET_PACKET_BYTES, pdsIpAddr, ARTNET_PORT);
        // Serial.printf ("AN: %02x %02x %02x\n\r", levels[24], levels[25], levels[26]);
        udp.stop ();
#endif
    }

    // debounce pushbuttons
    DebounceButtons ();

    // process button presses
    for (i = 0; i < NUM_BUTTONS; i++) {
        if (buttonDownEvents[i]) {
            buttonDownEvents[i] = false;
            Serial.printf ("button %d pressed\n\r", i);
            if (scene == (i+1)) {
                scene = 0;
            } else {
                scene = i + 1;
            }
        }
    }

    // update leds
    for (i = 0; i < NUM_LEDS; i++) {
        digitalWrite (leds[i], (scene == (i+1)) ? LED_ON : LED_OFF);
    }

    // change mode
    if (scene != lastScene) {
        Serial.printf ("Scene changed from %d to %d.\n\r", lastScene, scene);
        lastScene = scene;
        switch (scenes[scene].mode) {
            case MODE_BLACKOUT:
                InitBlackout ();
                break;
            case MODE_FIXED:
                InitFixed (scenes[scene].red, scenes[scene].green, scenes[scene].blue);
                break;
            case MODE_WASH:
                InitColorWash (scenes[scene].direction, scenes[scene].step);
                break;
            case MODE_RAINBOW:
                InitRainbow (scenes[scene].direction, scenes[scene].step, scenes[scene].offset);
                break;
            case MODE_RANDOM:
                InitRandomColor (scenes[scene].delay);
                break;
            case MODE_LFSR_RANDOM:
                InitLfsrRandom (scenes[scene].delay);
                break;
            case MODE_WHITE:
                InitWhite (scenes[scene].level);
                break;
        }
    }

    // run mode
    switch (scenes[scene].mode) {
        case MODE_BLACKOUT:
            RunBlackout ();
            break;
        case MODE_FIXED:
            RunFixed ();
            break;
        case MODE_WASH:
            RunColorWash ();
            break;
        case MODE_RAINBOW:
            RunRainbow ();
            break;
        case MODE_RANDOM:
            RunRandomColor ();
            break;
        case MODE_LFSR_RANDOM:
            RunLfsrRandom ();
            break;
        case MODE_WHITE:
            RunWhite ();
            break;
    }
}

void DebounceButtons (void)
{
    int i;

    for (i = 0; i < NUM_BUTTONS; i++) {
        switch (buttonStates[i]) {
            case 0:
                buttonStates[i] = (digitalRead (buttons[i]) == 0) ? 1 : 0;
                break;
            case 1:
                if (digitalRead (buttons[i]) == 0) {
                    buttonStates[i] = 2;
                    buttonDownEvents[i] = true;
                } else {
                    buttonStates[i] = 0;
                }
                break;
            case 2:
                buttonStates[i] = (digitalRead (buttons[i]) == 0) ? 2 : 3;
                break;
            case 3:
                buttonStates[i] = (digitalRead (buttons[i]) == 0) ? 2 : 0;
                break;
            default:
                buttonStates[i] = 0;
                break;
        }
    }
}
