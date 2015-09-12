#include "HttpClient.h"

#define HOOK_RESP	"hook-response/srxl3xzp_weather_hook"
#define HOOK_PUB	"srxl3xzp_weather_hook"
#define nexSerial Serial1

const unsigned long DISPLAY_UPDATE_INTERVAL = 5000UL; // 5 Seconds
const unsigned long WEATHER_UPDATE_INTERVAL = 1800000UL; // 30 minutes

const char * daysOfWeek[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char * monthsOfYear[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

const int TIMEZONE_OFFSET = 1;
const int gauge_max = 4000;

enum { // WEATHER STATES
  WEATHER_READY,
  WEATHER_REQUESTING,
  WEATHER_AVAILABLE
};

HttpClient http;

// If Content-Length isn't given this is used for the body length increments
const int kFallbackContentLength = 100;

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

http_response_t response;
http_request_t request;

bool firstRun = true;

unsigned long resetTime = 0;

int lastDay;
int lastWattsPic;
int lastWatts;
float lastTemperature;
float lastHumidity;
float lastkWh;

String w_weekday[4];
int w_high[4];
int w_low[4];
int w_icon[4];

int badWeatherCall;
int weather_state = WEATHER_READY;
unsigned long nextWeatherUpdate = 0;
unsigned long nextDisplayUpdate = 0;

void doGetRequest() {
  int err = 0;
  response.status = 0;
  response.body = '\0';
//  unsigned long httpStartTime = millis();

  // Using IP
  err = http.get(request.ip, request.hostname, request.path);

  if (err == 0)
  {
    err = http.responseStatusCode();
    if (err >= 0)
    {
      response.status = err;

      if (err >= 200 || err < 300)
      {
        err = http.skipResponseHeaders();
        if (err >= 0)
        {
          int bodyLen = http.contentLength();

          // Now we've got to the body, so we can print it out
          unsigned long timeoutStart = millis();
          if (bodyLen <= 0)
            bodyLen = kFallbackContentLength;

          char *body = (char *) malloc( sizeof(char) * ( bodyLen + 1 ) );

          char c;
          int i = 0;
          // Whilst we haven't timed out & haven't reached the end of the body
          while ( (http.connected() || http.available()) &&
                 ((millis() - timeoutStart) < http.kHttpResponseTimeout))
          {
            // Let's make sure this character will fit into our char array
            if (i == bodyLen)
            {
              // No it won't fit. Let's try and resize our body char array
              char *temp = (char *) realloc(body, sizeof(char) * (bodyLen+kFallbackContentLength));

              if ( temp != NULL ) //resize was successful
              {
                bodyLen += kFallbackContentLength;
                body = temp;
              }
              else //there was an error
              {
                free(temp);
                break;
              }
            }

            if (http.available())
            {
              c = http.read();

              body[i] = c;
              i++;
              // We read something, reset the timeout counter
              timeoutStart = millis();
            }
            else
            {
              // We haven't got any data, so let's pause to allow some to
              // arrive
              delay(http.kHttpWaitForDataDelay);
            }
          }
          body[i] = '\0';
          //return body;
          response.body = String(body);
          free(body);
        }
      }
    }
  }
  http.stop();

//  Serial.println();
//  Serial.print("http request took : ");
//  Serial.print(millis()-httpStartTime);
//  Serial.println("ms");
}

char *getDayOfMonthSuffix(int n) {
  if (n >= 11 && n <= 13) {
    return "th";
  }
  switch (n % 10) {
    case 1:  return "st";
    case 2:  return "nd";
    case 3:  return "rd";
    default: return "th";
  }
}

String getJsonValue(String name, String json) {
  int start = json.indexOf(name);
  if (start == -1) return "null";
  start = json.indexOf("value",start) + 8;
  if (start == -1) return "null";
  int finish = json.indexOf("\"", start);
  if (finish == -1) return "null";
  return json.substring(start,finish);
}

float getYesterdaykWh(String json) {
  int start = json.indexOf(",")+1;
  if (start == -1) return -1;
  int finish = json.indexOf("]", start);
  if (finish == -1) return -1;
  return json.substring(start,finish).toFloat();
}

void nextionExecute() {
//  nexSerial.println();
  nexSerial.write(0xFF);
  nexSerial.write(0xFF);
  nexSerial.write(0xFF);
}

int nextionrun(String command) {
  nexSerial.write(command);
  nextionExecute();
}

void setPic(const int page, const char* name, const int pic) {
  char buffer[100];
  sprintf(buffer, "page%d.%s.pic=%d", page, name, pic);
  nexSerial.write(buffer);
  nextionExecute();
}

void setText(const int page, const char* name, const char* value) {
  char buffer[100];
  sprintf(buffer, "page%d.%s.txt=\"%s\"", page, name, value);
  nexSerial.write(buffer);
  nextionExecute();
}

void refreshComponent(const char* name) {
  String cmd = "ref ";
  cmd += name;
  nexSerial.write(cmd);
  nextionExecute();
}

void setPage(const int page) {
  nexSerial.write("page "+String(page));
  nextionExecute();
}

int weatherDescriptionToInt(char* description) {
  String w = String(description);

  if (w.indexOf("mostlycloudy") >= 0)
    return 5;
  else if (w.indexOf("mostlysunny") >= 0)
    return 6;
  else if (w.indexOf("partlycloudy") >= 0)
    return 7;
  else if (w.indexOf("partlysunny") >= 0)
    return 8;
  else if (w.indexOf("clear") >= 0)
    return 0;
  else if (w.indexOf("cloudy") >= 0)
    return 1;
  else if (w.indexOf("flurries") >= 0)
    return 2;
  else if (w.indexOf("fog") >= 0)
    return 3;
  else if (w.indexOf("hazy") >= 0)
    return 4;
  else if (w.indexOf("rain") >= 0)
    return 9;
  else if (w.indexOf("sleet") >= 0)
    return 10;
  else if (w.indexOf("snow") >= 0)
    return 11;
  else if (w.indexOf("sunny") >= 0)
    return 12;
  else if (w.indexOf("tstorms") >= 0)
    return 13;
  else //unknown
    return 14;
}

//Updates Weather Forecast Data
void getWeather() {
  weather_state = WEATHER_REQUESTING;
  // publish the event that will trigger our webhook
  Particle.publish(HOOK_PUB);

  unsigned long wait = millis();
  //wait for subscribe to kick in or 5 secs
  while(weather_state == WEATHER_REQUESTING && (millis() < wait + 5000UL))	//wait for subscribe to kick in or 5 secs
    Particle.process();

  if (weather_state == WEATHER_REQUESTING) {
    nextWeatherUpdate = millis() + 60000UL; // If the request failed try again in 1 minute
    weather_state = WEATHER_READY;
    badWeatherCall++;
    if (badWeatherCall > 4)		//If 5 webhook call fail in a row, do a system reset
      System.reset();
	}
}//End of getWeather function

void gotWeatherData(const char *name, const char *data) {
  char t_weekday[4][10];
  char t_high[4][4];
  char t_low[4][4];
  char t_icon[4][20];

  int day = 0;
  int item = 0;
  int pos = 0;
  int i = 0;

  for (i = 0; i < strlen(data); i++) {
    if (data[i] == '~') {
      switch (item)
      {
      	case 0:
      		t_weekday[day][pos] = '\0';
      		break;
      	case 1:
      		t_high[day][pos] = '\0';
      		break;
      	case 2:
      		t_low[day][pos] = '\0';
      		break;
      	case 3:
      		t_icon[day][pos] = '\0';
      		break;
      }

      item++;
      pos = 0;

      if (item >= 4) {
        item = 0;
        day += 1;
      }

      if (day >= 4) {
        break;
      }

    } else if (data[i] != '\"') {
      switch (item)
      {
      	case 0:
      		t_weekday[day][pos] = data[i];
      		break;
      	case 1:
      		t_high[day][pos] = data[i];
      		break;
      	case 2:
      		t_low[day][pos] = data[i];
      		break;
      	case 3:
      		t_icon[day][pos] = data[i];
      		break;
      }
      pos++;
    } else if (data[i] == '\"' && i != 0)
      break;
  }

  for (i = 0; i < 4; i++) {
    w_weekday[i] = String(t_weekday[i]);
    w_high[i] = atoi(t_high[i]);
    w_low[i] = atoi(t_low[i]);
    w_icon[i] = weatherDescriptionToInt(t_icon[i]);
  }

  badWeatherCall = 0;
  weather_state = WEATHER_AVAILABLE;
  nextWeatherUpdate = millis() + WEATHER_UPDATE_INTERVAL;
}

void setup() {
  nexSerial.begin(115200);
  nextionExecute();
  setText(0, "txtLoading", "Initialising...");

  Particle.function("nextionrun", nextionrun);
  Particle.subscribe(HOOK_RESP, gotWeatherData, MY_DEVICES);

  Time.zone(TIMEZONE_OFFSET);

  do
  {
    resetTime = Time.now();
    delay(10);
  } while (resetTime < 1000000 && millis() < 20000);

  request.ip = { 80, 243, 190, 58 };
  request.hostname = "emoncms.org";

  setText(0, "txtLoading", "Downloading weather data...");
  getWeather();
  setText(0, "txtLoading", "Downloading power data...");
}

void loop() {
  long current_time = millis();
  char buffer[100];

  if (current_time > nextWeatherUpdate && weather_state == WEATHER_READY)
    getWeather();

  if (current_time > nextDisplayUpdate)
  {
    // Get request
    request.path = "/feed/list.json?apikey=APIKEY";
    doGetRequest();

    int watts = getJsonValue("watts",response.body).toInt();
    float temperature = getJsonValue("temperature",response.body).toFloat();
    float humidity = getJsonValue("humidity",response.body).toFloat();
    float kWh = getJsonValue("kWhd",response.body).toFloat();

    int wattsPic = (((float) (watts > gauge_max ? (gauge_max-1) : watts) / gauge_max) * 20)+1;

    // WATT HOURS PIC
    if (wattsPic != lastWattsPic)
      setPic(1, "imgPower", wattsPic);

    // CURRENT WATT HOURS
    if (watts != lastWatts) {
      lastWatts = watts;
      setText(1, "txtWh", String(watts)+"W");
    }
    // TODAY'S TOTALWATT HOURS
    if (kWh != lastkWh) {
      lastkWh = kWh;
      setText(1, "txtTodaykWh", String(kWh, 1)+"kWh");
    } else if (wattsPic != lastWattsPic)
      refreshComponent("txtTodaykWh");

    // DATE AND YESTERDAY'S kWh
    if (Time.day() != lastDay) {
      //DATES
      lastDay = Time.day();
      int dayOfWeek = Time.weekday()-1;
      int monthOfYear = Time.month()-1;

      // Example Wednesday 9th September 2015
      //TODO: String s = Time.format(Time.now(), "It's %I:%M%p.")
      //      s == It's 12:45pm
      // String s = Time.format(Time.now(), "%A %e %B %Y")
      sprintf(buffer, "%s %d%s %s %d", daysOfWeek[dayOfWeek], Time.day(), getDayOfMonthSuffix(Time.day()), monthsOfYear[monthOfYear], Time.year());
      setText(1, "txtDate", buffer);

      // YESTERDAY'S kWh
      request.path = "/feed/data.json?apikey=APIKEY&id=4&start=";
      request.path.concat(Time.now()-86400);
      request.path.concat("000");
      request.path.concat("&end=");
      request.path.concat(Time.now());
      request.path.concat("000");
      request.path.concat("&interval=86400&skipmissing=1&limitinterval=0");
      doGetRequest();
      float yesterdaykWh = getYesterdaykWh(response.body);

      setText(1, "txtYestkWh", String(yesterdaykWh, 1)+"kWh");
    } else if(wattsPic != lastWattsPic) {
      refreshComponent("txtDate");
      refreshComponent("txtYestkWh");
    }

    if (humidity != lastHumidity) {
      lastHumidity = humidity;
      setText(1, "txtHumidity", String(humidity, 0)+"%");
    }

    if (temperature != lastTemperature) {
      lastTemperature = temperature;
      setText(1, "txtTemp", String(temperature, 0)+"oC");
    }

    if (wattsPic != lastWattsPic) {
      refreshComponent("lblYestkWh");
      refreshComponent("lblTodaykWh");
      refreshComponent("txtDay1");
      refreshComponent("txtTemp1");
    }

    lastWattsPic = wattsPic;
    nextDisplayUpdate = millis() + DISPLAY_UPDATE_INTERVAL;
  }

  if (weather_state == WEATHER_AVAILABLE) {
    int dayOfWeek = 0;
    for (int i = 0; i < 4; i++) {
      dayOfWeek = Time.weekday()-1;

      if (w_weekday[i].equals(daysOfWeek[dayOfWeek])) {
        setText(1, "txtDay1", daysOfWeek[dayOfWeek]);
        setPic(1, "imgWeather", 38+w_icon[i]);
        setPic(1, "imgDay1", 23+w_icon[i]);

        sprintf(buffer, "%doC  %doC", w_high[i], w_low[i]);
        setText(1, "txtTemp1", buffer);
      }

      dayOfWeek = Time.weekday() >= 7 ? Time.weekday()-7 : Time.weekday();

      if (w_weekday[i].equals(daysOfWeek[dayOfWeek])) {
        setText(1, "txtDay2", daysOfWeek[dayOfWeek]);
        setPic(1, "imgDay2", 23+w_icon[i]);

        sprintf(buffer, "%doC  %doC", w_high[i], w_low[i]);
        setText(1, "txtTemp2", buffer);
      }

      dayOfWeek = Time.weekday()+1 >= 7 ? (Time.weekday()+1)-7 : Time.weekday()+1;

      if (w_weekday[i].equals(daysOfWeek[dayOfWeek])) {
        setText(1, "txtDay3", daysOfWeek[dayOfWeek]);
        setPic(1, "imgDay3", 23+w_icon[i]);

        sprintf(buffer, "%doC  %doC", w_high[i], w_low[i]);
        setText(1, "txtTemp3", buffer);
      }
    }

    weather_state = WEATHER_READY;
  }

  if (firstRun) {
    firstRun = false;
    setPage(1);
    setText(0, "txtLoading", "Booting...");
  }

  delay(10);
}
