#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>
#include <ESP8266WiFi.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_ADXL345_U.h>

// WiFi Credentials
const char* ssid = "Smart TV";
const char* password = "111000111";

// Server Setup
WiFiServer server(80);

// DHT11 Sensor
#define DHTPIN 2 // GPIO2 (D4)
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// GPS (SoftwareSerial)
#define RXPin 13  // GPS TX -> ESP8266 D7
#define TXPin 15  // GPS RX -> ESP8266 D8
#define GPSBaud 9600
SoftwareSerial gpsSerial(RXPin, TXPin);
TinyGPSPlus gps;

// ADXL345 Sensor
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(123);
float movementThreshold = 2.0; // Adjust sensitivity

void setup() {
    Serial.begin(115200);
    dht.begin();
    gpsSerial.begin(GPSBaud);
    Wire.begin(); // Initialize I2C for ADXL345

    // Start ADXL345
    if (!accel.begin()) {
        Serial.println("No ADXL345 sensor detected!");
        while (1);
    }
    accel.setRange(ADXL345_RANGE_16_G);

    // Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected! IP Address: " + WiFi.localIP().toString());

    server.begin();
}

void loop() {
    WiFiClient client = server.available();
    if (!client) return;

    Serial.println("New Client Connected.");
    while (!client.available()) delay(1);
    String request = client.readStringUntil('\r');
    client.flush();

    // Read DHT11 Sensor
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    if (isnan(temperature) || isnan(humidity)) {
        temperature = 0.0;
        humidity = 0.0;
    }

    // Read GPS Data
    String latitude = "Not Available";
    String longitude = "Not Available";
    String speed = "0";
    String altitude = "0";
    while (gpsSerial.available()) {
        gps.encode(gpsSerial.read());
        if (gps.location.isValid()) {
            latitude = String(gps.location.lat(), 6);
            longitude = String(gps.location.lng(), 6);
        }
        if (gps.speed.isValid()) {
            speed = String(gps.speed.kmph());
        }
        if (gps.altitude.isValid()) {
            altitude = String(gps.altitude.meters());
        }
    }

    // Read ADXL345 Acceleration Data
    sensors_event_t event;
    accel.getEvent(&event);
    float x = event.acceleration.x;
    float y = event.acceleration.y;
    float z = event.acceleration.z;

    // Detect Movement
    static float lastX = 0, lastY = 0, lastZ = 0;
    bool movementDetected = (abs(x - lastX) > movementThreshold || abs(y - lastY) > movementThreshold || abs(z - lastZ) > movementThreshold);
    lastX = x;
    lastY = y;
    lastZ = z;

    // Handle AJAX Request for Data
    if (request.indexOf("/data") != -1) {
        String json = "{";
        json += "\"temperature\": " + String(temperature) + ",";
        json += "\"humidity\": " + String(humidity) + ",";
        json += "\"latitude\": \"" + latitude + "\",";
        json += "\"longitude\": \"" + longitude + "\",";
        json += "\"speed\": " + speed + ",";
        json += "\"altitude\": " + altitude + ",";
        json += "\"x\": " + String(x, 2) + ",";
        json += "\"y\": " + String(y, 2) + ",";
        json += "\"z\": " + String(z, 2) + ",";
        json += "\"movement\": " + String(movementDetected);
        json += "}";

        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println();
        client.println(json);
        return;
    }

    // Serve Webpage
client.println("HTTP/1.1 200 OK");
client.println("Content-Type: text/html");
client.println("Connection: close");
client.println();

client.println("<!DOCTYPE html><html><head>");
client.println("<title> &#128004; ESP8266 Cow Monitoring</title>");
client.println("<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>");

// CSS Styles
client.println("<style>");
client.println("body { font-family: 'Poppins', sans-serif; text-align: center; background-color: #f4f4f4; margin: 0; padding: 0; transition: background 0.3s; }");

/* Header */
client.println(".header { background: #007bff; color: white; padding: 15px; font-size: 22px; font-weight: bold; position: fixed; top: 0; width: 100%; box-shadow: 0px 4px 6px rgba(0, 0, 0, 0.1); }");

/* Containers */
client.println(".container { display: flex; justify-content: center; gap: 15px; margin: 80px auto 20px; width: 90%; flex-wrap: wrap; }");
client.println(".box { background: #007bff; color: white; padding: 20px; border-radius: 12px; font-size: 20px; font-weight: bold; width: 30%; min-width: 250px; box-shadow: 3px 3px 10px rgba(0, 0, 0, 0.2); }");

/* Graph Section */
client.println(".graph-container { background: white; padding: 20px; margin: 20px auto; width: 90%; border-radius: 10px; box-shadow: 3px 3px 10px rgba(0, 0, 0, 0.2); }");
client.println(".graph-title { font-size: 18px; font-weight: bold; margin-bottom: 10px; }");

/* Status Box */
client.println(".status-box { padding: 10px; font-size: 18px; font-weight: bold; border-radius: 10px; display: inline-block; }");

/* Theme Toggle */
client.println(".theme-btn { position: fixed; top: 10px; right: 10px; padding: 8px 15px; background: black; color: white; border: none; border-radius: 5px; cursor: pointer; }");

/* Dark Mode */
client.println(".dark-mode { background-color: #222; color: white; } .dark-mode .box { background: #333; } .dark-mode .graph-container { background: #444; }");

client.println("@media (max-width: 800px) { .container { flex-direction: column; align-items: center; } .box { width: 80%; } }");
client.println("</style></head><body>");

// Header with Emoji
client.println("<div class='header'> &#128004; ESP8266 Cow Monitoring Dashboard</div>");

// Status Indicator
client.println("<div id='status' class='status-box' style='background: lime;'>Status: LIVE &#128994;</div>");

// Theme Toggle Button
client.println("<button onclick='toggleTheme()' class='theme-btn'>&#127769; Dark Mode</button>");

client.println("<div class='container'>");
client.println("<div class='box'>Temperature: <span id='temp'>" + String(temperature) + "</span>°C<br>Humidity: <span id='humidity'>" + String(humidity) + "</span>%</div>");
client.println("<div class='box'>Latitude: <span id='lat'>" + latitude + "</span><br>Longitude: <span id='lon'>" + longitude + "</span></div>");
client.println("<div class='box'>X: <span id='x'>" + String(x, 2) + "</span> m/s²<br>Y: <span id='y'>" + String(y, 2) + "</span> m/s²<br>Z: <span id='z'>" + String(z, 2) + "</span> m/s²</div>");
client.println("</div>");

client.println("<div class='graph-container'><canvas id='sensorChart'></canvas></div>");
client.println("<div class='graph-container'><canvas id='accelChart'></canvas></div>");

// JavaScript for Graphs & Live Updates
client.println("<script>");
client.println("var ctx1 = document.getElementById('sensorChart').getContext('2d');");
client.println("var tempHumChart = new Chart(ctx1, { type: 'line', data: { labels: [], datasets: [{ label: 'Temperature (°C)', borderColor: 'red', data: [] }, { label: 'Humidity (%)', borderColor: 'blue', data: [] }] }, options: { animation: false } });");

client.println("var ctx2 = document.getElementById('accelChart').getContext('2d');");
client.println("var accelChart = new Chart(ctx2, { type: 'line', data: { labels: [], datasets: [{ label: 'X (m/s²)', borderColor: 'green', data: [] }, { label: 'Y (m/s²)', borderColor: 'orange', data: [] }, { label: 'Z (m/s²)', borderColor: 'purple', data: [] }] }, options: { animation: false } });");

// Fetch & Update Data Every Second
client.println("setInterval(() => { fetch('/data').then(res => res.json()).then(data => { ");
client.println("document.getElementById('temp').innerText = data.temperature;");
client.println("document.getElementById('humidity').innerText = data.humidity;");
client.println("document.getElementById('lat').innerText = data.latitude;");
client.println("document.getElementById('lon').innerText = data.longitude;");
client.println("document.getElementById('x').innerText = data.x;");
client.println("document.getElementById('y').innerText = data.y;");
client.println("document.getElementById('z').innerText = data.z;");

// Update Graph Data
client.println("var time = new Date().toLocaleTimeString();");
client.println("tempHumChart.data.labels.push(time);");
client.println("tempHumChart.data.datasets[0].data.push(data.temperature);");
client.println("tempHumChart.data.datasets[1].data.push(data.humidity); tempHumChart.update();");

client.println("accelChart.data.labels.push(time);");
client.println("accelChart.data.datasets[0].data.push(data.x);");
client.println("accelChart.data.datasets[1].data.push(data.y);");
client.println("accelChart.data.datasets[2].data.push(data.z); accelChart.update(); }); }, 1000);");

// Theme Toggle Script
client.println("function toggleTheme() { document.body.classList.toggle('dark-mode'); }");
client.println("</script>");

client.println("</body></html>");

}