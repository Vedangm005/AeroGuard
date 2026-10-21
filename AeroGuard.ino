#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>

const char *WIFI_SSID = "YOUR_WIFI_NAME";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

WebServer server(80);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define SDA_PIN 21
#define SCL_PIN 22

Adafruit_SSD1306 oled(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET);

Adafruit_BME280 bme;

bool bmeAvailable = false;

#define PMS_RX 16
#define PMS_TX 17

HardwareSerial PMS(2);

uint16_t pm1_0 = 0;
uint16_t pm2_5 = 0;
uint16_t pm10_0 = 0;

#define SW420_PIN 27
#define LED_PIN 26
#define BUZZER_PIN 25

volatile bool vibrationInterrupt = false;

bool vibrationDetected = false;

unsigned long lastVibrationTime = 0;

const unsigned long alertDuration = 1500;

unsigned long lastSensorRead = 0;

const unsigned long sensorInterval = 1000;

float temperature = 0.0;
float humidity = 0.0;
float pressure = 0.0;

void IRAM_ATTR handleVibration() {

  vibrationInterrupt = true;
}

bool readPMS5003(
  uint16_t &pm1,
  uint16_t &pm25,
  uint16_t &pm10) {

  while (PMS.available() >= 32) {
    if (
      PMS.read() == 0x42 && PMS.peek() == 0x4D) {
      PMS.read();

      uint8_t buffer[30];

      size_t bytesRead =
        PMS.readBytes(buffer, 30);

      if (bytesRead != 30) {
        return false;
      }
      uint16_t checksum =
        0x42 + 0x4D;

      for (int i = 0; i < 28; i++) {
        checksum += buffer[i];
      }

      uint16_t expectedChecksum =
        ((uint16_t)buffer[28] << 8) | buffer[29];

      if (checksum == expectedChecksum) {
        pm1 =
          ((uint16_t)buffer[8] << 8) | buffer[9];

        pm25 =
          ((uint16_t)buffer[10] << 8) | buffer[11];

        pm10 =
          ((uint16_t)buffer[12] << 8) | buffer[13];

        return true;
      }
    }
  }

  return false;
}

const char MAIN_PAGE[] PROGMEM = R"rawliteral(

<!DOCTYPE html>

<html lang="en">

<head>

<meta charset="UTF-8">

<meta
  name="viewport"
  content="width=device-width, initial-scale=1.0"
>

<title>AeroGuard</title>


<style>


* {
    box-sizing: border-box;
}

html {
    scroll-behavior: smooth;
}

body {

    margin: 0;

    min-height: 100vh;

    font-family:
        Arial,
        Helvetica,
        sans-serif;

    background:
        radial-gradient(
            circle at 10% 0%,
            #102b3c 0%,
            #08151e 35%,
            #040a0f 75%
        );

    color: #edf8ff;
}


.container {

    width: 100%;

    max-width: 1100px;

    margin: auto;

    padding: 28px 24px 20px;
}


.header {

    display: flex;

    justify-content: space-between;

    align-items: center;

    margin-bottom: 26px;
}


.logo {

    display: flex;

    align-items: center;

    gap: 14px;
}


.logoIcon {

    width: 46px;

    height: 46px;

    border-radius: 14px;

    display: flex;

    align-items: center;

    justify-content: center;

    background:
        linear-gradient(
            135deg,
            #00c8ff,
            #0877ff
        );

    color: white;

    font-size: 14px;

    font-weight: 800;

    letter-spacing: 1px;

    box-shadow:
        0 8px 25px
        rgba(0, 180, 255, 0.22);
}


.title {

    font-size: 25px;

    font-weight: 800;

    letter-spacing: 1.5px;
}


.subtitle {

    margin-top: 4px;

    color: #7294a8;

    font-size: 12px;

    letter-spacing: 0.3px;
}


.status {

    display: flex;

    align-items: center;

    gap: 8px;

    padding: 10px 15px;

    border-radius: 30px;

    border:
        1px solid
        rgba(80, 170, 210, 0.2);

    background:
        rgba(15, 35, 46, 0.8);

    color: #e9f8ff;

    font-size: 12px;

    font-weight: 600;
}


.statusDot {

    width: 9px;

    height: 9px;

    border-radius: 50%;

    background: #00e676;

    box-shadow:
        0 0 12px
        rgba(0, 230, 118, 0.9);
}


.alert {

    display: none;

    margin-bottom: 18px;

    padding: 16px 18px;

    border-radius: 15px;

    border:
        1px solid
        rgba(255, 75, 75, 0.7);

    background:
        linear-gradient(
            90deg,
            rgba(110, 25, 25, 0.9),
            rgba(60, 18, 18, 0.8)
        );

    color: #ffb0b0;

    font-size: 14px;
}

.alert.show {
    display: block;
}


.grid {

    display: grid;

    grid-template-columns:
        repeat(3, 1fr);

    gap: 16px;
}


.card {

    background:
        rgba(10, 26, 36, 0.82);

    border:
        1px solid
        rgba(88, 164, 198, 0.18);

    border-radius: 19px;

    padding: 20px;

    box-shadow:
        0 15px 45px
        rgba(0, 0, 0, 0.20);

    transition:
        transform 0.2s ease,
        border-color 0.2s ease;
}

.card:hover {

    transform:
        translateY(-2px);

    border-color:
        rgba(0, 205, 255, 0.42);
}


.cardTop {

    display: flex;

    justify-content:
        space-between;

    align-items: center;

    margin-bottom: 20px;
}


.cardTitle {

    color: #78a0b7;

    font-size: 12px;

    font-weight: 500;

    letter-spacing: 1px;

    text-transform:
        uppercase;
}


.icon {

    display: flex;

    align-items: center;

    justify-content: center;

    width: 35px;

    height: 35px;

    border-radius: 10px;

    background:
        rgba(0, 170, 230, 0.09);

    border:
        1px solid
        rgba(0, 190, 240, 0.14);

    color: #6fdcff;

    font-size: 10px;

    font-weight: 700;

    letter-spacing: 0.5px;
}


.value {

    font-size: 32px;

    font-weight: 700;

    line-height: 1.1;

    color: #f1f8ff;

    margin-bottom: 7px;
}


.unit {

    color: #7895a5;

    font-size: 12px;
}


.chartCard {

    margin-top: 18px;

    min-height: 310px;
}


.chartHeader {

    display: flex;

    justify-content:
        space-between;

    align-items: center;

    margin-bottom: 12px;
}


.liveLabel {

    color: #00e6ff;

    font-size: 11px;

    font-weight: 600;
}


.chart {

    width: 100%;

    height: 235px;

    position: relative;

    overflow: hidden;

    border-radius: 10px;
}


canvas {

    display: block;

    width: 100%;

    height: 100%;
}


.sensorTitle {

    margin-top: 27px;

    margin-bottom: 14px;

    font-size: 17px;

    font-weight: 600;
}


.sensorGrid {

    display: grid;

    grid-template-columns:
        repeat(4, 1fr);

    gap: 12px;
}


.sensor {

    padding: 15px;

    border-radius: 15px;

    background:
        rgba(9, 23, 31, 0.85);

    border:
        1px solid
        rgba(88, 164, 198, 0.16);
}


.sensorName {

    margin-bottom: 9px;

    color: #77a4bc;

    font-size: 11px;

    letter-spacing: 0.5px;
}


.online {

    display: flex;

    align-items: center;

    color: #00e676;

    font-size: 12px;

    font-weight: 600;
}


.dot {

    display: inline-block;

    width: 7px;

    height: 7px;

    margin-right: 6px;

    border-radius: 50%;

    background: #00e676;

    box-shadow:
        0 0 8px
        rgba(0, 230, 118, 0.8);
}


.footer {

    margin-top: 25px;

    text-align: center;

    color: #4d7184;

    font-size: 11px;

    line-height: 1.5;
}


@media (max-width: 800px) {

    .grid {

        grid-template-columns:
            repeat(2, 1fr);
    }

    .sensorGrid {

        grid-template-columns:
            repeat(2, 1fr);
    }

}


@media (max-width: 560px) {

    .container {

        padding:
            20px 14px;
    }

    .header {

        align-items:
            flex-start;

        gap: 15px;
    }

    .title {

        font-size: 21px;
    }

    .subtitle {

        font-size: 10px;
    }

    .status {

        padding:
            8px 10px;

        font-size: 10px;
    }

    .grid {

        grid-template-columns:
            1fr;

        gap: 12px;
    }

    .sensorGrid {

        grid-template-columns:
            1fr 1fr;
    }

    .value {

        font-size: 29px;
    }

}


@media (max-width: 380px) {

    .header {

        flex-direction:
            column;
    }

    .sensorGrid {

        grid-template-columns:
            1fr;
    }

}

</style>

</head>


<body>


<div class="container">


<!-- =========================================================
     HEADER
     ========================================================= -->

<div class="header">

    <div class="logo">

        <div class="logoIcon">
            AG
        </div>

        <div>

            <div class="title">
                AEROGUARD
            </div>

            <div class="subtitle">
                Environmental &amp; Safety Intelligence
            </div>

        </div>

    </div>


    <div class="status">

        <span class="statusDot"></span>

        SYSTEM ONLINE

    </div>

</div>


<!-- =========================================================
     ALERT
     ========================================================= -->

<div
    id="alertBox"
    class="alert"
>

    <strong>SAFETY ALERT</strong>

    &nbsp; Vibration detected by the SW-420 sensor.

</div>


<!-- =========================================================
     SENSOR CARDS
     ========================================================= -->

<div class="grid">


<!-- TEMPERATURE -->

<div class="card">

    <div class="cardTop">

        <div class="cardTitle">
            Temperature
        </div>

        <div class="icon">
            TEMP
        </div>

    </div>

    <div
        id="temperature"
        class="value"
    >
        --.-
    </div>

    <div class="unit">
        &deg;C
    </div>

</div>


<!-- HUMIDITY -->

<div class="card">

    <div class="cardTop">

        <div class="cardTitle">
            Humidity
        </div>

        <div class="icon">
            HUM
        </div>

    </div>

    <div
        id="humidity"
        class="value"
    >
        --.-
    </div>

    <div class="unit">
        %
    </div>

</div>


<!-- PRESSURE -->

<div class="card">

    <div class="cardTop">

        <div class="cardTitle">
            Pressure
        </div>

        <div class="icon">
            HPA
        </div>

    </div>

    <div
        id="pressure"
        class="value"
    >
        ----
    </div>

    <div class="unit">
        hPa
    </div>

</div>


<!-- PM1 -->

<div class="card">

    <div class="cardTop">

        <div class="cardTitle">
            PM1.0
        </div>

        <div class="icon">
            AIR
        </div>

    </div>

    <div
        id="pm1"
        class="value"
    >
        --
    </div>

    <div class="unit">
        &micro;g/m<sup>3</sup>
    </div>

</div>


<!-- PM2.5 -->

<div class="card">

    <div class="cardTop">

        <div class="cardTitle">
            PM2.5
        </div>

        <div class="icon">
            AIR
        </div>

    </div>

    <div
        id="pm25"
        class="value"
    >
        --
    </div>

    <div class="unit">
        &micro;g/m<sup>3</sup>
    </div>

</div>


<!-- PM10 -->

<div class="card">

    <div class="cardTop">

        <div class="cardTitle">
            PM10
        </div>

        <div class="icon">
            AIR
        </div>

    </div>

    <div
        id="pm10"
        class="value"
    >
        --
    </div>

    <div class="unit">
        &micro;g/m<sup>3</sup>
    </div>

</div>

</div>


<!-- =========================================================
     CHART
     ========================================================= -->

<div class="card chartCard">

    <div class="chartHeader">

        <div class="cardTitle">
            Environmental Trend
        </div>

        <div class="liveLabel">
            LIVE
        </div>

    </div>

    <div class="chart">

        <canvas id="chart"></canvas>

    </div>

</div>


<!-- =========================================================
     SENSOR STATUS
     ========================================================= -->

<div class="sensorTitle">
    Sensor Status
</div>


<div class="sensorGrid">


<div class="sensor">

    <div class="sensorName">
        BME280
    </div>

    <div class="online">

        <span class="dot"></span>

        ONLINE

    </div>

</div>


<div class="sensor">

    <div class="sensorName">
        PMS5003
    </div>

    <div class="online">

        <span class="dot"></span>

        ONLINE

    </div>

</div>


<div class="sensor">

    <div class="sensorName">
        SW-420
    </div>

    <div class="online">

        <span class="dot"></span>

        ARMED

    </div>

</div>


<div class="sensor">

    <div class="sensorName">
        OLED DISPLAY
    </div>

    <div class="online">

        <span class="dot"></span>

        ONLINE

    </div>

</div>


</div>


<!-- =========================================================
     FOOTER
     ========================================================= -->

<div class="footer">

    AeroGuard &middot;
    ESP32 Environmental Monitoring System

    <br>

    Last update:
    <span id="time">--:--:--</span>

</div>


</div>


<!-- =========================================================
     JAVASCRIPT
     ========================================================= -->

<script>

const canvas =
    document.getElementById("chart");

const ctx =
    canvas.getContext("2d");

let history = [];


function resizeCanvas() {

    const rect =
        canvas.getBoundingClientRect();

    canvas.width =
        rect.width * window.devicePixelRatio;

    canvas.height =
        rect.height * window.devicePixelRatio;

    ctx.setTransform(
        window.devicePixelRatio,
        0,
        0,
        window.devicePixelRatio,
        0,
        0
    );

    drawChart();
}


window.addEventListener(
    "resize",
    resizeCanvas
);


function drawChart() {

    const width =
        canvas.clientWidth;

    const height =
        canvas.clientHeight;


    ctx.clearRect(
        0,
        0,
        width,
        height
    );

    ctx.lineWidth = 1;

    ctx.strokeStyle =
        "rgba(100,170,200,0.10)";


    for (
        let y = 20;
        y < height;
        y += 40
    ) {

        ctx.beginPath();

        ctx.moveTo(
            0,
            y
        );

        ctx.lineTo(
            width,
            y
        );

        ctx.stroke();

    }


    if (history.length < 2) {
        return;
    }


    let max =
        Math.max(
            ...history,
            10
        );

    let min =
        Math.min(
            ...history,
            0
        );


    let range =
        max - min;

    if (range <= 0) {
        range = 1;
    }

    ctx.beginPath();


    for (
        let i = 0;
        i < history.length;
        i++
    ) {

        const x =
            (i /
            (history.length - 1))
            * width;


        const y =
            height -
            15 -
            (
                (history[i] - min)
                / range
            )
            * (height - 30);


        if (i === 0) {

            ctx.moveTo(
                x,
                y
            );

        } else {

            ctx.lineTo(
                x,
                y
            );

        }

    }


    ctx.lineTo(
        width,
        height
    );

    ctx.lineTo(
        0,
        height
    );

    ctx.closePath();


    ctx.fillStyle =
        "rgba(0,210,255,0.07)";

    ctx.fill();

    ctx.beginPath();


    for (
        let i = 0;
        i < history.length;
        i++
    ) {

        const x =
            (i /
            (history.length - 1))
            * width;


        const y =
            height -
            15 -
            (
                (history[i] - min)
                / range
            )
            * (height - 30);


        if (i === 0) {

            ctx.moveTo(
                x,
                y
            );

        } else {

            ctx.lineTo(
                x,
                y
            );

        }

    }


    ctx.strokeStyle =
        "#00d9ff";

    ctx.lineWidth = 2.5;

    ctx.stroke();

}

function updateData() {

    fetch(
        "/api/data",
        {
            cache: "no-store"
        }
    )

    .then(
        response => response.json()
    )

    .then(
        data => {

            document.getElementById(
                "temperature"
            ).innerText =
                Number(
                    data.temperature
                ).toFixed(1);

            document.getElementById(
                "humidity"
            ).innerText =
                Number(
                    data.humidity
                ).toFixed(1);

            document.getElementById(
                "pressure"
            ).innerText =
                Number(
                    data.pressure
                ).toFixed(0);

            document.getElementById(
                "pm1"
            ).innerText =
                data.pm1;


            document.getElementById(
                "pm25"
            ).innerText =
                data.pm25;


            document.getElementById(
                "pm10"
            ).innerText =
                data.pm10;

            history.push(
                Number(data.pm25)
            );


            if (
                history.length > 50
            ) {

                history.shift();

            }


            drawChart();

            const alertBox =
                document.getElementById(
                    "alertBox"
                );


            if (
                data.vibration === true
            ) {

                alertBox.classList.add(
                    "show"
                );

            } else {

                alertBox.classList.remove(
                    "show"
                );

            }

            document.getElementById(
                "time"
            ).innerText =
                new Date()
                    .toLocaleTimeString();

        }
    )

    .catch(
        error => {

            console.log(
                "Dashboard error:",
                error
            );

        }
    );

}

resizeCanvas();

setInterval(
    updateData,
    1000
);
updateData();


</script>

</body>

</html>

)rawliteral";

void handleData() {

  String json = "{";


  json += "\"temperature\":";
  json += String(
    temperature,
    1);

  json += ",";


  json += "\"humidity\":";
  json += String(
    humidity,
    1);

  json += ",";


  json += "\"pressure\":";
  json += String(
    pressure,
    1);

  json += ",";


  json += "\"pm1\":";
  json += String(
    pm1_0);

  json += ",";


  json += "\"pm25\":";
  json += String(
    pm2_5);

  json += ",";


  json += "\"pm10\":";
  json += String(
    pm10_0);

  json += ",";


  json += "\"vibration\":";

  json +=
    vibrationDetected
      ? "true"
      : "false";


  json += "}";


  server.send(
    200,
    "application/json; charset=utf-8",
    json);
}

void setup() {

  Serial.begin(115200);

  delay(1000);

  pinMode(
    SW420_PIN,
    INPUT_PULLUP);

  pinMode(
    LED_PIN,
    OUTPUT);

  pinMode(
    BUZZER_PIN,
    OUTPUT);


  digitalWrite(
    LED_PIN,
    LOW);

  digitalWrite(
    BUZZER_PIN,
    LOW);
  attachInterrupt(
    digitalPinToInterrupt(
      SW420_PIN),
    handleVibration,
    RISING);

  Wire.begin(
    SDA_PIN,
    SCL_PIN);


  Serial.println();
  Serial.println(
    "================================");

  Serial.println(
    "       AEROGUARD SYSTEM");

  Serial.println(
    "================================");

  if (
    oled.begin(
      SSD1306_SWITCHCAPVCC,
      0x3C)) {

    Serial.println(
      "OLED: OK");

  } else {

    Serial.println(
      "OLED: FAILED");
  }

  if (
    bme.begin(0x76)) {

    bmeAvailable = true;

    Serial.println(
      "BME280: OK @ 0x76");

  }

  else if (
    bme.begin(0x77)) {

    bmeAvailable = true;

    Serial.println(
      "BME280: OK @ 0x77");

  }

  else {

    Serial.println(
      "BME280: FAILED");
  }

  PMS.begin(
    9600,
    SERIAL_8N1,
    PMS_RX,
    PMS_TX);


  Serial.println(
    "PMS5003: UART Started");

  oled.clearDisplay();

  oled.setTextColor(
    SSD1306_WHITE);

  oled.setTextSize(1);


  oled.setCursor(
    15,
    20);

  oled.println(
    "AEROGUARD");


  oled.setCursor(
    30,
    36);

  oled.println(
    "INITIALIZED");


  oled.display();

  delay(1500);

  Serial.println();

  Serial.print(
    "Connecting to WiFi");


  WiFi.mode(
    WIFI_STA);


  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD);


  int attempts = 0;


  while (
    WiFi.status() != WL_CONNECTED && attempts < 20) {

    delay(500);

    Serial.print(".");

    attempts++;
  }


  Serial.println();

  if (
    WiFi.status() == WL_CONNECTED) {

    Serial.println(
      "WiFi connected!");


    Serial.print(
      "IP Address: ");

    Serial.println(
      WiFi.localIP());


    Serial.print(
      "Dashboard: http://");

    Serial.println(
      WiFi.localIP());

  }

  else {

    Serial.println(
      "WiFi connection failed.");

    Serial.println(
      "Starting AeroGuard Access Point...");


    WiFi.mode(
      WIFI_AP);


    WiFi.softAP(
      "AeroGuard",
      "12345678");


    Serial.print(
      "AP IP Address: ");

    Serial.println(
      WiFi.softAPIP());


    Serial.println(
      "WiFi: AeroGuard");

    Serial.println(
      "Password: 12345678");
  }

  server.on(
    "/",
    []() {
      server.send_P(
        200,
        "text/html; charset=utf-8",
        MAIN_PAGE);
    });


  server.on(
    "/api/data",
    handleData);


  server.begin();


  Serial.println(
    "Web server started.");


  Serial.println(
    "================================");
}

void loop() {

  server.handleClient();

  if (
    readPMS5003(
      pm1_0,
      pm2_5,
      pm10_0)) {

    Serial.printf(
      "PM1.0: %d | PM2.5: %d | PM10: %d ug/m3\n",
      pm1_0,
      pm2_5,
      pm10_0);
  }

  if (
    vibrationInterrupt) {

    vibrationInterrupt =
      false;


    vibrationDetected =
      true;


    lastVibrationTime =
      millis();
  }

  if (
    vibrationDetected && (millis() - lastVibrationTime > alertDuration)) {

    vibrationDetected =
      false;
  }

  digitalWrite(
    LED_PIN,
    vibrationDetected
      ? HIGH
      : LOW);


  digitalWrite(
    BUZZER_PIN,
    vibrationDetected
      ? HIGH
      : LOW);

  if (
    millis() - lastSensorRead
    >= sensorInterval) {

    lastSensorRead =
      millis();

    if (
      bmeAvailable) {

      temperature =
        bme.readTemperature();


      humidity =
        bme.readHumidity();


      pressure =
        bme.readPressure()
        / 100.0F;
    }

    Serial.printf(
      "T: %.1f C | "
      "H: %.1f %% | "
      "P: %.1f hPa | "
      "PM2.5: %d | "
      "Vib: %s\n",

      temperature,

      humidity,

      pressure,

      pm2_5,

      vibrationDetected
        ? "ALERT"
        : "OK");

    oled.clearDisplay();

    oled.setTextSize(1);

    oled.setTextColor(
      SSD1306_WHITE);

    oled.setCursor(
      0,
      0);

    oled.print(
      "AEROGUARD");

    oled.setCursor(
      75,
      0);

    oled.print(
      vibrationDetected
        ? "[VIB!]"
        : "[OK]");

    oled.setCursor(
      0,
      16);

    oled.printf(
      "Temp: %.1f C",
      temperature);

    oled.setCursor(
      0,
      28);

    oled.printf(
      "Hum : %.1f %%",
      humidity);

    oled.setCursor(
      0,
      40);

    oled.printf(
      "Pres: %.0f hPa",
      pressure);

    oled.setCursor(
      0,
      52);

    oled.printf(
      "PM2.5: %d ug/m3",
      pm2_5);


    oled.display();
  }
}
