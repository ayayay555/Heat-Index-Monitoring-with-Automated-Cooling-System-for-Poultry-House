#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#define relayPin 14
#define SCL 5
#define SDA 4

// Wi-Fi credentials
const char* ssid = "monitor";
const char* password = "monitor123";

// BME280 and relay setup
Adafruit_BME280 bme; // I2C

float TEMP_THRESHOLD = 31;
bool humidifierState = false;

ESP8266WebServer server(80);

// Variables for tracking temperature readings
float totalTemperature = 0;
int totalReadings = 0;
float lastTemperature = 0;
float lastHeatIndex = 0;

void handleData() {
  float currentTemp = bme.readTemperature();
  float currentHumidity = bme.readHumidity();
  //float heatIndex = computeHeatIndex(currentTemp, currentHumidity);
  float heatIndex = 64.52;

  // Store latest values
  lastTemperature = currentTemp;
  lastHeatIndex = heatIndex;

  // Send data as JSON
  String json = "{";
  json += "\"currentTemp\": " + String(currentTemp) + ",";
  json += "\"heatIndex\": " + String(heatIndex);
  json += "}";

  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(9600);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected");
  Serial.println(WiFi.localIP());

  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

  pinMode(relayPin, OUTPUT);
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();

  Serial.println("HTTP server started");
}

float computeHeatIndex(float T, float RH) {
    return T + (0.5555 * (6.112 * exp((17.67 * T) / (T + 243.5)) * RH / 100 - 10));
}

void stateOn() {
  Serial.println("Humidifier ON");
  digitalWrite(relayPin, HIGH);
  delay(50);
  digitalWrite(relayPin, LOW);
  delay(250);
}

void stateOff() {
  Serial.println("Humidifier OFF");
  digitalWrite(relayPin, HIGH);
  delay(50);
  digitalWrite(relayPin, LOW);
  delay(50);
  digitalWrite(relayPin, HIGH);
  delay(50);
  digitalWrite(relayPin, LOW);
  delay(250);
}

void loop() {
  server.handleClient();

  // Read sensor data
  float currentTemp = bme.readTemperature();
  float currentHumidity = bme.readHumidity();
  float heatIndex = computeHeatIndex(currentTemp, currentHumidity);

  Serial.print("Temperature: ");
  Serial.print(currentTemp);
  Serial.println(" °C");

  // Relay control based on temperature threshold
  if (currentTemp >= TEMP_THRESHOLD && !humidifierState) {
    stateOn();
    humidifierState = true;
  } else if (currentTemp < TEMP_THRESHOLD && humidifierState) {
    stateOff();
    humidifierState = false;
  }

  // Update temperature tracking
  totalTemperature += currentTemp;
  totalReadings++;

  delay(5000); // Update every 5 seconds
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Temperature Monitor</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        body {
            font-family: 'Roboto', Arial, sans-serif;
            text-align: center;
            background-color: #f4f4f4;
            margin: 0;
            padding: 0;
        }
        .container {
            display: flex;
            flex-direction: row;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }
        .chart-container {
            width: 320px; 
            height: 180px;
            position: relative;
            margin-right: 20px;
        }
        .heat-index-box {
            width: 140px;
            height: 180px; 
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            font-size: 22px;
            font-weight: bold;
            border: 2px solid black;
            border-radius: 10px;
            padding: 10px;
        }
        .heat-index-label {
            font-size: 14px;
            font-weight: bold;
            margin-bottom: 5px;
        }
    </style>
</head>
<body>
    <h1>Poultry's Heat Stress Monitoring Dashboard</h1>
    <h3>Average Temperature: <span id="averageTemp">0</span> °C</h3>
    <div class="container">
        <div class="chart-container">
            <canvas id="temperatureChart"></canvas>
        </div>
        <div class="heat-index-box" id="heatIndexBox">
            <span class="heat-index-label">Heat Index</span>
            <span id="heatIndex">0 °C</span>
        </div>
    </div>

    <script>
        const ctx = document.getElementById('temperatureChart').getContext('2d');
        const temperatureData = {
            labels: [],
            datasets: [{
                label: 'Temperature (°C)',
                data: [],
                borderColor: 'green', 
                backgroundColor: 'rgba(0, 255, 0, 0.2)',
                borderWidth: 2,
                tension: 0.3,
            }]
        };

        const config = {
            type: 'line',
            data: temperatureData,
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    x: { title: { display: true, text: 'Time' } },
                    y: { title: { display: true, text: 'Temperature (°C)' }, min: 25, max: 35 }
                },
                plugins: { legend: { display: false } }
            }
        };

        const temperatureChart = new Chart(ctx, config);
        let totalTemperature = 0, totalReadings = 0;

        async function updateChartData() {
            const response = await fetch('/data');
            const data = await response.json();
            const Temp = parseFloat(data.currentTemp.toFixed(2));
            const heatIndex = parseFloat(data.heatIndex.toFixed(2));

            // Change graph color based on temperature
            let tempColor = Temp < 31 ? 'green' : 'red';
            temperatureChart.data.datasets[0].borderColor = tempColor;
            temperatureChart.data.datasets[0].backgroundColor = tempColor === 'green' ? 'rgba(0,255,0,0.2)' : 'rgba(255,0,0,0.2)';

            // Change heat index box color
            document.getElementById('heatIndexBox').style.borderColor = heatIndex >= 70 ? 'red' : 'green';

            // Update chart data
            const timeLabel = new Date().toLocaleTimeString();
            temperatureChart.data.labels.push(timeLabel);
            temperatureChart.data.datasets[0].data.push(Temp);

            totalTemperature += Temp;
            totalReadings++;
            document.getElementById('averageTemp').textContent = (totalTemperature / totalReadings).toFixed(2);

            if (temperatureChart.data.labels.length > 12) {
                temperatureChart.data.labels.shift();
                temperatureChart.data.datasets[0].data.shift();
            }
            temperatureChart.update();

            document.getElementById('heatIndex').textContent = heatIndex.toFixed(2) + " °C";
        }

        setInterval(updateChartData, 5000);
    </script>
</body>
</html>
  )rawliteral";

  server.send(200, "text/html", html);
}
