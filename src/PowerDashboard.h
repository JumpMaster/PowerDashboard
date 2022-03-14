#ifndef main_h
#define main_h

#define WEATHER_HOOK_RESP	"hook-response/darkSkyWeather"
#define WEATHER_HOOK_PUB	"darkSkyWeather"

const unsigned long DISPLAY_UPDATE_INTERVAL = 10000; // 10 Seconds
const unsigned long WEATHER_UPDATE_INTERVAL = 7200000; // 2 hours
const unsigned long INSIDE_TEMPERATURE_UPDATE_INTERVAL = 60000; // 1 minute
const unsigned long DISPLAY_DIM_TIME = 600000; // 10 minutes
const unsigned long DISPLAY_SLEEP_TIME = 1800000; // 30 minutes

const char * daysOfWeek[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char * monthsOfYear[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

const int TIMEZONE_OFFSET = 0;
const int gauge_max = 4000;
const int DISPLAY_BRIGHTNESS = 50;
// const int PIR_PIN = WKP;

// If Content-Length isn't given this is used for the body length increments
const int kFallbackContentLength = 100;

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

#endif
