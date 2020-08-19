#include "papertrail.h"
#include "Particle.h"
#include "nextiondisplay.h"
#include "Nextion.h"
#include "mqtt.h"
#include "secrets.h"
#include "Adafruit_Sensor.h"
#include "Adafruit_BME280.h"
#include "DiagnosticsHelperRK.h"

// Stubs
void mqttCallback(char* topic, byte* payload, unsigned int length);

Nextion nextion;

bool firstRun = true;
bool isDebug = false;
int sleep_state = STATE_AWAKE;
int sleep_state_request = STATE_AWAKE;

bool sofaOccupied = false;
int sofaPositions[3];

uint32_t resetTime = 0;
retained uint32_t lastHardResetTime;
retained int resetCount;

int lastDay;
int lastUsePic;
int todayIcon = 14;

WeatherData weatherData[4];

int badWeatherCall;
int weather_state = WEATHER_READY;
unsigned long nextWeatherUpdate = 0;

unsigned long pir_detection_time;

bool useUpdated;
bool temperatureUpdated;
bool humidityUpdated;
bool todayKwhUpdated;
bool yesterdayKwhUpdated;

int use; // Current power usage.
int outsideTemperature;
int outsideHumidity;
double totalKwhToday; // KWH used as of today at 00:00 local time.
int lastUpdatedDay; // the day of month the stats where last updated.
float yesterdayKwh;
float todayKwh;

ApplicationWatchdog *wd;

Adafruit_BME280 bme; // I2C
bool bmePresent;
unsigned long nextInternalTemperatureCheck;

MQTT mqttClient(mqttServer, 1883, mqttCallback);
unsigned long lastMqttConnectAttempt;
const int mqttConnectAtemptTimeout = 5000;

PapertrailLogHandler papertrailHandler(papertrailAddress, papertrailPort, "Nextion Display");

// recieve message
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    char p[length + 1];
    memcpy(p, payload, length);
    p[length] = '\0';

    if (strcmp(topic, "emon/particle/power") == 0) {
        int newUse = atoi(p);
        if (newUse != use) {
            use = newUse;
            useUpdated = true;
        }
    } else if (strcmp(topic, "emon/particle/temperature") == 0) {
        int newTemperature = round(atof(p));
        if (newTemperature != outsideTemperature) {
            outsideTemperature = newTemperature;
            temperatureUpdated = true;
        }
    } else if (strcmp(topic, "emon/particle/humidity") == 0) {
        int newHumidity = atoi(p);
        if (newHumidity != outsideHumidity) {
            outsideHumidity = newHumidity;
            humidityUpdated = true;
        }
    } else if (strcmp(topic, "emon/nodered/todaykwh") == 0) {
        float newTodayKwh = round(atof(p)*10) / 10;
        if (newTodayKwh != todayKwh) {
            todayKwh = newTodayKwh;
            todayKwhUpdated = true;
        }
    } else if (strcmp(topic, "emon/nodered/yesterdaykwh") == 0) {
        float newYesterdayKwh = round(atof(p)*10) / 10;
        if (newYesterdayKwh != todayKwh) {
            yesterdayKwh = newYesterdayKwh;
            yesterdayKwhUpdated = true;
        }
    } else if (strcmp(topic, "home/zigbee2mqtt/living_room_occupancy") == 0) {
        if (strstr(p, "\"occupancy\":true"))
            pir_detection_time = millis();
    } else if (strncmp(topic, "home/sofa/seat/", 15) == 0) {
        int seat = topic[15] - '0';
        sofaPositions[seat-1] = atoi(p);
        sofaOccupied = (sofaPositions[0]+sofaPositions[1]+sofaPositions[2]) > 0;
    } else if (strcmp(topic, "utilities/isDST") == 0) {
        if (strcmp(p, "true") == 0)
            Time.beginDST();
        else
            Time.endDST();
        
        if (Time.isDST())
            Log.info("DST is active");
        else
            Log.info("DST is inactive");
    }
}

void connectToMQTT() {
    lastMqttConnectAttempt = millis();
    bool mqttConnected = mqttClient.connect(System.deviceID(), mqttUsername, mqttPassword);
    if (mqttConnected) {
        Log.info("MQTT Connected");
        mqttClient.subscribe("emon/particle/#");
        mqttClient.subscribe("emon/nodered/#");
        mqttClient.subscribe("home/sofa/seat/+/position");
        mqttClient.subscribe("home/zigbee2mqtt/living_room_occupancy");
        mqttClient.subscribe("utilities/#");
    } else
        Log.info("MQTT failed to connect");
}

uint32_t nextMetricsUpdate = 0;
void sendTelegrafMetrics() {
    if (millis() > nextMetricsUpdate) {
        nextMetricsUpdate = millis() + 30000;

        char buffer[150];
        snprintf(buffer, sizeof(buffer),
            "status,device=Nextion uptime=%d,resetReason=%d,firmware=\"%s\",memTotal=%ld,memFree=%ld",
            System.uptime(),
            System.resetReason(),
            System.version().c_str(),
            DiagnosticsHelper::getValue(DIAG_ID_SYSTEM_TOTAL_RAM),
            DiagnosticsHelper::getValue(DIAG_ID_SYSTEM_USED_RAM)
            );
        mqttClient.publish("telegraf/particle", buffer);
    }
}

const char *getDayOfMonthSuffix(int n) {
    if (n >= 11 && n <= 13) {
        return "th";
    }
    switch (n % 10) {
        case 1:  return (char *) "st";
        case 2:  return "nd";
        case 3:  return "rd";
        default: return "th";
    }
}

int weatherDescriptionToInt(char *description, bool mainImage)
{
    if (mainImage) {
        if (strlen(description) >= 11 && (strncmp("clear-night", description, 11) == 0 ))
            return 41;
        else if (strlen(description) >= 19 && (strncmp("partly-cloudy-night", description, 19) == 0 ))
            return 42;
    }
    
    int icon = 8; // Set to unknown
    
    if (strlen(description) >= 5 && (strncmp("clear", description, 5) == 0 ))
        icon = 0;
    else if (strlen(description) >= 6 && (strncmp("cloudy", description, 6) == 0 ))
        icon = 1;
    else if (strlen(description) >= 3 && (strncmp("fog", description, 3) == 0 ))
        icon = 2;
    else if (strlen(description) >= 13 && (strncmp("partly-cloudy", description, 13) == 0 ))
        icon = 3;
    else if (strlen(description) >= 4 && (strncmp("rain", description, 4) == 0 ))
        icon = 4;
    else if (strlen(description) >= 5 && (strncmp("sleet", description, 5) == 0 ))
        icon = 5;
    else if (strlen(description) >= 4 && (strncmp("snow", description, 4) == 0 ))
        icon = 6;
    else if (strlen(description) >= 4 && (strncmp("wind", description, 4) == 0 ))
        icon = 7;
    
    if (mainImage)
        return (icon+32);
    else
        return (icon+23);
}

//Updates Weather Forecast Data
void getWeather()
{
    weather_state = WEATHER_REQUESTING;
    // publish the event that will trigger our webhook
    Particle.publish(WEATHER_HOOK_PUB, "", PRIVATE);
    
    unsigned long wait = millis();
    //wait for subscribe to kick in or 5 secs
    while(weather_state == WEATHER_REQUESTING && (millis() < wait + 5000UL))	//wait for subscribe to kick in or 5 secs
        Particle.process();
    
    if (weather_state == WEATHER_REQUESTING)
    {
        nextWeatherUpdate = millis() + 60000UL; // If the request failed try again in 1 minute
        weather_state = WEATHER_READY;
        badWeatherCall++;
        if (badWeatherCall > 4)		//If 5 webhook calls fail in a row, do a system reset
            System.reset();
    }
} //End of getWeather function

void processWeatherData(const char *name, const char *data) {
    
    char strBuffer[200] = "";

    strncpy(strBuffer, data, 199);
    
    todayIcon = weatherDescriptionToInt(strtok(strBuffer, "~"), true);
    
    for (int i = 0; i <= 3; i++) {
        weatherData[i].weekday = Time.weekday(atoi(strtok(NULL, "~"))) -1;
        weatherData[i].icon = weatherDescriptionToInt(strtok(NULL, "~"), false);
        weatherData[i].high = round(atof(strtok(NULL, "~")));
        weatherData[i].low = round(atof(strtok(NULL, "~")));
    }

    badWeatherCall = 0;
    weather_state = WEATHER_AVAILABLE;
    nextWeatherUpdate = millis() + WEATHER_UPDATE_INTERVAL;
}

int nextionrun(String command)
{
    nextion.run(command);
    return 0;
}

void random_seed_from_cloud(unsigned seed) {
    srand(seed);
}

SYSTEM_THREAD(ENABLED)

void startupMacro() {
    System.enableFeature(FEATURE_RESET_INFO);
    System.enableFeature(FEATURE_RETAINED_MEMORY);
}
STARTUP(startupMacro());

void setup() {
  wd = new ApplicationWatchdog(60000, System.reset, 1536);

  nexSerial.begin(115200);
  delay(200);
  nextion.setSleep(false);
  delay(200);
  nextion.setBrightness(DISPLAY_BRIGHTNESS);
  nextion.setText(0, "txtLoading", "Initialising...");
 
    waitFor(Particle.connected, 30000);
    
    do {
        resetTime = Time.now();
        Particle.process();
    } while (resetTime < 1500000000 || millis() < 10000);
    
    if (System.resetReason() == RESET_REASON_PANIC) {
        if ((Time.now() - lastHardResetTime) < 120) {
            resetCount++;
        } else {
            resetCount = 1;
        }

        lastHardResetTime = Time.now();

        if (resetCount > 3) {
            System.enterSafeMode();
        }
    } else if (System.resetReason() == RESET_REASON_WATCHDOG) {
      Log.info("RESET BY WATCHDOG");
    } else {
        resetCount = 0;
    }
    
    Particle.variable("resetTime", &resetTime, INT);
    Particle.function("nextionrun", nextionrun);
    
    Particle.subscribe(WEATHER_HOOK_RESP, processWeatherData, MY_DEVICES);
    
    Particle.publishVitals(900);

    Log.info("Display is online");

    pir_detection_time = millis();
    
    nextion.setText(0, "txtLoading", "Downloading weather data...");
    getWeather();
    nextion.setText(0, "txtLoading", "Downloading power data...");
    
    connectToMQTT();

    bmePresent = bme.begin();
    if (!bmePresent) {
        Log.info("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
        Log.info("SensorID was: 0x%lx", bme.sensorID());
    } else {
        Log.info("Valid BME280 sensor found");

        bme.setSampling(Adafruit_BME280::MODE_FORCED,
                        Adafruit_BME280::SAMPLING_X1,   // temperature
                        Adafruit_BME280::SAMPLING_NONE, // pressure
                        Adafruit_BME280::SAMPLING_X1,   // humidity
                        Adafruit_BME280::FILTER_OFF);
    }
}

void loop() {
    unsigned long current_millis = millis();

    if (sleep_state == STATE_AWAKE) {
        if (current_millis > nextWeatherUpdate && weather_state == WEATHER_READY)
            getWeather();


        if (useUpdated) {
            useUpdated = false;
            int usePic = (((float) (use > gauge_max ? (gauge_max-1) : use) / gauge_max) * 20)+1;

            // USE DIAL
            if (usePic != lastUsePic) {
                nextion.setPic(1, "imgPower", usePic);
                nextion.refreshComponent("lblYestkWh");
                nextion.refreshComponent("lblTodaykWh");
                nextion.refreshComponent("txtDay1");
                nextion.refreshComponent("txtTemp1");
                nextion.refreshComponent("txtTodaykWh");
                nextion.refreshComponent("txtDate");
                nextion.refreshComponent("txtYestkWh");
            }
            
            lastUsePic = usePic;
        
            // CURRENT USAGE IN WATTS
            nextion.setText(1, "txtWh", String(use)+"W");
        }
    
        // TODAY'S KWH
        if (todayKwhUpdated) {
            todayKwhUpdated = false;
            nextion.setText(1, "txtTodaykWh", String(todayKwh, 1)+"kWh");
        }
        // YESTERDAY'S KWH
        if (yesterdayKwhUpdated) {
            yesterdayKwhUpdated = false;
            nextion.setText(1, "txtYestkWh", String(yesterdayKwh, 1)+"kWh");
        }

        // DATE
        if (Time.day() != lastDay) {
            lastDay = Time.day();
            int dayOfWeek = Time.weekday()-1;
            int monthOfYear = Time.month()-1;
            
            // Example Wednesday 9th September 2015
            nextion.setText(1, "txtDate", String::format("%s %d%s %s %d",
                daysOfWeek[dayOfWeek],
                Time.day(),
                getDayOfMonthSuffix(Time.day()),
                monthsOfYear[monthOfYear],
                Time.year()));
        }


        if (humidityUpdated) {
            humidityUpdated = false;
            nextion.setText(1, "txtHumidity", String::format("%d%%", outsideHumidity));
        }
        
        if (temperatureUpdated) {
            temperatureUpdated = false;
            nextion.setText(1, "txtTemp", String::format("%doC", outsideTemperature));
        }


        if (weather_state == WEATHER_AVAILABLE) {

            nextion.setPic(1, "imgWeather", todayIcon);
            
            for (int i = 0; i <= 3; i++) {            
                nextion.setText(1, "txtDay"+String(i+1), daysOfWeek[weatherData[i].weekday]);
                nextion.setPic(1, "imgDay"+String(i+1), weatherData[i].icon);
                nextion.setText(1, "txtTemp"+String(i+1), String::format("%doC  %doC", weatherData[i].high, weatherData[i].low));
                
            }
            weather_state = WEATHER_READY;
        }

        if (firstRun) {
            firstRun = false;
            nextion.setPage(1);
            nextion.setText(0, "txtLoading", "Booting...");
        }
    }
  
    if (sleep_state == STATE_AWAKE && !sofaOccupied && (current_millis - pir_detection_time) >= DISPLAY_DIM_TIME) {
        sleep_state_request = STATE_DIM_SCREEN;
    } else if (sleep_state == STATE_DIM_SCREEN && (current_millis - pir_detection_time) >= DISPLAY_SLEEP_TIME) {
        sleep_state_request = STATE_SCREEN_OFF;
    } else if (sleep_state > STATE_AWAKE && (current_millis - pir_detection_time) < DISPLAY_DIM_TIME) {
        sleep_state_request = STATE_AWAKE;
    }

    if (sleep_state != sleep_state_request) {
        switch (sleep_state_request) {
            case STATE_AWAKE:
                nextion.setSleep(false);
                delay(200);
                nextion.setBrightness(DISPLAY_BRIGHTNESS);
                if (sleep_state == STATE_SCREEN_OFF)
                    nextion.setText(0, "txtLoading", "Initialising...");
                break;
            case STATE_DIM_SCREEN:
                nextion.setBrightness(0);
                break;
            case STATE_SCREEN_OFF:
                nextion.setPage(0);
                nextion.setSleep(true);
                firstRun = true;
                Log.info("Display is going to sleep");
                break;
        }
        sleep_state = sleep_state_request;
    }


    if (mqttClient.isConnected()) {
        mqttClient.loop();
    } else if (millis() > (lastMqttConnectAttempt + mqttConnectAtemptTimeout)) {
        Log.info("MQTT Disconnected");
        connectToMQTT();
    }

    if (bmePresent && millis() > nextInternalTemperatureCheck) {
        nextInternalTemperatureCheck = millis() + INSIDE_TEMPERATURE_UPDATE_INTERVAL;
        bme.takeForcedMeasurement();
        double insideTemperature = round(bme.readTemperature()*10.0) / 10.0;
        int insideHumidity = (int) round(bme.readHumidity());

        if (isDebug)
            Log.info("Temperature = %.1f *C", insideTemperature);

        if (mqttClient.isConnected()) {
            char buffer[5];
            snprintf(buffer, sizeof buffer, "%.1f", insideTemperature);
            mqttClient.publish("home/livingroom/temperature", buffer, true);
        }

        if (isDebug)
            Log.info("Humidity = %d%%", insideHumidity);

        if (mqttClient.isConnected()) {
            char buffer[4];
            snprintf(buffer, sizeof buffer, "%d", insideHumidity);
            mqttClient.publish("home/livingroom/humidity", buffer, true);
            sendTelegrafMetrics();
        }
    }

    // nextion.setText(1, "txtDebug", String(pir_detection_time));
    wd->checkin();  // resets the AWDT count
}