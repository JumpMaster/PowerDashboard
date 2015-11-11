#ifndef main_h
#define main_h

#define WEATHER_HOOK_RESP	"hook-response/srxl3xzp_weather_hook"
#define WEATHER_HOOK_PUB	"srxl3xzp_weather_hook"

const unsigned long DISPLAY_UPDATE_INTERVAL = 5000; // 5 Seconds
const unsigned long WEATHER_UPDATE_INTERVAL = 1800000; // 30 minutes
const unsigned long DISPLAY_DIM_TIME = 600000; // 10 minutes
const unsigned long DISPLAY_SLEEP_TIME = 1800000; // 30 minutes

const char * daysOfWeek[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char * monthsOfYear[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

const int TIMEZONE_OFFSET = 0;
const int gauge_max = 4000;
const int DISPLAY_BRIGHTNESS = 50;
const int PIR_PIN = WKP;

// If Content-Length isn't given this is used for the body length increments
const int kFallbackContentLength = 100;

const String EMONCMS_URL[] = "emoncms.org";
const byte EMONCMS_IP[] = { 80, 243, 190, 58 };
const String API_KEY[] = "MYAPIKEY";

enum { // WEATHER STATES
  WEATHER_READY,
  WEATHER_REQUESTING,
  WEATHER_AVAILABLE
};

enum { //SLEEP STATES
  STATE_AWAKE,
  STATE_DIM_SCREEN,
  STATE_SCREEN_OFF
};

typedef struct {
 int weekday;
 int high;
 int low;
 int icon;
} WeatherData;

typedef struct
{
  String hostname;
  IPAddress ip;
  String path;
  int port;
  String body;
} http_request_t;

/**
 * HTTP Response struct.
 * status  response status code.
 * body    response body
 */
typedef struct
{
  int status;
  String body;
} http_response_t;

#endif
