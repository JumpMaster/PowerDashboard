#include "HttpClient.h"
#include "application.h"
#include "main.h"
#include "Nextion.h"

HttpClient http;
Nextion nextion;

http_response_t response;
http_request_t request;

bool firstRun;
retained int sleep_state = STATE_AWAKE;
retained int sleep_state_request = STATE_AWAKE;
bool sofaOccupied = false;

unsigned long resetTime = 0;
String firmwareVersion;
volatile unsigned long pir_detection_time;
int lastDay;
int lastWattsPic;
int lastWatts;
float lastTemperature;
float lastHumidity;
float lastkWh;

WeatherData weatherData[4];

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

String getJsonValue(String name, String json)
{
  int start = json.indexOf(name);
  if (start == -1) return "null";
  start = json.indexOf("value",start) + 8;
  if (start == -1) return "null";
  int finish = json.indexOf("\"", start);
  if (finish == -1) return "null";
  return json.substring(start,finish);
}

float getYesterdaykWh(String json)
{
  int start = json.indexOf(",")+1;
  if (start == -1) return -1;
  int finish = json.indexOf("]", start);
  if (finish == -1) return -1;
  return json.substring(start,finish).toFloat();
}

int weatherDescriptionToInt(String description)
{
  if (description.indexOf("mostlycloudy") >= 0)
    return 5;
  else if (description.indexOf("mostlysunny") >= 0)
    return 6;
  else if (description.indexOf("partlycloudy") >= 0)
    return 7;
  else if (description.indexOf("partlysunny") >= 0)
    return 8;
  else if (description.indexOf("clear") >= 0)
    return 0;
  else if (description.indexOf("cloudy") >= 0)
    return 1;
  else if (description.indexOf("flurries") >= 0)
    return 2;
  else if (description.indexOf("fog") >= 0)
    return 3;
  else if (description.indexOf("hazy") >= 0)
    return 4;
  else if (description.indexOf("rain") >= 0)
    return 9;
  else if (description.indexOf("sleet") >= 0)
    return 10;
  else if (description.indexOf("snow") >= 0)
    return 11;
  else if (description.indexOf("sunny") >= 0)
    return 12;
  else if (description.indexOf("tstorms") >= 0)
    return 13;
  else //unknown
    return 14;
}

//Updates Weather Forecast Data
void getWeather()
{
  weather_state = WEATHER_REQUESTING;
  // publish the event that will trigger our webhook
  Particle.publish(WEATHER_HOOK_PUB);

  unsigned long wait = millis();
  //wait for subscribe to kick in or 5 secs
  while(weather_state == WEATHER_REQUESTING && (millis() < wait + 5000UL))	//wait for subscribe to kick in or 5 secs
    Particle.process();

  if (weather_state == WEATHER_REQUESTING)
  {
    nextWeatherUpdate = millis() + 60000UL; // If the request failed try again in 1 minute
    weather_state = WEATHER_READY;
    badWeatherCall++;
    if (badWeatherCall > 4)		//If 5 webhook call fail in a row, do a system reset
      System.reset();
	}
}//End of getWeather function

void processWeatherData(const char *name, const char *data)
{
  String sData = String(data);
  int day = 0;
  int field = 0;
  int index = 0;
  int next_index = 0;

  while (next_index != -1)
  {
    next_index = sData.indexOf("~", index);
    if (next_index > 0)
    {
      String temp = sData.substring(index, next_index);
      int tempDay = 0;

      switch (field % 4)
      {
        case 0:
          for (int i = 0; i < 7; i++)
          {
            if (temp.equals(daysOfWeek[i]))
            {
              tempDay = i;
              break;
            }
          }

          weatherData[day].weekday = tempDay;
          break;
        case 1:
          weatherData[day].high = temp.toInt();
          break;
        case 2:
          weatherData[day].low = temp.toInt();
          break;
        case 3:
          weatherData[day++].icon = weatherDescriptionToInt(temp);
          break;
      }
      field++;
      if (day > 4) { break; }
    }
    index = next_index+1;
  }

  badWeatherCall = 0;
  weather_state = WEATHER_AVAILABLE;
  nextWeatherUpdate = millis() + WEATHER_UPDATE_INTERVAL;
}

void homeLogEvent(const char *event, const char *data)
{
  String sData = String(data);

  if (sData.substring(0,13).equals("SOFA_OCCUPIED"))
    sofaOccupied = sData.substring(14,15).toInt();
}

void pir_changed()
{
  pir_detection_time = millis();
}

int nextionrun(String command)
{
  nextion.run(command);
}

STARTUP(System.enableFeature(FEATURE_RETAINED_MEMORY));

SYSTEM_THREAD(ENABLED);

void setup() {
  WiFi.useDynamicIP();

  nexSerial.begin(115200);
  nextion.execute();

  nextion.setSleep(false);
  nextion.setBrightness(DISPLAY_BRIGHTNESS);

  nextion.setText(0, "txtLoading", "Initialising...");

  firstRun = true;
  sleep_state = STATE_AWAKE;
  sleep_state_request = STATE_AWAKE;
  firmwareVersion = System.version();

  pinMode(PIR_PIN, INPUT_PULLDOWN);
  attachInterrupt(PIR_PIN, pir_changed, RISING);

  waitUntil(Particle.connected);

  Particle.function("nextionrun", nextionrun);
  Particle.variable("resetTime", resetTime);
  Particle.variable("sysVersion", firmwareVersion);
  Particle.subscribe(WEATHER_HOOK_RESP, processWeatherData, MY_DEVICES);
  Particle.subscribe("HOME_LOG", homeLogEvent, MY_DEVICES);

  Time.zone(TIMEZONE_OFFSET);
  do
  {
    resetTime = Time.now();
    delay(10);
  } while (resetTime < 1000000 && millis() < 20000);

  pir_detection_time = millis();

  request.ip = EMONCMS_IP;
  request.hostname = EMONCMS_URL;
}

void loop() {
  unsigned long current_millis = millis();

  if (sleep_state == STATE_AWAKE)
  {
    if (firstRun)
    {
      nextion.setText(0, "txtLoading", "Downloading weather data...");
      getWeather();
      nextion.setText(0, "txtLoading", "Downloading power data...");
    }

    if (current_millis > nextWeatherUpdate && weather_state == WEATHER_READY)
      getWeather();

    if (current_millis > nextDisplayUpdate)
    {
      // Get request
      request.path = String::format("/feed/list.json?apikey=%s", API_KEY);
      doGetRequest();
      int watts = getJsonValue("watts", response.body).toInt();
      float temperature = getJsonValue("temperature", response.body).toFloat();
      float humidity = getJsonValue("humidity", response.body).toFloat();
      float kWh = getJsonValue("kWhd", response.body).toFloat();

      int wattsPic = (((float) (watts > gauge_max ? (gauge_max-1) : watts) / gauge_max) * 20)+1;

      // WATT HOURS PIC
      if (wattsPic != lastWattsPic)
        nextion.setPic(1, "imgPower", wattsPic);

      // CURRENT WATT HOURS
      if (watts != lastWatts) {
        lastWatts = watts;
        nextion.setText(1, "txtWh", String(watts)+"W");
      }
      // TODAY'S TOTALWATT HOURS
      if (kWh != lastkWh) {
        lastkWh = kWh;
        nextion.setText(1, "txtTodaykWh", String(kWh, 1)+"kWh");
      } else if (wattsPic != lastWattsPic)
        nextion.refreshComponent("txtTodaykWh");

      // DATE AND YESTERDAY'S kWh
      if (Time.day() != lastDay) {
        //DATES
        lastDay = Time.day();
        int dayOfWeek = Time.weekday()-1;
        int monthOfYear = Time.month()-1;

        // Example Wednesday 9th September 2015
        nextion.setText(1, "txtDate", String::format("%s %d%s %s %d",
          daysOfWeek[dayOfWeek], Time.day(), getDayOfMonthSuffix(Time.day()),
          monthsOfYear[monthOfYear], Time.year()));

        // YESTERDAY'S kWh
        request.path = String::format(
          "/feed/data.json?id=4&start=%d000&end=%d000&interval=86400&skipmissing=1&limitinterval=0&apikey=%s",
          Time.now()-86400, Time.now(), API_KEY);
        doGetRequest();
        float yesterdaykWh = getYesterdaykWh(response.body);

        nextion.setText(1, "txtYestkWh", String(yesterdaykWh, 1)+"kWh");
      } else if(wattsPic != lastWattsPic) {
        nextion.refreshComponent("txtDate");
        nextion.refreshComponent("txtYestkWh");
      }

      if (humidity != lastHumidity) {
        lastHumidity = humidity;
        nextion.setText(1, "txtHumidity", String(humidity, 0)+"%");
      }

      if (temperature != lastTemperature) {
        lastTemperature = temperature;
        nextion.setText(1, "txtTemp", String(temperature, 0)+"oC");
      }

      if (wattsPic != lastWattsPic) {
        nextion.refreshComponent("lblYestkWh");
        nextion.refreshComponent("lblTodaykWh");
        nextion.refreshComponent("txtDay1");
        nextion.refreshComponent("txtTemp1");
      }

      lastWattsPic = wattsPic;
      nextDisplayUpdate = millis() + DISPLAY_UPDATE_INTERVAL;
    }
    if (weather_state == WEATHER_AVAILABLE)
    {
      for (int i = 0; i < 4; i++)
      {
        for (int a = -1; a < 2; a++)
        {
          int dayOfWeek = Time.weekday()+a >= 7 ? Time.weekday()-7+a : Time.weekday()+a;

          if (dayOfWeek == weatherData[i].weekday)
          {
            if (a < 0)
              nextion.setPic(1, "imgWeather", 38+weatherData[i].icon);

            nextion.setText(1, "txtDay"+String(a+2), daysOfWeek[dayOfWeek]);
            nextion.setPic(1, "imgDay"+String(a+2), 23+weatherData[i].icon);
            nextion.setText(1, "txtTemp"+String(a+2), String::format(
              "%doC  %doC", weatherData[i].high, weatherData[i].low));
          }
        }
      }
      weather_state = WEATHER_READY;
    }

    if (firstRun) {
      firstRun = false;
      nextion.setPage(1);
      nextion.setText(0, "txtLoading", "Booting...");
    }
  }

  if (sleep_state != sleep_state_request)
  {
    switch (sleep_state_request)
    {
      case STATE_AWAKE:
        nextion.setBrightness(DISPLAY_BRIGHTNESS);
        break;
      case STATE_DIM_SCREEN:
        nextion.setBrightness(0);
        break;
      case STATE_SCREEN_OFF:
        nextion.setPage(0);
        nextion.setSleep(true);
        Particle.publish("HOME_LOG", "SLEEP", 60, PRIVATE);

        while (millis() < (current_millis+5000UL))
          Particle.process();

        System.sleep(SLEEP_MODE_DEEP);
        break;
    }
    sleep_state = sleep_state_request;
  }

  if (sleep_state == STATE_AWAKE && !sofaOccupied && (current_millis - pir_detection_time) >= DISPLAY_DIM_TIME)
    sleep_state_request = STATE_DIM_SCREEN;
  else if (sleep_state == STATE_DIM_SCREEN && (current_millis - pir_detection_time) >= DISPLAY_SLEEP_TIME)
    sleep_state_request = STATE_SCREEN_OFF;
  else if (sleep_state > STATE_AWAKE && (current_millis - pir_detection_time) < DISPLAY_DIM_TIME)
    sleep_state_request = STATE_AWAKE;

  //nextion.setText(1, "txtDebug", String(pir_detection_time));

  delay(10);
}
