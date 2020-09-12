
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include "FS.h"
#include "SPIFFS.h"

#define FORMAT_SPIFFS_IF_FAILED true

//// The MQTTT endpoint for the device (unique for each AWS account but shared amongst devices within the account)
//#define AWS_IOT_ENDPOINT "akp17u3blui5g-ats.iot.us-west-2.amazonaws.com"

//// The MQTT topic that this device should publish to
//#define AWS_IOT_TOPIC "Iot_mqtt"

#define MAX_ERROR_THRESHOLD 50 //(APPROX 1 MINUTE)

// How many times we should attempt to connect to AWS
#define AWS_MAX_RECONNECT_TRIES 50
// Update these with values suitable for your network.

// Amazon's root CA. This should be the same for everyone.
const char AWS_CERT_CA[] = "-----BEGIN CERTIFICATE-----\n" \
                           "MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n" \
                           "ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n" \
                           "b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n" \
                           "MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n" \
                           "b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n" \
                           "ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n" \
                           "9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n" \
                           "IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n" \
                           "VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n" \
                           "93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n" \
                           "jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n" \
                           "AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n" \
                           "A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n" \
                           "U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n" \
                           "N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n" \
                           "o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n" \
                           "5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n" \
                           "rqXRfboQnoZsG4q5WTP468SQvvG5\n" \
                           "-----END CERTIFICATE-----\n";

char* ver = "0.01";
bool heartbeat = false, sen = false, conf, Con_type;
int bid = 0, mid = 0, noErr = 0;
byte mac[6];
String sendata = "";
String rd;

//Wifi credentials
String ssid, password;

//Topics
String TOPIC_SUB, TOPIC_PUB;

String commands[] = {"write", "read", "subscribe"};
//Aws credentials
String AWS_IOT_ENDPOINT, AWS_IOT_THING_NAME, AWS_CERT_PRIVATE, AWS_CERT_CRT;
//Other platform credentials
String ClientID, Username, UserPass, BrokerAdd, PortNo ;

WiFiClientSecure net = WiFiClientSecure();
MQTTClient client = MQTTClient(1024);

WiFiClient espClient;
PubSubClient client_esp(espClient);

// The private key for your device(just to check recieved one is matched)
char AWS_CERT_PRIVATE_ck[] = "-----BEGIN RSA PRIVATE KEY-----\n" \
                             "MIIEpAIBAAKCAQEAs5EixDgzOrW+Cc/Uev/HXJL26a/pK+EzhS8iAoXhJgVWrmsb\n" \
                             "kfhH/IWajdRvVth2d/TJVLjliYYf6zAXSn8CIS31HP/i2BkFrWMLxO9cNebB3xps\n" \
                             "+CQNPpRtHBd4yV3ufizb/leqXmYU+bLIjwEg09tSjRqC3uBi5teoJ+b8bRXZvm66\n" \
                             "e4c9Gddi2GVak+kuiimTxSIwU4WlN2DEzbfRaZZ6waXgxppvOpHNXOihtffeyZpW\n" \
                             "ZKXIRXSwkqaiBxO3pfWNXT+Sqh8iVGiHgl/Xxq/67Pxqyl2GiZKf8bOqgiH+cneK\n" \
                             "RqGbkASqQfPuMrtxK7dUo38Tmoh3PWZEYBvUdwIDAQABAoIBAAv3HNj0Yb2ExMAE\n" \
                             "oET97Dvn8xoJRcFNxVAXnu2KHEGbU3ZV3sVwROO3x1+yCyU/UU2W+x9xHqJ2VIQo\n" \
                             "dTTal7q8RDwFdQkvSaiPFAawaHWTBdInAaHbTSKhY0/e5IaOgsjXlmUxVEHsDXPC\n" \
                             "DQkyawyS7cJHRPcy/oQhVKwsASAHmwou0kkd3W+ER/Nv0eOEFCjb0l2Vpn9jNYAQ\n" \
                             "tLOJ7TpMhQ78dWYEaWC6+ovpHLwYNML97D3CGq09NdKBW3SQSIGscXwgP+Tp3Ny7\n" \
                             "nlnVqwPlRV0nmC0wPqVCHovPaJzLSN+nn+5IWMASzrWZ9eAbiMZx4SQquT0UoF5E\n" \
                             "4Gxo1LECgYEA6MgZE92wOAo76QqqOgXLXdZXRsfrrD+g85MORvKbi7g401OSQIX6\n" \
                             "tn9AYl2RBcyo+ifNDyXiuPySCBM6c7wCGGXCWqPSsOHVoEqcMHAHZAldCBaHL6Bu\n" \
                             "kkQKRdZR4/i1dTxZvBa2hu4cP4d/PbZmwmQdSVUYyauxyawYCnxHHY8CgYEAxXo9\n" \
                             "tLB5BGTNCsqFLSOaZ6gKJ2OQq8MhUKp3JMun+xzzeDXFDPemu6eKrMb35qRBaygf\n" \
                             "Rq16SPK362bKgU5O75+UgCFCcFcacrQBvUfsp/Aw0M/0EY4mxYVTmqdBZEZ7+RCR\n" \
                             "yHKc/q7yoTjr9MaUh0UhbOiF+rAuHWxrFf8xNpkCgYEAr/GmOsTCD+l0VPVRqt98\n" \
                             "UiXS+9XaBOxm/BO3o9p1xQpuMRSmo4xg7pWKFY9BMQ/63HE+5ect0cJdoirecGG3\n" \
                             "d7daSmYutrFLZYdfPKFAhNUq8xUMAuyRBo7U8OpIJTZz+POvo6HLPns08LO6ceuv\n" \
                             "Cdjf5fCi9rOGgrdHyI0ct3MCgYAJ6RWZsNWR8+Eabol6d3PzScqgqW2EQTm1y6hJ\n" \
                             "D3NxtcU+PiySdwdGGaVrAF1GlO23i/7t1Bzz9kJmrPTywlRR0EdqmsCz1Js+MGx5\n" \
                             "7FcjInnAsP8FtoWZmhRVCZnNh4AHQt6eGappWaxRjQLCeQjRNRX1WkIHD7pwvZUu\n" \
                             "OG1m2QKBgQDdgK2Daj3CI6cA7/ngTPXwfKAZrUXkxlu+3+Xi2H4r0C8uw2Lkvboh\n" \
                             "rh72Kjj2cW7etLo6k+LHn9BuGtHHfY1FyEGVM49HsIeTq9LLbnWeyUtbfI/M/VZA\n" \
                             "EYaxfV5Ouv85GCzVIQGOwwlNg3otLcg0my/pwNTDPJka3zUdZ+z+Ig==\n" \
                             "-----END RSA PRIVATE KEY-----\n";

// The certificate for your device(just to check recieved one is matched)
char AWS_CERT_CRT_ck[] = "-----BEGIN CERTIFICATE-----\n" \
                         "MIIDWTCCAkGgAwIBAgIUfztjQb+CIna9soTp3BXMO1NqQ+QwDQYJKoZIhvcNAQEL\n" \
                         "BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g\n" \
                         "SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTIwMDgwMzA5Mzc0\n" \
                         "MVoXDTQ5MTIzMTIzNTk1OVowHjEcMBoGA1UEAwwTQVdTIElvVCBDZXJ0aWZpY2F0\n" \
                         "ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALORIsQ4Mzq1vgnP1Hr/\n" \
                         "x1yS9umv6SvhM4UvIgKF4SYFVq5rG5H4R/yFmo3Ub1bYdnf0yVS45YmGH+swF0p/\n" \
                         "AiEt9Rz/4tgZBa1jC8TvXDXmwd8abPgkDT6UbRwXeMld7n4s2/5Xql5mFPmyyI8B\n" \
                         "INPbUo0agt7gYubXqCfm/G0V2b5uunuHPRnXYthlWpPpLoopk8UiMFOFpTdgxM23\n" \
                         "0WmWesGl4MaabzqRzVzoobX33smaVmSlyEV0sJKmogcTt6X1jV0/kqofIlRoh4Jf\n" \
                         "18av+uz8aspdhomSn/GzqoIh/nJ3ikahm5AEqkHz7jK7cSu3VKN/E5qIdz1mRGAb\n" \
                         "1HcCAwEAAaNgMF4wHwYDVR0jBBgwFoAUT7ZA/sWeoybOcHF+smwVqjbHB5gwHQYD\n" \
                         "VR0OBBYEFBIA1LzX6SE7jQKuKpYdowm6m38xMAwGA1UdEwEB/wQCMAAwDgYDVR0P\n" \
                         "AQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQBJZ4h97I77Y2hPcYcpOL7zH3RU\n" \
                         "AmButcDFvH3rTCSjONfqZlyvuW6VJegUvBPIBxcD62F3Zu+9ILur41bq5SjIfCG0\n" \
                         "Z6DDRKBNEY6ny8wekgyB0BVwhj2BJsH/UnKQItjzZz+JMvIJWGtYOPH/meUvkYOa\n" \
                         "5xMuhTcR1llVgW17Gep17N8UyFqw91Mu62Br1ieg4RwwhQ/XO+OAswxyHz4VVuyH\n" \
                         "J2cAHZ16GRLfDomlWaqYAOqSFMpkj86a/MZyWr8SpmMH7LOSDCEUH8ZmvLV6Kd2S\n" \
                         "X3D6wFD0TYBp0KaAS/hTzzrD57FdoiOSnFoNIsU+vgfWnMNxYBpoiTDGVeUI\n" \
                         "-----END CERTIFICATE-----\n";


void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\r\n", dirname);
  File root = fs.open(dirname);
  if (!root) {
    Serial.println("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(" - not a directory");
    return;
  }
  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void readFile(fs::FS &fs, const char * path) {
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return;
  }

  Serial.println("- read from file:");
  while (file.available()) {
    Serial.write(file.read());
  }
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
}


void setup_wifi() {
  if (WiFi.status() != WL_CONNECTED) {
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    char conv1[ssid.length() + 1];
    ssid.toCharArray(conv1, ssid.length() + 1);
    char conv2[password.length() + 1];
    password.toCharArray(conv2, password.length() + 1);
    Serial.println(conv1);
    Serial.println(conv2);
    WiFi.mode(WIFI_STA);
    WiFi.begin(conv1, conv2);
    Serial.print("Connecting to wifi");
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 50) {
      delay(500);
      Serial.print(".");
      retries++;
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Could not find such wifi, please recheck!!");
      return;
    }
    //randomSeed(micros());
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void connectToAWS()
{
  // Configure WiFiClientSecure to use the AWS certificates we generated
  net.setCACert(AWS_CERT_CA);

  char cert[AWS_CERT_CRT.length() + 1];
  AWS_CERT_CRT.toCharArray(cert, AWS_CERT_CRT.length() + 1);
  net.setCertificate(cert);

  char key[AWS_CERT_PRIVATE.length() + 1];
  AWS_CERT_PRIVATE.toCharArray(key, AWS_CERT_PRIVATE.length() + 1);
  net.setPrivateKey(key);

  char endpt[AWS_IOT_ENDPOINT.length() + 1];
  AWS_IOT_ENDPOINT.toCharArray(endpt, AWS_IOT_ENDPOINT.length() + 1);
  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.begin(endpt, 8883, net);

  // Try to connect to AWS and count how many times we retried.
  int retries = 0;
  Serial.print("Connecting to AWS IOT");

  char thingname[AWS_IOT_THING_NAME.length() + 1];
  AWS_IOT_THING_NAME.toCharArray(thingname, AWS_IOT_THING_NAME.length() + 1);
  while (!client.connect(thingname) && retries < AWS_MAX_RECONNECT_TRIES) {
    Serial.print(".");
    delay(100);
    retries++;
  }

  // Make sure that we did indeed successfully connect to the MQTT broker
  // If not we just end the function and wait for the next loop.
  if (!client.connected()) {
    Serial.println(" Timeout!");
    return;
  }

  // If we land here, we have successfully connected to AWS!
  // And we can subscribe to topics and send messages.
  Serial.println("Connected!");
  char subscrib[TOPIC_SUB.length() + 1];
  TOPIC_SUB.toCharArray(subscrib, TOPIC_SUB.length() + 1);
  client.subscribe(subscrib);
  char publis[TOPIC_PUB.length() + 1];
  TOPIC_PUB.toCharArray(publis, TOPIC_PUB.length() + 1);
  client.publish(publis, "hello from esp");
}

void ConnecttoNonAws () {
  char BrAdd[BrokerAdd.length() + 1];
  BrokerAdd.toCharArray(BrAdd, BrokerAdd.length() + 1);
  client_esp.setServer(BrAdd, PortNo.toInt());
  client_esp.setCallback(callback_NotAws);
  Serial.print("Connecting to Non AWS Platform");
  int retries = 0;
  delay(1000);
  char CliID[ClientID.length() + 1];
  ClientID.toCharArray(CliID, ClientID.length() + 1);

  char User[Username.length() + 1];
  Username.toCharArray(User, Username.length() + 1);

  char Upass[UserPass.length() + 1];
  UserPass.toCharArray(Upass, UserPass.length() + 1);
  while (!client_esp.connect(CliID, User, Upass) && retries < AWS_MAX_RECONNECT_TRIES) {
    Serial.print(".");
    delay(100);
    retries++;
  }
  // Make sure that we did indeed successfully connect to the MQTT broker
  // If not we just end the function and wait for the next loop.
  if (!client_esp.connected()) {
    Serial.println(" Timeout!");
    return;
  }
  char subscrib[TOPIC_SUB.length() + 1];
  TOPIC_SUB.toCharArray(subscrib, TOPIC_SUB.length() + 1);
  Serial.println("Connected!");
  client_esp.subscribe(subscrib);
}

void callback(String &topic, String &payload) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < payload.length(); i++) {
    Serial.print((char)payload[i]);
    //Serial2.print((char)payload[i]);
    sendata += (char) payload[i];
  }
  Serial.println();
  sen = true;
}

void callback_NotAws(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    //Serial2.print((char)payload[i]);
    sendata += (char) payload[i];
  }
  Serial.println();
  sen = true;
}


bool SendData(String in) {
  int vals = 0;
  for (int i = 0; i < in.length(); i++)
    if (in.charAt(i) == ';')
      vals++;
  if (vals > 0) {
    bid++;
    mid++;
    char msg[in.length() + 1];
    in.toCharArray(msg, in.length() + 1);
    Serial.print("Publish message: ");

    WiFi.macAddress(mac);
    char hexCar[2];
    String mac_hex = "";
    for (int i = 0; i < 6; i++) {
      sprintf(hexCar, "%02X", mac[i]);
      mac_hex += hexCar;
    }
    mac_hex.toUpperCase();
    // Serial.println(mac);
    //Convert to JSON
    StaticJsonDocument<1024> doc;
    doc["id"] = mac_hex;
    doc["v"] = ver;
    doc["rssi"] = WiFi.RSSI();
    String LocalIP = String() + WiFi.localIP()[0] + "." + WiFi.localIP()[1] + "." + WiFi.localIP()[2] + "." + WiFi.localIP()[3];
    doc["ip"] = LocalIP;
    doc["bid"] = bid;
    doc["mid"] = mid;
    doc["heartbeat"] = false;
    doc["raw_beacon_data"] = msg;
    String pub;
    serializeJsonPretty(doc, pub);
    //Convert to char array to publish
    char pubdata[pub.length() + 1];
    pub.toCharArray(pubdata, pub.length() + 1);
    Serial.println(pubdata);

    char publis[TOPIC_PUB.length() + 1];
    TOPIC_PUB.toCharArray(publis, TOPIC_PUB.length() + 1);
    if (Con_type)
      return (client.publish(publis, pubdata));
    else
      return (client_esp.publish(publis, pubdata));
  }
  return false;
}

void ReadFiles() {

  if (SPIFFS.exists("/ssid.txt") && SPIFFS.exists("/pass.txt")) {
    File _ssid = SPIFFS.open("/ssid.txt");
    ssid = "";
    password = "";
    while (_ssid.available()) {
      ssid += (char)_ssid.read();
    }
    _ssid.close();
    File _pass = SPIFFS.open("/pass.txt");
    while (_pass.available()) {
      password += (char)_pass.read();
    }
    _pass.close();
    ssid.trim();
    password.trim();

    setup_wifi();

    if (SPIFFS.exists("/Conn_type.txt") && WiFi.status() == WL_CONNECTED) {
      String Data_file = "";
      File _datafile = SPIFFS.open("/Conn_type.txt");
      while (_datafile.available()) {
        Data_file += (char)_datafile.read();
      }
      _datafile.close();
      Data_file.trim();
      Data_file.toLowerCase();
      if (SPIFFS.exists("/sub.txt") && SPIFFS.exists("/pub.txt")) {
        TOPIC_SUB = "";
        TOPIC_PUB = "";

        File _sub = SPIFFS.open("/sub.txt");
        while (_sub.available()) {
          TOPIC_SUB += (char)_sub.read();
        }
        _sub.close();

        File _pub = SPIFFS.open("/pub.txt");
        while (_pub.available()) {
          TOPIC_PUB += (char)_pub.read();
        }
        _pub.close();

        TOPIC_SUB.trim();
        TOPIC_PUB.trim();

        Serial.print("Topic to be subscribed: ");
        Serial.print(TOPIC_SUB);
        Serial.print(" and Topic where data is published: ");
        Serial.println(TOPIC_PUB);

      }
      else {
        Serial.println("Please Provide Sub/Pub Topic");
      }
      if (Data_file == "aws") {
        Con_type = true;
        if (SPIFFS.exists("/thing.txt") && SPIFFS.exists("/endpoint.txt") && SPIFFS.exists("/Cert.txt") && SPIFFS.exists("/PvtKey.txt")) {
          AWS_IOT_THING_NAME = "";
          AWS_IOT_ENDPOINT = "";
          AWS_CERT_CRT = "";
          AWS_CERT_PRIVATE = "";

          File _thing = SPIFFS.open("/thing.txt");
          while (_thing.available()) {
            AWS_IOT_THING_NAME += (char)_thing.read();
          }
          _thing.close();

          File _end = SPIFFS.open("/endpoint.txt");
          while (_end.available()) {
            AWS_IOT_ENDPOINT += (char)_end.read();
          }
          _end.close();

          File _cert = SPIFFS.open("/Cert.txt");
          while (_cert.available()) {
            AWS_CERT_CRT += (char)_cert.read();
          }
          _cert.close();

          File _key = SPIFFS.open("/PvtKey.txt");
          while (_key.available()) {
            AWS_CERT_PRIVATE += (char)_key.read();
          }
          _key.close();

          AWS_IOT_THING_NAME.trim();
          AWS_IOT_ENDPOINT.trim();
          AWS_CERT_CRT.trim();
          AWS_CERT_CRT += '\n';
          AWS_CERT_PRIVATE.trim();
          AWS_CERT_PRIVATE += '\n';
          Serial.print("Thing name is: ");
          Serial.println(AWS_IOT_THING_NAME);
          Serial.print("End Point is: ");
          Serial.println(AWS_IOT_ENDPOINT);
          Serial.print("Check Result for Cert : ");
          Serial.println((AWS_CERT_CRT.equals(String(AWS_CERT_CRT_ck))) ? "true" : "false");
          Serial.print("Check Result for Private Key : ");
          Serial.println((AWS_CERT_PRIVATE.equals(String(AWS_CERT_PRIVATE_ck))) ? "true" : "false");
          connectToAWS();
          client.onMessage(callback);
          conf = false;
        }
        else {
          Serial.println("Please Provide aws credentials..");
        }
      }
      else if (Data_file == "other") {
        Con_type = false;
        if (SPIFFS.exists("/Client_ID.txt") && SPIFFS.exists("/Username.txt") && SPIFFS.exists("/User_pass.txt") && SPIFFS.exists("/Broker_Address.txt") && SPIFFS.exists("/Port.txt")) {
          ClientID = "";
          Username = "";
          UserPass = "";
          BrokerAdd = "";
          PortNo = "";
          File file_CID = SPIFFS.open("/Client_ID.txt");
          while (file_CID.available()) {
            ClientID += (char)file_CID.read();
          }
          file_CID.close();

          File file_uname = SPIFFS.open("/Username.txt");
          while (file_uname.available()) {
            Username += (char)file_uname.read();
          }
          file_uname.close();

          File file_upass = SPIFFS.open("/User_pass.txt");
          while (file_upass.available()) {
            UserPass += (char)file_upass.read();
          }
          file_upass.close();

          File file_Badd = SPIFFS.open("/Broker_Address.txt");
          while (file_Badd.available()) {
            BrokerAdd += (char)file_Badd.read();
          }
          file_Badd.close();

          File file_port = SPIFFS.open("/Port.txt");
          while (file_port.available()) {
            PortNo += (char)file_port.read();
          }
          file_port.close();

          ClientID.trim();
          Username.trim();
          UserPass.trim();
          BrokerAdd.trim();
          PortNo.trim();

          Serial.println(ClientID);
          Serial.println(Username);
          Serial.println(UserPass);
          Serial.println(BrokerAdd);
          Serial.println(PortNo);

          ConnecttoNonAws();
          conf = false;

        }
        else {
          Serial.println("Please Provide mqqt credentials");
        }
      }
      else {
        Serial.println("Please Provide Correct Connection Type..");
      }
    }
    else {
      Serial.println("Please Provide Connection Type(or provide correct wifi configurations)..");
    }
  }
  else {
    Serial.println("Please provide wifi configurations..");
  }

}

int checkCommand(String command) {
  for (int i = 0; i < 4; i++) {
    if (command.equalsIgnoreCase(commands[i]))
      return i + 1;
  }
  return 0;
}

void setup() {
  Serial.begin(115200);

  //For Recieving data
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  conf = true;
  ReadFiles();
  Serial.println("reboot");
  listDir(SPIFFS, "/", 0);

}
int hexCharToInt(char c) {
  int val = (int)c;
  if ((val >= 'A' && val <= 'F'))
    val = 9 + (val % 16);
  return val % 16;
}

void loop() {
  while (conf) {
    if (Serial2.available()) {
      //char hexCar[2];
      char c = Serial2.read();
      //sprintf(hexCar, "%02X", c);
      //Serial.println(hexCar);
      if (c == 'X') {
        Serial.println("Trying enter normal mode");
        String val = "";
        while (Serial2.available())
        {
          c = Serial2.read();
          if (c != 0x06 && c >= 'a' && c <= 'z')
            val += c;
        }
        if (val.equals("norm")) {
          Serial.println("Entring normal mode");
          conf = false;
          ReadFiles();
        }
      }
      if (c == 'B') {
        String val = "";
        while (c != 0x06) {
          if (Serial2.available())
          {
            c = Serial2.read();
            if (c != 0x06)
              val += c;
          }
        }
        Serial.println(val);
        char val3[val.length() + 1];
        val.toCharArray(val3, val.length() + 1);

        writeFile(SPIFFS, "/ssid.txt", val3);
      }
      if (c == 'C') {
        String val = "";
        while (c != 0x06) {
          if (Serial2.available())
          {
            c = Serial2.read();
            if (c != 0x06)
              val += c;
          }
        }
        Serial.println(val);
        char val3[val.length() + 1];
        val.toCharArray(val3, val.length() + 1);
        writeFile(SPIFFS, "/pass.txt", val3);
      }
      if (c == 'D') {
        String val = "";
        while (c != 0x06) {
          if (Serial2.available())
          {
            c = Serial2.read();
            if (c != 0x06)
              val += c;
          }
        }
        Serial.println(val);
        char val3[val.length() + 1];
        val.toCharArray(val3, val.length() + 1);
        writeFile(SPIFFS, "/thing.txt", val3);
      }
      if (c == 'E') {
        String val = "";
        while (c != 0x06) {
          if (Serial2.available())
          {
            c = Serial2.read();
            if (c != 0x06)
              val += c;
          }
        }
        Serial.println(val);
        char val3[val.length() + 1];
        val.toCharArray(val3, val.length() + 1);
        writeFile(SPIFFS, "/endpoint.txt", val3);
      }
      if (c == 'F') {
        String val = "";
        while (c != 0x06) {
          if (Serial2.available())
          {
            c = Serial2.read();
            if (c != 0x06)
              val += c;
          }
        }
        Serial.println(val);
        char val3[val.length() + 1];
        val.toCharArray(val3, val.length() + 1);
        writeFile(SPIFFS, "/Cert.txt", val3);
      }
      if (c == 'G') {
        String val = "";
        while (c != 0x06) {
          if (Serial2.available())
          {
            c = Serial2.read();
            if (c != 0x06)
              val += c;
          }
        }
        Serial.println(val);
        char val3[val.length() + 1];
        val.toCharArray(val3, val.length() + 1);
        writeFile(SPIFFS, "/PvtKey.txt", val3);
      }
      if (c == 'H') {
        String val = "";
        while (c != 0x06) {
          if (Serial2.available())
          {
            c = Serial2.read();
            if (c != 0x06)
              val += c;
          }
        }
        Serial.println(val);
        char val3[val.length() + 1];
        val.toCharArray(val3, val.length() + 1);
        writeFile(SPIFFS, "/sub.txt", val3);
      }
      if (c == 'I') {
        String val = "";
        while (c != 0x06) {
          if (Serial2.available())
          {
            c = Serial2.read();
            if (c != 0x06)
              val += c;
          }
        }
        Serial.println(val);
        char val3[val.length() + 1];
        val.toCharArray(val3, val.length() + 1);
        writeFile(SPIFFS, "/pub.txt", val3);
      }
      if (c == 'J') {
        String val = "";
        while (c != 0x06) {
          if (Serial2.available())
          {
            c = Serial2.read();
            if (c != 0x06)
              val += c;
          }
        }
        Serial.println(val);
        char val3[val.length() + 1];
        val.toCharArray(val3, val.length() + 1);

        writeFile(SPIFFS, "/Conn_type.txt", val3);
      }
      if (c == 'K') {
        String val = "";
        while (c != 0x06) {
          if (Serial2.available())
          {
            c = Serial2.read();
            if (c != 0x06)
              val += c;
          }
        }
        Serial.println(val);
        char val3[val.length() + 1];
        val.toCharArray(val3, val.length() + 1);

        writeFile(SPIFFS, "/Client_ID.txt", val3);
      }
      if (c == 'L') {
        String val = "";
        while (c != 0x06) {
          if (Serial2.available())
          {
            c = Serial2.read();
            if (c != 0x06)
              val += c;
          }
        }
        Serial.println(val);
        char val3[val.length() + 1];
        val.toCharArray(val3, val.length() + 1);

        writeFile(SPIFFS, "/Username.txt", val3);
      }
      if (c == 'M') {
        String val = "";
        while (c != 0x06) {
          if (Serial2.available())
          {
            c = Serial2.read();
            if (c != 0x06)
              val += c;
          }
        }
        Serial.println(val);
        char val3[val.length() + 1];
        val.toCharArray(val3, val.length() + 1);

        writeFile(SPIFFS, "/User_pass.txt", val3);
      }
      if (c == 'N') {
        String val = "";
        while (c != 0x06) {
          if (Serial2.available())
          {
            c = Serial2.read();
            if (c != 0x06)
              val += c;
          }
        }
        Serial.println(val);
        char val3[val.length() + 1];
        val.toCharArray(val3, val.length() + 1);

        writeFile(SPIFFS, "/Broker_Address.txt", val3);
      }
      if (c == 'O') {
        String val = "";
        while (c != 0x06) {
          if (Serial2.available())
          {
            c = Serial2.read();
            if (c != 0x06)
              val += c;
          }
        }
        Serial.println(val);
        char val3[val.length() + 1];
        val.toCharArray(val3, val.length() + 1);

        writeFile(SPIFFS, "/Port.txt", val3);
      }
    }
  }
  if (Con_type)
    client.loop();
  else
    client_esp.loop();

  if (sen) {
    StaticJsonDocument<200> doc;

    char json[sendata.length() + 1];
    sendata.toCharArray(json, sendata.length() + 1);
    DeserializationError error = deserializeJson(doc, json);

    // Test if parsing succeeds.
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
    }
    else {
      //deserializeJson(doc, json);
      String command = doc["Command"];
      String _mac = doc["Mac"];
      String ser = doc["Service_UUID"];
      String chr = doc["Characteristic_UUID"];
      String dat = doc["Data"];
      String Data = _mac + ser + chr + dat;

      int _command = checkCommand(command);
      bool _valid = false;
      if (_command == 1 && Data.length() >= 78) {
        _valid = true;
      }
      else if (_command == 2 && Data.length() == 78) {
        _valid = true;
      }
      else if (_command == 3 && Data.length() == 80) {
        _valid = true;
      }
      if (_valid) {
        Serial.print("Sending: ");
        Serial.print("(Command) ");
        Serial.print(_command);
        Serial.print(" (Data) ");
        Serial.println(Data);
        Serial2.write('s');
        Serial2.write(_command);
        Serial2.print(Data);
        //        Serial2.print(mac);
        //        Serial2.print(ser);
        //        Serial2.print(chr);
        //        Serial2.print(dat);

        //        for (int i = 0; i < Data.length(); i++) {
        //          Serial2.write((char)Data[i]);
        //        }
        Serial2.write('\n');
      }
      else {
        Serial.println("Wrong Data ");
        StaticJsonDocument<300> doc;
        WiFi.macAddress(mac);
        char hexCar[2];
        String mac_hex = "";
        for (int i = 0; i < 6; i++) {
          sprintf(hexCar, "%02X", mac[i]);
          mac_hex += hexCar;
        }
        rd.toUpperCase();
        doc["MAC"] = mac_hex;
        doc["RSSI"] = WiFi.RSSI();
        String LocalIP = String() + WiFi.localIP()[0] + "." + WiFi.localIP()[1] + "." + WiFi.localIP()[2] + "." + WiFi.localIP()[3];
        doc["IP"] = LocalIP;
        doc["Ver"] = ver;
        doc["MSG_Type"] = "Error Response";
        doc["Error_Type"] = "Invalid Data Entered";
        String pub;
        serializeJsonPretty(doc, pub);
        //Convert to char array to publish
        char pubdata[pub.length() + 1];
        pub.toCharArray(pubdata, pub.length() + 1);
        Serial.println(pubdata);

        char publis[TOPIC_PUB.length() + 1];
        TOPIC_PUB.toCharArray(publis, TOPIC_PUB.length() + 1);
        bool _sendData;
        if (Con_type)
          _sendData = client.publish(publis, pubdata);
        else
          _sendData = (client_esp.publish(publis, pubdata));
        if (_sendData) {
          Serial.println("Data Published");
          noErr = 0;
        }
        else {
          Serial.println("Error in publishing data");
          noErr ++;
        }
      }
    }
    sen = false;
    sendata = "";
  }
  if (noErr > MAX_ERROR_THRESHOLD) {
    Serial.println("Limit Reached");
    if (WiFi.status() != WL_CONNECTED) {
      if (Con_type)
        client.disconnect();
      else
        client_esp.disconnect();
      WiFi.disconnect();
      setup_wifi();
      if (Con_type)
        connectToAWS();
      else
        ConnecttoNonAws();
      noErr = 0;
    }
    else if (Con_type && !client.connected()) {
      client.disconnect();
      connectToAWS();
    }
    else if (!Con_type && !client_esp.connected()) {
      client_esp.disconnect();
      ConnecttoNonAws();
    }
  }
  if (Serial2.available() > 0) {
    char c;
    c = Serial2.read();
    Serial.print(c);
    if (c == 'Y') {
      rd = "";
      int i = 0;
      uint16_t len = 0;
      while (i < 2) {
        if (Serial2.available()) {
          c = Serial2.read();
          uint8_t len1 = (uint8_t)c;
          len = (uint16_t)(len + len1 * (pow(256, i)));
          i++;
        }
      }
      i = 0;
      Serial.println(len);
      while (i < len) {
        if (Serial2.available()) {
          c = Serial2.read();
          rd += (char)c;
          i++;
        }
      }
      Serial.print("Data Recieved : ");
      Serial.println(rd);
      if (SendData(rd)) {
        Serial.println("Data Published");
        noErr = 0;
      }
      else {
        Serial.println("Error in publishing data");
        noErr ++;
      }
    }
    else if (c == 'E') {
      rd = "";
      while (c != '\n') {
        if (Serial2.available())
        {
          c = Serial2.read();
          Serial.print(c);
          rd += (char) c;
        }
      }
      Serial.println("Error Message Recieved");
      Serial.println(rd);
      StaticJsonDocument<300> doc;
      WiFi.macAddress(mac);
      char hexCar[2];
      String mac_hex = "";
      for (int i = 0; i < 6; i++) {
        sprintf(hexCar, "%02X", mac[i]);
        mac_hex += hexCar;
      }
      rd.toUpperCase();
      doc["MAC"] = mac_hex;
      doc["RSSI"] = WiFi.RSSI();
      String LocalIP = String() + WiFi.localIP()[0] + "." + WiFi.localIP()[1] + "." + WiFi.localIP()[2] + "." + WiFi.localIP()[3];
      doc["IP"] = LocalIP;
      doc["Ver"] = ver;
      doc["MSG_Type"] = "Error Response";
      switch (rd.charAt(0)) {
        case '1':
          doc["Error_Type"] = "Service Not Found";
          break;
        case '2':
          doc["Error_Type"] = "Characteristic  Not Found";
          break;
        case '3':
          doc["Error_Type"] = "Write Failed: Permissions Not Found";
          break;
        case '4':
          doc["Error_Type"] = "Read Failed: Permission Not Found";
          break;
        case '5':
          doc["Error_Type"] = "Read Failed: Could Not Read";
          break;
        case '6':
          doc["Error_Type"] = "Subscription Failed: Permission Not Found";
          break;
        case '7':
          doc["Error_Type"] = "Mac Address not Found: Timeout";
          break;
        default:
          doc["Error_Type"] = "Unknown Error";
          break;
      }
      String pub;
      serializeJsonPretty(doc, pub);
      //Convert to char array to publish
      char pubdata[pub.length() + 1];
      pub.toCharArray(pubdata, pub.length() + 1);
      Serial.println(pubdata);

      char publis[TOPIC_PUB.length() + 1];
      TOPIC_PUB.toCharArray(publis, TOPIC_PUB.length() + 1);
      bool _sendData;
      if (Con_type)
        _sendData = client.publish(publis, pubdata);
      else
        _sendData = (client_esp.publish(publis, pubdata));
      if (_sendData) {
        Serial.println("Data Published");
        noErr = 0;
      }
      else {
        Serial.println("Error in publishing data");
        noErr ++;
      }
    }
    else if (c == 'U') {
      rd = "";
      while (c != '\n') {
        if (Serial2.available())
        {
          c = Serial2.read();
          Serial.print(c);
          rd += (char) c;
        }
      }
      Serial.println("Notify Data Recieved");
      Serial.println(rd);
      StaticJsonDocument<700> doc;
      WiFi.macAddress(mac);
      char hexCar[2];
      String mac_hex = "";
      for (int i = 0; i < 6; i++) {
        sprintf(hexCar, "%02X", mac[i]);
        mac_hex += hexCar;
      }
      rd.toUpperCase();
      doc["MAC"] = mac_hex;
      doc["RSSI"] = WiFi.RSSI();
      String LocalIP = String() + WiFi.localIP()[0] + "." + WiFi.localIP()[1] + "." + WiFi.localIP()[2] + "." + WiFi.localIP()[3];
      doc["IP"] = LocalIP;
      doc["Ver"] = ver;
      String strBidHex = rd.substring(1, 3);
      int val1 = hexCharToInt(strBidHex.charAt(0));
      int val2 = hexCharToInt(strBidHex.charAt(1));
      uint8_t intBid = (((val1) << 4) & (0xF0)) + ((val2) & 0x0F);
      doc["BID"] = intBid;
      String strMidHex = rd.substring(3, 5);
      val1 = hexCharToInt(strMidHex.charAt(0));
      val2 = hexCharToInt(strMidHex.charAt(1));
      uint8_t intMid = (((val1) << 4) & (0xF0)) + ((val2) & 0x0F);
      doc["MID"] = intMid;
      if (rd.charAt(0) == 'R') {
        doc["Device_MAC"] = rd.substring(7, 19);
        doc["MSG_Type"] = "Records";
        String strCntHex = rd.substring(5, 7);
        val1 = hexCharToInt(strCntHex.charAt(0));
        val2 = hexCharToInt(strCntHex.charAt(1));
        uint8_t intCnt = (((val1) << 4) & (0xF0)) + ((val2) & 0x0F);
        doc["R_Record_Count"] = intCnt;
        doc["Records"] = rd.substring(19, rd.length() - 1);
      }
      if (rd.charAt(0) == 'N') {
        doc["Device_MAC"] = rd.substring(5, 17);
        doc["MSG_Type"] = "NOTIFY DATA";
        doc["Packet"] = rd.substring(17, rd.length() - 1);
      }
      String pub;
      serializeJsonPretty(doc, pub);
      //Convert to char array to publish
      char pubdata[pub.length() + 1];
      pub.toCharArray(pubdata, pub.length() + 1);
      Serial.println(pubdata);

      char publis[TOPIC_PUB.length() + 1];
      TOPIC_PUB.toCharArray(publis, TOPIC_PUB.length() + 1);
      bool _sendData;
      if (Con_type)
        _sendData = client.publish(publis, pubdata);
      else
        _sendData = (client_esp.publish(publis, pubdata));
      if (_sendData) {
        Serial.println("Data Published");
        noErr = 0;
      }
      else {
        Serial.println("Error in publishing data");
        noErr ++;
      }
    }
    else if (c == 'W') {
      rd = "";
      while (c != '\n') {
        if (Serial2.available())
        {
          c = Serial2.read();
          Serial.print(c);
          rd += (char) c;
        }
      }
      Serial.println("Read Data Recieved");
      Serial.println(rd);
      StaticJsonDocument<300> doc;
      WiFi.macAddress(mac);
      char hexCar[2];
      String mac_hex = "";
      for (int i = 0; i < 6; i++) {
        sprintf(hexCar, "%02X", mac[i]);
        mac_hex += hexCar;
      }
      rd.toUpperCase();
      doc["MAC"] = mac_hex;
      doc["RSSI"] = WiFi.RSSI();
      String LocalIP = String() + WiFi.localIP()[0] + "." + WiFi.localIP()[1] + "." + WiFi.localIP()[2] + "." + WiFi.localIP()[3];
      doc["IP"] = LocalIP;
      doc["Ver"] = ver;
      doc["Device_MAC"] = rd.substring(0, 12);
      doc["MSG_Type"] = "READ FROM DEVICE";
      doc["Data"] = rd.substring(12, rd.length() - 1);
      String pub;
      serializeJsonPretty(doc, pub);
      //Convert to char array to publish
      char pubdata[pub.length() + 1];
      pub.toCharArray(pubdata, pub.length() + 1);
      Serial.println(pubdata);

      char publis[TOPIC_PUB.length() + 1];
      TOPIC_PUB.toCharArray(publis, TOPIC_PUB.length() + 1);
      bool _sendData;
      if (Con_type)
        _sendData = client.publish(publis, pubdata);
      else
        _sendData = (client_esp.publish(publis, pubdata));
      if (_sendData) {
        Serial.println("Data Published");
      }
      else {
        Serial.println("Error in publishing data");
      }
    }
    else if (c == 'Z') {
      Serial.println("Trying enter config mode");

      String val = "";
      while (Serial2.available()) {
        c = Serial2.read();
        if (c != 0x06 && (c >= 'a' && c <= 'z'))
          val += c;
      }
      if (val.equals("conf")) {
        Serial.println("Entring Configuration mode");
        conf = true;
        WiFi.disconnect();
        if (Con_type)
          client.disconnect();
        else
          client_esp.disconnect();
      }
      //conf = true;
    }

  }
}
