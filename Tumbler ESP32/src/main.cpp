#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <HardwareSerial.h>
#include "LinkedList.h"
#include <TMCStepper.h>
#include <AccelStepper.h>
#include <time.h>

//Constants
//Parameters
String request ;
char* ssid  = "xxxxxxxx";
char* password  = "xxxxxxxxxxx";
String nom  = "ESP32 Tumbler";

bool InRun = false;
bool InstantiateMouvement = false;

// Vars in routine
class ExecuteOrder66 {
    public:
        bool direction = false; // false normal direction
        uint64_t time = 0; // Time execution in seconds
};
int totalTimeToTurn = 0;
int totalTimeToTurnOver = 0;
int totalRPM = 0;
LinkedLists<ExecuteOrder66*> StepOrder = LinkedLists<ExecuteOrder66*>();

#pragma region InterfaceWeb
int TimeToTurn = 0;
int TimeToTurnOver = 0;
int NumberRPM = 0;

String TimeToTurnCustom = "";
String TimeToTurnOverCustom = "";
int NumberRPMCustom = 0;
String TypeRequest = "";

char *html;

AsyncWebServer server(80);
#pragma endregion 

#pragma region TMC2209
#define RX2                GPIO_NUM_16
#define TX2                GPIO_NUM_17
#define DIAG_PIN           GPIO_NUM_27         // STALL motor 2
#define EN_PIN             GPIO_NUM_13         // Enable
#define DIR_PIN            GPIO_NUM_25         // Direction
#define STEP_PIN           GPIO_NUM_14         // Step
#define SERIAL_PORT        Serial2    // TMC2208/TMC2224 HardwareSerial port
#define DRIVER_ADDRESS     0b01       // TMC2209 Driver address according to MS1 and MS2a
#define R_SENSE            0.11f      // E_SENSE for current calc.  
#define STALL_VALUE        2          // [0..255]

#define LARGEGEARTOOTH 170
#define SMALLGEARTOOTH 30
#define NUMBERSTEPPERTURN 1600

hw_timer_t * timer1 = NULL;
TMC2209Stepper driver(&SERIAL_PORT, R_SENSE , DRIVER_ADDRESS );
AccelStepper stepper = AccelStepper(stepper.DRIVER, STEP_PIN, DIR_PIN);

int64_t TimeAtSart;
#pragma endregion

#pragma region Web Function
void print_uint64_t(uint64_t num) {

  char rev[128]; 
  char *p = rev+1;

  while (num > 0) {
    *p++ = '0' + ( num % 10);
    num/= 10;
  }
  p--;
  /*Print the number which is now in reverse*/
  while (p > rev) {
    Serial.print(*p--);
  }
  Serial.println("");
}
void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}
char * TimeToString(int64_t t)
{
    static char str[12];
    long h = t / 3600;
    t = t % 3600;
    int m = t / 60;
    int s = t % 60;
    sprintf(str, "%02d:%02d:%02d", h, m, s);
    return str;
}
String WebPageBuilder() {
   String htmlPreview = R"rawliteral(
    <html>
        <head>
            <title>ESP32 Tumbler</title>
            <meta http-equiv='content-type' content='text/html; charset=UTF-8'>
            <meta name='apple-mobile-web-app-capable' content='yes' />
            <meta name='apple-mobile-web-app-status-bar-style' content='black-translucent' />
            <link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css" integrity="sha384-BVYiiSIFeK1dGmJRAkycuHAHRg32OmUcww7on3RYdg4Va+PmSTsz/K68vbdEjh4u" crossorigin="anonymous">
        </head>
        <body>
            <h1><center>ESP32 Tumbler with TMC2209 (BTT)</center></h1>
            <table style="width:100%; text-align: center;" class="table table-striped">
                <form action="/">
    )rawliteral";
if (!InRun) {
htmlPreview += R"rawliteral(
                    <tr>
                        <td>
                            <label>Processing time :</label>
                            <br>
                            <input type="radio" id="time1" name="timeToTurn" value="2">
                            <label for="time1" style="font-weight: normal;">2h</label><br>

                            <input type="radio" id="time2" name="timeToTurn" value="4" checked>
                            <label for="time2" style="font-weight: normal;">4h</label><br>

                            <input type="radio" id="time3" name="timeToTurn" value="8">
                            <label for="time3" style="font-weight: normal;">8h</label><br>

                            <input type="radio" id="timeCustom" name="timeToTurn" value="0">
                            <label for="timeCustom" style="font-weight: normal;">custom (hours:minute)</label>
                            <br>
                            <input type="text" id="timeCustom" name="timeToTurnCustom" placeholder="1:30">
                        </td>
                    </tr>
                    <tr>
                        <td>
                            <label>change direction time :</label>
                            <br>
                            <input type="radio" id="time1" name="timeToTurnOver" value="1" checked>
                            <label for="direction1" style="font-weight: normal;">1h</label><br>

                            <input type="radio" id="time2" name="timeToTurnOver" value="2">
                            <label for="direction2" style="font-weight: normal;">2h</label><br>

                            <input type="radio" id="time3" name="timeToTurnOver" value="3">
                            <label for="direction3" style="font-weight: normal;">3h</label><br>

                            <input type="radio" id="timeCustomDirection" name="timeToTurnOver" value="0">
                            <label for="timeCustomDirection" style="font-weight: normal;">custom (hours:minute)</label>
                            <br>
                            <input type="text" id="timeCustom" name="timeToTurnOverCustom" placeholder="1:30">
                        </td>
                    </tr>
                    <tr>
                        <td>
                            <label>Rotation per minutes</label>
                            <br>
                            <input type="radio" id="time1" name="RPM" value="3" checked>
                            <label for="time1" style="font-weight: normal;">3</label><br>

                            <input type="radio" id="time2" name="RPM" value="5">
                            <label for="time2" style="font-weight: normal;">5</label><br>

                            <input type="radio" id="time3" name="RPM" value="7">
                            <label for="time3" style="font-weight: normal;">7</label><br>

                            <input type="radio" id="timeCustomRPM" name="RPM" value="0">
                            <label for="timeCustomRPM" style="font-weight: normal;">custom (please enter a integer number)</label>
                            <br>
                            <input type="text" id="timeCustom" name="RPMCustom" placeholder="8">
                        </td>
                    </tr>
                    <tr>
                    <td>
                        )rawliteral";

htmlPreview += R"rawliteral(<input type="submit" name="action" class="btn btn-primary" value="Start">)rawliteral";
} else {
htmlPreview += R"rawliteral(
                        </td>
                    <tr>
                        <td>
                            <input type="submit" name="action" class="btn btn-danger" value="Stop">
                        </td>
                    </tr>
                    <tr>
                        <td>
                            Time remaining for this period: 
                )rawliteral";
ExecuteOrder66 *step = StepOrder.get(0);
int64_t time = step->time - ((esp_timer_get_time() / 1000000) - TimeAtSart);
htmlPreview += TimeToString(time);
htmlPreview += R"rawliteral(
                        </td>
                    </tr>
                    <tr>
                        <td>
                            All time remaining : 
                )rawliteral";
int64_t totalTime = 0;
for (int i = 0; i < StepOrder.size(); i++)
    totalTime += StepOrder.get(i)->time;
int64_t timeT = totalTime - ((esp_timer_get_time() / 1000000) - TimeAtSart);
htmlPreview += TimeToString(timeT);
}
htmlPreview += R"rawliteral(
                        </td>
                    </tr>
                </form>
            </table>
        </body>
    </html>
    )rawliteral";
    return htmlPreview;
}
#pragma endregion

#pragma region TMC2209 Function
float calculateSpeed (int RPM) {
    float ratio = LARGEGEARTOOTH / SMALLGEARTOOTH;
    float numberSteForMinute = RPM * (NUMBERSTEPPERTURN * ratio);
    return numberSteForMinute / 60; // 60 because 60 sec in one minute and SetSpeed get RPS
}
#pragma endregion

void setup() {
    Serial.begin(9600);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println("WiFi Failed!");
        return;
    }
    Serial.println();
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

#pragma region Web
    server.on("/", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String inputMessage;
        String inputParam;

        if (request->hasParam("timeToTurn")) {
            TimeToTurn = request->getParam("timeToTurn")->value().toInt();
        }
        if (request->hasParam("timeToTurnOver")) {
            TimeToTurnOver = request->getParam("timeToTurnOver")->value().toInt();
        }
        if (request->hasParam("RPM")) {
            NumberRPM = request->getParam("RPM")->value().toInt();
        }
        if (request->hasParam("timeToTurnCustom")) {
            TimeToTurnCustom = request->getParam("timeToTurnCustom")->value();
        }
        if (request->hasParam("timeToTurnOverCustom")) {
            TimeToTurnOverCustom = request->getParam("timeToTurnOverCustom")->value();
        }
        if (request->hasParam("RPMCustom")) {
            NumberRPMCustom = request->getParam("RPMCustom")->value().toInt();
        }
        if (request->hasParam("action")) {
            TypeRequest = request->getParam("action")->value();
        }

        if (!InRun && TypeRequest == "Start") {
            if (TimeToTurn == 0) {
                int getDot = TimeToTurnCustom.indexOf(":");
                int hours = TimeToTurnCustom.substring(0, getDot).toInt();
                int minutes = TimeToTurnCustom.substring(getDot + 1, TimeToTurnCustom.length()).toInt();
                totalTimeToTurn = ((hours * 60) + minutes) * 60;
            } else {
                totalTimeToTurn = (TimeToTurn * 60) * 60;
            }

            if (TimeToTurnOver == 0) {
                int getDot = TimeToTurnOverCustom.indexOf(":");
                int hours = TimeToTurnOverCustom.substring(0, getDot).toInt();
                int minutes = TimeToTurnOverCustom.substring(getDot + 1, TimeToTurnOverCustom.length()).toInt();
                totalTimeToTurnOver = (((hours * 60) + minutes) * 60);
            } else {
                totalTimeToTurnOver = (TimeToTurnOver * 60) * 60;
            }

            if (NumberRPM == 0) {
                totalRPM = NumberRPMCustom;
            } else {
                totalRPM = NumberRPM;
            }

            bool direction = false;
            StepOrder.clear();
            while (totalTimeToTurn > 0) {
                if (totalTimeToTurn > totalTimeToTurnOver) {
                    ExecuteOrder66 *item = new ExecuteOrder66();
                    item->direction = direction;
                    item->time = totalTimeToTurnOver;
                    StepOrder.add(item);
                    totalTimeToTurn -= totalTimeToTurnOver;
                } else {
                    ExecuteOrder66 *item = new ExecuteOrder66();
                    item->direction = direction;
                    item->time = totalTimeToTurn;
                    totalTimeToTurn = 0;
                    StepOrder.add(item);
                }
                direction = !direction;
            }

            InRun = true;
            InstantiateMouvement = true;
        }

        if (TypeRequest == "Stop") {
            InRun = false;
            totalTimeToTurn = totalTimeToTurnOver = totalRPM = 0;
        }

        String htmlPreview = WebPageBuilder();
        html = (char *)malloc(sizeof(char) * (htmlPreview.length() + 1));
        for (int i = 0; i < htmlPreview.length(); i++) {
            html[i] = htmlPreview[i];
        }
        html[htmlPreview.length()] = '\0';
        request->send_P(200, "text/html", html);
    });
    server.onNotFound(notFound);
    server.begin();
    #pragma endregion

#pragma region TMC2209
    while(!Serial);
    driver.begin();
    driver.rms_current(1400);
    driver.pwm_autoscale(1);
    driver.microsteps(16);
    stepper.setMaxSpeed(10000);
    stepper.setAcceleration(500);
    stepper.setEnablePin(EN_PIN);
    stepper.setPinsInverted(false, false, true);
    stepper.enableOutputs();
#pragma endregion
}

void loop() {
    if (InstantiateMouvement && InRun) {
        if (StepOrder.size() > 0) {
            ExecuteOrder66 *step = StepOrder.get(0);
            float stepPerSecondes = calculateSpeed(totalRPM);
            if (step->direction)
                stepPerSecondes *= -1;
            stepper.enableOutputs(); // activate stepper
            stepper.setSpeed(stepPerSecondes);
            stepper.setMaxSpeed(stepPerSecondes);
            TimeAtSart = (esp_timer_get_time() / 1000000);
        } else {
            InRun = false;
        }
        InstantiateMouvement = false;
    }
    else if (!InstantiateMouvement && !InRun) {
        totalRPM = 0;
        totalTimeToTurn = 0;
        totalTimeToTurnOver = 0;
        StepOrder.clear();
        stepper.stop();
        stepper.disableOutputs(); // desactivate stepper
    }
    if (InRun) {
        stepper.runSpeed();
        ExecuteOrder66 *step = StepOrder.get(0);
        if (step->time <= ((esp_timer_get_time() / 1000000) - TimeAtSart)) {
            StepOrder.remove(0);
            InstantiateMouvement = true;
        }
    }
}