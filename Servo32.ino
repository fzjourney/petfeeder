#include <WiFi.h>
#include <Servo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Your wifi name and password, change it urself
const char *ssid = "HiVe";
const char *password = "punyapeija";

WiFiServer server(80);
Servo servo;

const int servoPin = 18;
const int openAngle = 85;
const int closeAngle = 0;

unsigned long feedingDuration = 800;  // Durasi pembukaan servo untuk memberi makan
unsigned long feedingInterval = 1800; // Interval waktu antara setiap pemberian makan (1,2 detik)
unsigned long lastFeedingTime = 0;    // Waktu terakhir saat pemberian makan dilakukan
bool automaticFeedingEnabled = false; // Flag untuk mengaktifkan/menonaktifkan pemberian makan otomatis

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

int scheduledHour = -1;
int scheduledMinute = -1;

const int maxHistoryCount = 5;
String scheduleHistory[maxHistoryCount];

void setup()
{
    Serial.begin(115200);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    server.begin();

    servo.attach(servoPin);
    servo.write(closeAngle); // Set the initial position of the servo to 0 degrees

    timeClient.begin();
    timeClient.setTimeOffset(25200); // Set the time offset to match your timezone (in seconds)

    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void loop()
{
    timeClient.update(); // Update the current time

    WiFiClient client = server.available();

    if (client)
    {
        Serial.println("New Client.");
        String currentLine = "";
        while (client.connected())
        {
            if (client.available())
            {
                char c = client.read();
                Serial.write(c);
                if (c == '\n')
                {
                    if (currentLine.length() == 0)
                    {
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type:text/html");
                        client.println();

                        client.println("<html>");
                        client.println("<head><title>Smart Pet Feeder</title>");
                        client.println("<style>");
                        client.println("body { font-family: Arial, sans-serif; background-color: #072F5F; color: #ffffff; }");
                        client.println(".container { display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; }");
                        client.println("h1 { text-align: center; font-size: 86px; font-weight: bold; }");
                        client.println(".button { display: inline-block; padding: 10px 20px; font-size: 18px; text-decoration: none; margin-top: 10px; border-radius: 20px; border: none; cursor: pointer; }");
                        client.println(".button-green { background-color: #4CAF50; color: white; }");
                        client.println(".button-red { background-color: #f44336; color: white; }");
                        client.println("label { display: inline-block; width: 60px; text-align: right; }");
                        client.println("input[type=\"number\"] { width: 50px; border-radius: 10px; border: none; padding: 5px; }");
                        client.println("</style>");
                        client.println("</head>");
                        client.println("<body>");
                        client.println("<div class=\"container\">");
                        client.println("<h1>Smart Pet Feeder</h1>");

                        client.println("<h2>Manual Control</h2>");
                        client.println("<div><a href=\"/open\" class=\"button button-green\">Feed Now</a></div>");

                        client.println("<h2>Automatic Feeding</h2>");
                        client.println("<div>");
                        if (automaticFeedingEnabled)
                        {
                            client.println("<a href=\"/disable\" class=\"button button-red\">Disable</a>");
                        }
                        else
                        {
                            client.println("<a href=\"/enable\" class=\"button button-green\">Enable</a>");
                        }
                        client.println("</div>");

                        client.println("<h2>Set Schedule</h2>");
                        client.println("<div>");
                        client.println("<form method=\"POST\" action=\"/set-schedule\">");
                        client.println("<label for=\"hour\">Hour:</label>");
                        client.println("<input type=\"number\" id=\"hour\" name=\"hour\" min=\"0\" max=\"23\">");
                        client.println("<label for=\"minute\">Minute:</label>");
                        client.println("<input type=\"number\" id=\"minute\" name=\"minute\" min=\"0\" max=\"59\">");
                        client.println("<input type=\"submit\" value=\"Set Schedule\" class=\"button button-green\">");
                        client.println("</form>");
                        client.println("</div>");

                        client.println("<h2>Schedule History</h2>");
                        for (int i = 0; i < maxHistoryCount; i++)
                        {
                            client.print("<p>");
                            client.print(scheduleHistory[i]);
                            client.println("</p>");
                        }

                        client.println("</div>");
                        client.println("</body></html>");

                        client.println();
                        break;
                    }
                    else
                    {
                        currentLine = "";
                    }
                }
                else if (c != '\r')
                {
                    currentLine += c;
                }

                if (currentLine.endsWith("GET /open"))
                {
                    feed();
                }
                else if (currentLine.endsWith("GET /enable"))
                {
                    enableAutomaticFeeding();
                }
                else if (currentLine.endsWith("GET /disable"))
                {
                    disableAutomaticFeeding();
                }
                else if (currentLine.endsWith("POST /set-schedule"))
                {
                    handleSetSchedule(client);
                }
            }
        }
        client.stop();
        Serial.println("Client Disconnected.");
    }

    // Periksa pemberian makan otomatis
    if (automaticFeedingEnabled)
    {
        unsigned long currentTime = millis();
        if (currentTime - lastFeedingTime >= feedingInterval)
        {
            feed();
            lastFeedingTime = currentTime;
        }
    }

    // Check if it's time for scheduled feeding
    if (scheduledHour != -1 && scheduledMinute != -1 && timeClient.getHours() == scheduledHour && timeClient.getMinutes() == scheduledMinute)
    {
        feed();
        scheduledHour = -1; // Set scheduledHour dan scheduledMinute kembali ke nilai awal
        scheduledMinute = -1;
    }
}

void feed()
{
    servo.write(openAngle);
    delay(feedingDuration);
    servo.write(closeAngle);
}

void enableAutomaticFeeding()
{
    automaticFeedingEnabled = true;
}

void disableAutomaticFeeding()
{
    automaticFeedingEnabled = false;
}

void handleSetSchedule(WiFiClient &client)
{
    String request = client.readString();
    int hourIndex = request.indexOf("hour=");
    int minuteIndex = request.indexOf("&minute=");
    if (hourIndex != -1 && minuteIndex != -1)
    {
        String hourValue = request.substring(hourIndex + 5, minuteIndex);
        String minuteValue = request.substring(minuteIndex + 8);
        int hour = hourValue.toInt();
        int minute = minuteValue.toInt();
        if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59)
        {
            scheduledHour = hour;
            scheduledMinute = minute;
            updateScheduleHistory(hour, minute);
        }
    }
    client.println("HTTP/1.1 302 Found");
    client.println("Location: /");
    client.println();
}

void updateScheduleHistory(int hour, int minute) 
{
  // Get the current date and time
  String dateString = timeClient.getFormattedTime();
  String timeString = getTimeString(hour, minute);
  String dateTimeString = "Time Taken: " + dateString + " | " + "Time Scheduled: " + timeString;

  // Shift the history entries down
  for (int i = maxHistoryCount - 1; i > 0; i--) {
    scheduleHistory[i] = scheduleHistory[i - 1];
  }

  // Add the new schedule entry at the first position
  scheduleHistory[0] = dateTimeString;
}


String getTimeString(int hour, int minute)
{
    String hourString = (hour < 10) ? "0" + String(hour) : String(hour);
    String minuteString = (minute < 10) ? "0" + String(minute) : String(minute);
    return hourString + ":" + minuteString;
}
