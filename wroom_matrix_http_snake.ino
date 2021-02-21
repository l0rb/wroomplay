#include <queue>
#include <set>
#include <LEDMatrixDriver.hpp>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SD.h>
#include <IniFile.h>

#include "src/SnakeGame.h"
//#include <SnakeGame.h>

// === wifi/http === //
char* ssid; // value from read_conf()
char* password; // value from read_conf()
unsigned long currentTime = millis();
unsigned long previousTime = 0; 
const long timeoutTime = 2000;
String header;
WiFiServer server(80);

// === ledmatrix === //
const uint8_t LEDMATRIX_CS_PIN = 16;
const int LEDMATRIX_SEGMENTS = 4; // Number of 8x8 segments you are connecting
const int LEDMATRIX_WIDTH    = LEDMATRIX_SEGMENTS * 8;
LEDMatrixDriver lmd(LEDMATRIX_SEGMENTS, LEDMATRIX_CS_PIN);

// === sd-card/config === //
const uint8_t SDCARD_CS_PIN = 5;
const char* CONFIG_FILENAME = "/lorbconf.txt";

void read_conf()
{
    if (!SD.begin(SDCARD_CS_PIN))
    {
        Serial.println("SD.begin() failed");
        return;
    }
    IniFile ini(CONFIG_FILENAME);
    if (!ini.open())
    {
        Serial.println("File doesn't seem to exist");
        return;
    }
    const size_t bufferLen = 40;
    char buffer[bufferLen];

    if (ini.getValue("wifi", "ssid", buffer, bufferLen))
    {
        ssid = (char*) malloc(sizeof(char) * (strlen(buffer)+1));
        strncpy(ssid, buffer, strlen(buffer)+1);
    }
    if (ini.getValue("wifi", "password", buffer, bufferLen))
    {
        password = (char*) malloc(sizeof(char) * (strlen(buffer)+1));
        strncpy(password, buffer, strlen(buffer)+1);
    }
}

void sp(int x, int y, bool s)
{
    lmd.setPixel(x,y,s);
}

TaskHandle_t tskhttp;
TaskHandle_t tskmatrix;

QueueHandle_t q = NULL;
SnakeGame g = SnakeGame(&sp, LEDMATRIX_WIDTH, 8);

void connect_wifi()
{
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void setup() {
    Serial.begin(9600);
    Serial.println("setting up");

    q = xQueueCreate(20,sizeof(char));

    //Serial.println(wc.ssid);

    read_conf();
    digitalWrite(SDCARD_CS_PIN, HIGH);

    connect_wifi();
    server.begin();
    lmd.setEnabled(true);
    lmd.setIntensity(2);   // 0 = low, 10 = high


    xTaskCreatePinnedToCore(
                    Task_http,   /* Task function. */
                    "tskhttp",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &tskhttp,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */
    delay(500);
    xTaskCreatePinnedToCore(
                    Task_matrix,   /* Task function. */
                    "tskmatrix",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &tskmatrix,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */
}

void send_file(WiFiClient client, char* filename)
{
    // make sure spi chipselect is active only for the sd card:
    digitalWrite(LEDMATRIX_CS_PIN, HIGH);
    digitalWrite(SDCARD_CS_PIN, LOW);
    if (!SD.begin(SDCARD_CS_PIN))
    {
        Serial.println("SD.begin() failed");
        return;
    }
    File file_to_send = SD.open(filename);
    while (file_to_send.available()) {
        client.write(file_to_send.read());
    }
    client.println();
    digitalWrite(SDCARD_CS_PIN, HIGH);
    return;
}
void send_response_headers(WiFiClient client)
{
    // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
    // and a content-type so the client knows what's coming, then a blank line:
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println("Connection: close");
    client.println();
}
void send_response_html(WiFiClient client)
{
    return send_file(client, "/snake_controls.html");

    client.println("<!DOCTYPE html><html>");
    client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
    client.println("<link rel=\"icon\" href=\"data:,\">");

    client.println("<style>html { font-family:Helvetica; display:inline-block; margin:0px auto; text-align: center;}");
    client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
    client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
    client.println("</style>");

    client.println("<body><h1>Muff go Wroom!</h1>");

    client.println("<table>");
    client.println("<tr><td>&nbsp;</td>");
    client.println("<td><a href=\"/up\"><button class=\"button\">U</button></a></td>");
    client.println("<td>&nbsp;</td></tr>");

    client.println("<tr><td><a href=\"/left\"><button class=\"button\">L</button></a></td>");
    client.println("<td>&nbsp;</td>");
    client.println("<td><a href=\"/right\"><button class=\"button\">R</button></a></td></tr>");

    client.println("<tr><td>&nbsp;</td>");
    client.println("<td><a href=\"/down\"><button class=\"button\">D</button></a></td>");
    client.println("<td>&nbsp;</td></tr>");
    client.println("</table>");

    client.println("</body></html>");

    // The HTTP response ends with another blank line
    client.println();
}

void loop_http()
{
    char signal;

    if(q == NULL)
    {
        Serial.println("Http: Queue is not ready.");
        return;
    }

    WiFiClient client = server.available();
    
    if(!client)
    {
        return;
    }

    // loop while the client's connected
    String currentLine = "";
    currentTime = millis();
    previousTime = currentTime;

    while(client.connected() && currentTime - previousTime <= timeoutTime)
    {
        currentTime = millis();
        
        if (!client.available())
        {
            continue;
        }

        char c = client.read();
        Serial.write(c);
        header += c;

        if (c == '\r')
        {
            continue;
        }
        if (c != '\n')
        {
            currentLine += c;
            continue;
        }

        // if we got a newline, and previous line wasn't empty: 
        // clear currentLine and continue with next line
        if (currentLine.length() != 0)
        {
            currentLine = "";
            continue;
        }

        if (header.indexOf("GET /up") >= 0)
        {
            Serial.println("/up");
            signal = 'U';
            xQueueSend(q,(void *)&signal,(TickType_t )0);
        }
        if (header.indexOf("GET /down") >= 0)
        {
            Serial.println("/down");
            signal = 'D';
            xQueueSend(q,(void *)&signal,(TickType_t )0);
        }
        if (header.indexOf("GET /left") >= 0)
        {
            Serial.println("/left");
            signal = 'L';
            xQueueSend(q,(void *)&signal,(TickType_t )0);
        }
        if (header.indexOf("GET /right") >= 0)
        {
            Serial.println("/right");
            signal = 'R';
            xQueueSend(q,(void *)&signal,(TickType_t )0);
        }

        // if the current line is blank, you got two newline characters in a row.
        // that's the end of the client HTTP request, so send a response:
        send_response_headers(client);
        send_response_html(client);

        // Break out of the while loop
        break;
    }
    header = "";
    client.stop(); // Close the connection
    Serial.println("Client disconnected.");
    Serial.println("");
}
void Task_http(void * args)
{
    for(;;)
    {
        vTaskDelay( pdMS_TO_TICKS( 10 ) );
        loop_http();
    }
}

TickType_t delay_time = 500/portTICK_PERIOD_MS;

void loop_matrix()
{
    char signal = 0;
    if(q == NULL)
    {
        Serial.println("Matrix: Queue is not ready.");
        return;
    }

    xQueueReceive(q,&signal,delay_time/5);
    if( signal > 0 )
    {
        Serial.println("got signal!");
        g.input(signal);
    }
    
    g.step();
    lmd.display();
    vTaskDelay(delay_time);
}
void Task_matrix(void * args)
{
    for(;;)
    {
        loop_matrix();
    }
}

void loop()
{
  delay(10);
  vTaskDelete( NULL );
}
