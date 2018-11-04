// OPEN       CLOSE
// TOP OPEN   BOTTOM OPEN
// HOLD BDRM  HOLD BATH

#include "Particle.h"
#include "Sunrise.h"
#include "HttpClient.h"
#include "ToDTimer.h"

STARTUP(WiFi.selectAntenna(ANT_INTERNAL));
SYSTEM_THREAD(ENABLED);

#define MOUNTAIN_STANDARD           -7
#define TIME_ZONE                   (MOUNTAIN_STANDARD)
#define LATITUDE                    40.546864
#define LONGITUDE                   -105.127797
#define HDPV_HUB_IP_ADDR            192,168,180,19

#define HDPV_BATH_OPEN_SCENE_ID     19133
#define HDPV_BATH_CLOSE_SCENE_ID    50256
#define HDPV_BATH_TOPDWN_SCENE_ID   4
#define HDPV_BATH_BTMUP_SCENE_ID    24593

#define HDPV_BDRM_OPEN_SCENE_ID     48440
#define HDPV_BDRM_CLOSE_SCENE_ID    55754
#define HDPV_BDRM_TOPDWN_SCENE_ID   10818
#define HDPV_BDRM_BTMUP_SCENE_ID    37982

int led1 = D7;

int connected = false;

int lastDay = 0;
bool timeSyncRequested = false;
bool timeValid = false;
bool dstActive = false;

// pushbutton debounce state machine states
#define NUM_BUTTONS 6
int buttons[NUM_BUTTONS] = { A4, A5, A2, A3, A0, A1 };
uint8_t buttonStates[NUM_BUTTONS] = { 0, 0, 0, 0, 0, 0 };
bool buttonDownEvents[NUM_BUTTONS] = { false, false, false, false, false, false };

// when set to true, holds execution of all bedroom time of day events until hold released
#define HOLD0_LED D0
#define HOLD0_LED_OFF 0
#define HOLD0_LED_ON 1
bool hold0State = false;

// when set to true, holds execution of all bathroom time of day events until hold released
#define HOLD1_LED D1
#define HOLD1_LED_OFF 0
#define HOLD1_LED_ON 1
bool hold1State = false;

Timer timer (10, tick_100Hz, false);
Sunrise sunrise (LATITUDE, LONGITUDE);
HttpClient http;

ToDTimer todSunrise;
ToDTimer todSunset;
ToDTimer tod1pm;
ToDTimer tod11pm;

http_request_t request;
http_response_t response;
http_header_t headers[] = {
    { "Content-Type", "application/json" },
    { NULL, NULL }
};

int ledToggle (String command);
void DebounceButtons (void);
bool IsDST(int dayOfMonth, int month, int dayOfWeek);
void PrintResponse (void);
void MakeGetRequest (String path);
void HunterDouglasPowerviewHubSetScene (int sceneId);

void setup()
{
    int i;

    delay (1000);
    Serial.begin (9600);
    Serial.printf ("\n\r\n\rHello, world!\n\r");

    pinMode (led1, OUTPUT);
    digitalWrite (led1, LOW);

    for (i = 0; i < NUM_BUTTONS; i++) {
        pinMode (buttons[i], INPUT_PULLUP);
    }
    pinMode (HOLD0_LED, OUTPUT);
    digitalWrite (HOLD0_LED, HOLD0_LED_OFF);
    pinMode (HOLD1_LED, OUTPUT);
    digitalWrite (HOLD1_LED, HOLD1_LED_OFF);

    // default to time zone, will update dstActive and time zone after sync
    Time.zone (TIME_ZONE);
    Time.setDSTOffset (1.0);
    Time.endDST ();

    // configure http client
    request.ip = IPAddress (HDPV_HUB_IP_ADDR);
    request.port = 80;

    // start periodic task to update time displayed on nixie tubes
    timer.start();
}

void loop()
{
    if (!connected) {
        if (Particle.connected ()) {
            connected = true;
            Serial.printf ("Connected to the Particle Cloud.\n\r");
            Particle.function ("led", ledToggle);
        }
    }
}

void tick_100Hz (void)
{
    time_t now;

    // debounce pushbuttons
    DebounceButtons ();

    // button 0: open shades
    if (buttonDownEvents[0]) {
        buttonDownEvents[0] = false;
        Serial.printf ("button 0 pressed\n\r");
        if (!hold1State) { HunterDouglasPowerviewHubSetScene (HDPV_BATH_OPEN_SCENE_ID); }
        if (!hold0State) { HunterDouglasPowerviewHubSetScene (HDPV_BDRM_OPEN_SCENE_ID); }
    }

    // button 1: close shades
    if (buttonDownEvents[1]) {
        buttonDownEvents[1] = false;
        Serial.printf ("button 1 pressed\n\r");
        if (!hold1State) { HunterDouglasPowerviewHubSetScene (HDPV_BATH_CLOSE_SCENE_ID); }
        if (!hold0State) { HunterDouglasPowerviewHubSetScene (HDPV_BDRM_CLOSE_SCENE_ID); }
    }

    // button 2: partially open shades from the top
    if (buttonDownEvents[2]) {
        buttonDownEvents[2] = false;
        Serial.printf ("button 2 pressed\n\r");
        if (!hold1State) { HunterDouglasPowerviewHubSetScene (HDPV_BATH_TOPDWN_SCENE_ID); }
        if (!hold0State) { HunterDouglasPowerviewHubSetScene (HDPV_BDRM_TOPDWN_SCENE_ID); }
    }

    // button 3: partially open shades from the bottom
    if (buttonDownEvents[3]) {
        buttonDownEvents[3] = false;
        Serial.printf ("button 3 pressed\n\r");
        if (!hold1State) { HunterDouglasPowerviewHubSetScene (HDPV_BATH_BTMUP_SCENE_ID); }
        if (!hold0State) { HunterDouglasPowerviewHubSetScene (HDPV_BDRM_BTMUP_SCENE_ID); }
    }

    // button 4: change hold state
    if (buttonDownEvents[4]) {
        buttonDownEvents[4] = false;
        Serial.printf ("button 4 pressed\n\r");
        hold0State = !hold0State;
        digitalWrite (HOLD0_LED, hold0State ? HOLD0_LED_ON : HOLD0_LED_OFF);
        Serial.printf ("hold0State changed: %s\n\r", hold0State ? "true" : "false");
    }

    // button 5: TBD
    if (buttonDownEvents[5]) {
        buttonDownEvents[5] = false;
        Serial.printf ("button 5 pressed\n\r");
        hold1State = !hold1State;
        digitalWrite (HOLD1_LED, hold1State ? HOLD1_LED_ON : HOLD1_LED_OFF);
        Serial.printf ("hold1State changed: %s\n\r", hold1State ? "true" : "false");
    }

    // perform actions based on time of day, sunrise, sunset, holdState
    if (timeValid) {
        now = Time.now ();

        if (todSunrise.CheckTimer (now)) {
            Serial.printf ("Executing sunrise event at %s\n\r", Time.format (now, TIME_FORMAT_DEFAULT).c_str ());
            if (!hold1State) { HunterDouglasPowerviewHubSetScene (HDPV_BATH_TOPDWN_SCENE_ID); }
            if (!hold0State) { HunterDouglasPowerviewHubSetScene (HDPV_BDRM_TOPDWN_SCENE_ID); }
        }

        if (todSunset.CheckTimer (now)) {
            Serial.printf ("Executing sunset event at %s\n\r", Time.format (now, TIME_FORMAT_DEFAULT).c_str ());
            if (!hold1State) { HunterDouglasPowerviewHubSetScene (HDPV_BATH_TOPDWN_SCENE_ID); }
            if (!hold0State) { HunterDouglasPowerviewHubSetScene (HDPV_BDRM_TOPDWN_SCENE_ID); }
        }

        if (tod1pm.CheckTimer (now)) {
            Serial.printf ("Executing 1pm event at %s\n\r", Time.format (now, TIME_FORMAT_DEFAULT).c_str ());
            if (!hold1State) { HunterDouglasPowerviewHubSetScene (HDPV_BATH_CLOSE_SCENE_ID); }
            if (!hold0State) { HunterDouglasPowerviewHubSetScene (HDPV_BDRM_CLOSE_SCENE_ID); }
        }

        if (tod11pm.CheckTimer (now)) {
            Serial.printf ("Executing 11pm event at %s\n\r", Time.format (now, TIME_FORMAT_DEFAULT).c_str ());
            if (!hold1State) { HunterDouglasPowerviewHubSetScene (HDPV_BATH_CLOSE_SCENE_ID); }
        }
    }

    if (connected) {
        // sync clock once a day
        now = Time.now ();
        if (lastDay != Time.day (now)) {
            Serial.printf ("It's a new day or a reboot. A time sync is needed.\n\r");
            lastDay = Time.day (now);
            // don't request anoother sync if one is currently pending
            if (Particle.syncTimePending ()) {
                Serial.printf ("Sync time already pending.\n\r");
                timeSyncRequested = true;
            } else if (Particle.syncTimeDone ()) {
                Serial.printf ("Syncing time...\n\r");
                Particle.syncTime ();
                timeSyncRequested = true;
            }
        }

        // update dst flag, sunrise, sunset after sync is completed
        if (timeSyncRequested) {
            if (Particle.syncTimeDone ()) {
                timeSyncRequested = false;
                Serial.printf ("Time sync complete.\n\r");
                now = Time.now ();
                if (dstActive = IsDST (Time.day(now), Time.month(now), Time.weekday(now))) {
                    Time.beginDST ();
                } else {
                    Time.endDST ();
                }
                timeValid = true;
                Serial.printf ("The time is now %s.\n\r", Time.format (now, TIME_FORMAT_DEFAULT).c_str ());

                // update sunrise / sunset times
                sunrise.updateSolarTimes();

                // set timers
                todSunrise.SetTimer (now, sunrise.sunRiseHour, sunrise.sunRiseMinute);
                todSunset.SetTimer (now, sunrise.sunSetHour, sunrise.sunSetMinute);
                tod1pm.SetTimer (now, 13 - (TIME_ZONE + (dstActive ? 1 : 0)), 00);
                tod11pm.SetTimer (now, 23 - (TIME_ZONE + (dstActive ? 1 : 0)), 00);

                Serial.printf ("Sunrise is at %s.\n\r", todSunrise.GetTimeString().c_str ());
                Serial.printf ("Sunset is at  %s.\n\r", todSunset.GetTimeString().c_str ());
                Serial.printf ("1pm is at     %s.\n\r", tod1pm.GetTimeString().c_str ());
                Serial.printf ("11pm is at    %s.\n\r", tod11pm.GetTimeString().c_str ());
            }
        }
    }
}

int ledToggle (String command)
{
    Serial.printf("received command: %s\n\r", command.c_str());

    if (command=="on") {
        digitalWrite(led1,HIGH);
        return 1;
    } else if (command=="off") {
        digitalWrite(led1,LOW);
        return 0;
    } else {
        return -1;
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

bool IsDST(int dayOfMonth, int month, int dayOfWeek)
{
  if (month < 3 || month > 11)
  {
    return false;
  }
  if (month > 3 && month < 11)
  {
    return true;
  }
  int previousSunday = dayOfMonth - (dayOfWeek - 1);
  if (month == 3)
  {
    return previousSunday >= 8;
  }
  return previousSunday <= 0;
}

void PrintResponse (http_response_t &response)
{
     Serial.println("HTTP Response: ");
     Serial.println(response.status);
     Serial.println(response.body);
}

void MakeGetRequest (String path)
{
    request.path = path;
    request.body = "";
    http.get (request, response, headers);
    PrintResponse (response);
}

void HunterDouglasPowerviewHubSetScene (int sceneId)
{
    String path;

    path = path.format ("/api/scenes?sceneId=%d", sceneId);
    Serial.printf ("Sending http request %s...\n\r", path.c_str ());
    MakeGetRequest (path);
}
