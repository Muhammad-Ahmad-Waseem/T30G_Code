#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

using namespace Adafruit_LittleFS_Namespace;
#define devs 100                      //Max devices that can be scanned in a duration
#define MAX_PRPH_CONNECTION   2       //Maximum allowed connection request
#define DEVICE_SEARCH_LIMIT   10000   //Time limit to search device in connection requye
#define BUFFSIZE 10                   //Limit of notification messages

//UUID's for POM100
uint8_t FILTER_SERVICE_UUID[] =               {0x31, 0x2d, 0x44, 0x4b, 0x50, 0x2d, 0x50, 0x45, 0x2d, 0x30, 0x30, 0x31, 0x2d, 0x4d, 0x4f, 0x50};
uint8_t NOTIFY_SERVICE_UUID[] =               {0x33, 0x4d, 0x44, 0x4b, 0x50, 0x2d, 0x50, 0x45, 0x2d, 0x30, 0x30, 0x31, 0x2d, 0x4d, 0x4f, 0x50};
uint8_t NOTIFY_CHARACTERISTIC_UUID[] =        {0x33, 0x4d, 0x44, 0x4b, 0x50, 0x2d, 0x50, 0x45, 0x2d, 0x30, 0x30, 0x31, 0x4d, 0x5d, 0x4f, 0x50};
uint8_t NOTIFY_CHARACTERISTIC_UUID_ENABLER[] = {0x33, 0x4d, 0x44, 0x4b, 0x50, 0x2d, 0x50, 0x45, 0x2d, 0x30, 0x30, 0x31, 0x3d, 0x5d, 0x4f, 0x50};

//Initialization of POM100 clients
BLEClientService        notifyService(NOTIFY_SERVICE_UUID);
BLEClientCharacteristic notifyCharacteristic(NOTIFY_CHARACTERISTIC_UUID);
BLEClientCharacteristic notifyCharacteristicEnabler(NOTIFY_CHARACTERISTIC_UUID_ENABLER);

//For initialization of client service and characteristic we use these uuids (these will be of no use as we will rewrite them acc to command)
uint8_t BEACON_SERVICE_UUID_D[] =               {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t BEACON_CHARACTERISTIC_UUID_D[] =        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

//This will strore actuall UUID acc to command
uint8_t Service_UUID[16];
uint8_t Charteristic_UUID[16];

//Initializing clients for commands, read/write/subscribe
BLEClientService        service_beacon(BEACON_SERVICE_UUID_D);
BLEClientCharacteristic characteristic_beacon(BEACON_CHARACTERISTIC_UUID_D);

//FS for storing necessary data
File file(InternalFS);

//UUIDs used in configuration mode (advertisement)
const uint8_t AWS_UUID_SERVICE[] =
{
  0x23, 0xD1, 0xBE, 0xEA, 0x5F, 0x78, 0x23, 0x15,
  0xDE, 0xEF, 0x12, 0x12, 0x23, 0x15, 0x00, 0x00
};

const uint8_t AWS_UUID_CHR_DATA[] =
{
  0x23, 0xD1, 0xBE, 0xEA, 0x5F, 0x78, 0x23, 0x15,
  0xDE, 0xEF, 0x12, 0x12, 0x26, 0x15, 0x00, 0x00
};
//Initializing central GATT attributes
BLEService        aws(AWS_UUID_SERVICE);
BLECharacteristic aws_data(AWS_UUID_CHR_DATA);

//All flags used in code
bool present, con = false, pressed, scanned = false, conDataRec = false, scan_resp, _cert = false, _key = false, _certRec = false, _keyRec = false, sen = false, _dataComplete = false, _buffComplete = false, found = false, _readData = false;

//Antennna Paremeters(default values)
int scanInt = 100, duration = 1000; //(in ms) (scan int (multiple of 5), duration int (1000,10000))

//Tx power(default value)
int TxPwr = 8; //(-40, -20, -16, -12, -8, -4, 0, 2, 3, 4, 5, 6, 7, 8)

//Rssi filter(default value)
int filtRss = -100;//(integer between -50 to -100)

//Used in code
uint16_t lengthCert, lengthKey, conHandle;
uint8_t notifyKey[2];

//Strings and array of strings used in complete code
String adv_devices[devs], scan_resp_devices[devs], str = "", mac = "", rss, inp, hex_data, len, config_data, write_mac, cert = "", key = "", strRecords, buffRecs = "",buffRecs2="", outVal = "", val = "",  deciveMac; // = "d094ef9c4619";

// constants won't change. They're used here to set pin numbers:
const int buttonPin = 5;     // the number of the pushbutton pin(P0.05)

// variables will change:
int buttonState = 0;         // variable for reading the pushbutton status

//integers used in code
int cur , i, tries = 0, dataItems, CurrentItems, bid = 1, mid = 1, bidN, midN , _command = 0;

//Call back to recieve scaned devices
void scan_callback(ble_gap_evt_adv_report_t* report)
{
  uint8_t buffer[32];
  memset(buffer, 0, sizeof(buffer));
  // MAC is in little endian --> print reverse
  mac = "";
  uint8_t* addr = report->peer_addr.addr;
  char hexCar[2];
  for (int i = 5; i > -1; i--) {
    sprintf(hexCar, "%02X", addr[i]);
    mac += hexCar;
  }
  //if we have a connection request (con is true), we will try to connect
  if (con && write_mac != NULL) {
    Serial.print("Found mac : ");
    Serial.println(mac);
    if (matchstr(mac, write_mac)) {
      deciveMac = mac;
      Serial.println("Match Found!");
      Serial.println("Connecting to Peripheral ... ");
      found = true;
      Bluefruit.Central.connect(report);
      //con = false;
      Serial.println("Returned from write value!!");
    }
    else {
      Bluefruit.Scanner.resume(); // continue scanning
    }
  }
  else
  {
    //If not we have to get all required data needed for a scan
    if ((report->rssi > filtRss) && !scanned) {
      if (report->type.scan_response)
      {
        scan_resp = true;
      }
      else
      {
        scan_resp = false;
      }

      int8_t _addr = report->rssi;
      rss = String((char)_addr, HEX);
      rss.toUpperCase();

      sprintf(hexCar, "%02X", report->data.len);
      len = hexCar;


      uint8_t* data = report->data.p_data;
      hex_data = "";
      if (report->data.len)
      {
        for (int i = 0; i < report->data.len; i++) {
          sprintf(hexCar, "%02X", data[i]);
          hex_data += hexCar;
        }
        //Check the flag
        Serial.println(data[29], HEX);
        Serial.println(((data[29] & 0x80) == 0x80) ? "High" : "LOW");
        //if flag is high we need to make connection otherwise continue with scan
        if (((data[29] & 0x80) == 0x80) && !scan_resp) {
          sen = true;
          deciveMac = mac;
          Bluefruit.Central.connect(report);
          Serial.println("Connnect Request Sent");
        }
        else {
          Bluefruit.Scanner.resume();
          scanned = true;
        }
      }
    }
    else {
      Bluefruit.Scanner.resume();
    }
  }
}

// Random function for matching two strings
bool matchstr (String str1, String str2) {
  str1.toUpperCase();
  str2.toUpperCase();
  if (str1.length() != str2.length())
  { Serial.println("String size not equal");
    return false;
  }
  else {
    for (int i = 0; i < str1.length(); i++) {
      if (str1.charAt(i) != str2.charAt(i))
        return false;
    }
  }
  return true;
}

//Setup
void setup() {
  pressed = false;
  // pressed is false untill overwritten
  pinMode(buttonPin, INPUT);
  delay(5000);
  // Initialize both serial coms
  Serial.begin(115200);

  // Initialize Internal File System
  InternalFS.begin();
  // read the state of the pushbutton value:
  buttonState = digitalRead(buttonPin);
  if (buttonState == HIGH) {
    // Overwrite pressed flag
    Serial.println("Configuration Mode");
    pressed = true;
  }
  Serial1.begin(115200);  //Rx=P1.01, Tx = P1.02
  delay(500);
  Serial1.print('\n');
  //High flag, means we need to advertise (configuration)
  if (pressed) {
    Serial1.write('Z');
    Serial1.print("conf");
    Serial1.write(0x06);
    Bluefruit.begin(MAX_PRPH_CONNECTION, 0);
    Bluefruit.Periph.setConnectCallback(connect_callback);
    Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
    Bluefruit.setName("T30G");
    aws.begin();
    aws_data.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE | CHR_PROPS_NOTIFY);
    aws_data.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    aws_data.begin();
    aws_data.setWriteCallback(value_write_callback);
    // Set up and start advertising
    startAdv();
    Serial.println("Bluetooth device active, waiting for connections...");
  }
  if (!pressed) {
    Serial1.write('X');
    Serial1.print("norm");
    Serial1.write(0x06);
    Serial.println("BLE Central scan");
    // Scan mode, we need to read files first
    file.open("/TxPower.txt", FILE_O_READ);
    if (file) {
      uint32_t readlen;
      char buffer[64] = { 0 };
      readlen = file.read(buffer, sizeof(buffer));
      buffer[readlen] = 0;
      String val = String(buffer);
      TxPwr = val.toInt();
      Serial.print("Found updated value for Tx: ");
      Serial.println(TxPwr);
      file.close();
    }
    else {
      Serial.print("Using default value for Tx: ");
      Serial.println(TxPwr);
    }
    Bluefruit.begin(0, 1);
    Bluefruit.setTxPower(TxPwr);

    //Setup POM100 services and charcateristics
    notifyService.begin();
    notifyCharacteristic.setNotifyCallback(notify_callback);
    notifyCharacteristic.begin();
    notifyCharacteristicEnabler.begin();
    Bluefruit.setConnLedInterval(250);

    // Callbacks for Central
    Bluefruit.Central.setDisconnectCallback(disconnect_callback2);
    Bluefruit.Central.setConnectCallback(connect_callback2);
    Bluefruit.Scanner.setRxCallback(scan_callback);
    Bluefruit.Scanner.restartOnDisconnect(true);

    //check updated values of scan interval and duration
    file.open("/ScanInterval.txt", FILE_O_READ);
    if (file) {
      uint32_t readlen;
      char buffer[64] = { 0 };
      readlen = file.read(buffer, sizeof(buffer));
      buffer[readlen] = 0;
      String val = String(buffer);
      scanInt = val.toInt();
      Serial.print("Found updated value for Scan Interval: ");
      Serial.println(scanInt);
      file.close();
    }
    else {
      Serial.print("Using default value for Scan Interval: ");
      Serial.println(scanInt);
    }
    int interval = (int)scanInt / 0.625;
    Bluefruit.Scanner.setInterval(interval, interval / 2);     // in units of 0.625 ms
    Bluefruit.Scanner.useActiveScan(true);        // Request active data
    Bluefruit.Scanner.filterUuid(FILTER_SERVICE_UUID);
    Bluefruit.Scanner.start(0);                   // 0 = Don't stop scanning after n seconds

    //Check updated values for RSSi filter and scan duration
    file.open("/RssiFilter.txt", FILE_O_READ);
    if (file) {
      uint32_t readlen;
      char buffer[64] = { 0 };
      readlen = file.read(buffer, sizeof(buffer));
      buffer[readlen] = 0;
      String val = String(buffer);
      filtRss = val.toInt();
      Serial.print("Found updated value for Rss filter: ");
      Serial.println(filtRss);
      file.close();
    }
    else {
      Serial.print("Using default value for Rss filter: ");
      Serial.println(filtRss);
    }
    file.open("/ScanDuration.txt", FILE_O_READ);
    if (file) {
      uint32_t readlen;
      char buffer[64] = { 0 };
      readlen = file.read(buffer, sizeof(buffer));
      buffer[readlen] = 0;
      String val = String(buffer);
      duration = val.toInt();
      Serial.print("Found updated value for Scan Duration: ");
      Serial.println(duration);
      file.close();
    }
    else {
      Serial.print("Using default value for Scan Duration: ");
      Serial.println(duration);
    }
    //capture current time, just before running loop
    cur = millis();
  }
}

//Callback when external device writes something at device's characteristic (Configuration mode)
void value_write_callback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len)
{
  (void) conn_hdl;
  (void) chr;
  (void) len;
  Serial.print("Data Received: ");
  for (int i = 0; i < len; i++) {
    Serial.print((char)data[i]);
    config_data += (char)data[i];
    if ((char)data[i] == '\n')
      conDataRec = true;
  }
  Serial.println();
}

//Called when we need to start advertising
void startAdv(void)
{
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(aws);
  Bluefruit.ScanResponse.addName();

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
}

//loop
void loop() {
  if (pressed && conDataRec) {
    //if we are in configuration and we recieve some data through callback
    if (_cert) {
      //if we already have pending data for certificate
      for (int i = 0; i < config_data.length(); i++) {
        if ((char)config_data.charAt(i) == '\t') {
          //if cert data is completed,check whether length recieved is correct ot not
          if (lengthCert == 0 ) {
            _certRec = true;
            Serial.println("Certificate Recieved.. ");
            Serial.println();
            if (aws_data.notifyEnabled()) {
              aws_data.notify("YES");
              Serial.println("Notified");
            }
          }
          else {
            Serial.println("Wrong Certificate Data");
            if (aws_data.notifyEnabled()) {
              aws_data.notify("NO");
              Serial.println("Notified");
            }
            cert = "";
          }
          _cert = false;
        }
        else {
          //if other than ending byte, add in cert
          cert += (char)config_data.charAt(i);
          lengthCert--;
        }
      }
      Serial.print("Remaining Length: ");
      Serial.println(lengthCert);
    }
    else if (_key) {//Works similar to cert
      for (int i = 0; i < config_data.length(); i++) {
        if ((char)config_data.charAt(i) == '\t') {
          if (lengthKey == 0 ) {
            _keyRec = true;
            Serial.println("Key Received..");
            if (aws_data.notifyEnabled()) {
              aws_data.notify("YES");
              Serial.println("Notified");
            }
          }
          else {
            Serial.println("Wrong Key Data");
            if (aws_data.notifyEnabled()) {
              aws_data.notify("NO");
              Serial.println("Notified");
            }
            key = "";
          }
          _key = false;
        }
        else {
          key += (char)config_data.charAt(i);
          lengthKey--;
        }
      }
      Serial.print("Remaining Length: ");
      Serial.println(lengthKey);
    }//Now these are conditions on recieved data to check what is sent via APP
    else if (config_data.charAt(0) == 'B') {
      if (config_data.charAt(config_data.length() - 1) == 0x0A) {
        Serial.println("SSID");
        Serial1.write('B');
        for (int i = 1; i < config_data.length() - 1; i++) {
          Serial.print((char)config_data.charAt(i));
          Serial1.write((char)config_data.charAt(i));
        }
        Serial.println();
        Serial1.write(0x06);
        if (aws_data.notifyEnabled()) {
          aws_data.notify("YES");
          Serial.println("Notified");
        }
      }
      else {
        if (aws_data.notifyEnabled()) {
          aws_data.notify("NO");
          Serial.println("Notified");
        }
      }
    }
    else if (config_data.charAt(0) == 'C') {
      if (config_data.charAt(config_data.length() - 1) == 0x0A) {
        Serial.println("Password");
        Serial1.write('C');
        for (int i = 1; i < config_data.length() - 1 ; i++) {
          Serial.print((char)config_data.charAt(i));
          Serial1.write((char)config_data.charAt(i));
        }
        Serial.println();
        Serial1.write(0x06);
        if (aws_data.notifyEnabled()) {
          aws_data.notify("YES");
          Serial.println("Notified");
        }
      }
      else {
        if (aws_data.notifyEnabled()) {
          aws_data.notify("NO");
          Serial.println("Notified");
        }
      }
    }
    else if (config_data.charAt(0) == 'D') {
      if (config_data.charAt(config_data.length() - 1) == 0x0A) {
        Serial1.write('J');
        Serial1.write("aws");
        Serial1.write(0x06);
        Serial.println("Thing name");
        Serial1.write('D');
        for (int i = 1; i < config_data.length() - 1 ; i++) {
          Serial.print((char)config_data.charAt(i));
          Serial1.write((char)config_data.charAt(i));
        }
        Serial.println();
        Serial1.write(0x06);
        if (aws_data.notifyEnabled()) {
          aws_data.notify("YES");
          Serial.println("Notified");
        }
      }
      else {
        if (aws_data.notifyEnabled()) {
          aws_data.notify("NO");
          Serial.println("Notified");
        }
      }
    }
    else if (config_data.charAt(0) == 'E') {
      if (config_data.charAt(config_data.length() - 1) == 0x0A) {
        Serial.println("End point");
        Serial1.write('E');
        for (int i = 1; i < config_data.length() - 1 ; i++) {
          Serial.print((char)config_data.charAt(i));
          Serial1.write((char)config_data.charAt(i));
        }
        Serial.println();
        Serial1.write(0x06);
        if (aws_data.notifyEnabled()) {
          aws_data.notify("YES");
          Serial.println("Notified");
        }
      }
      else {
        if (aws_data.notifyEnabled()) {
          aws_data.notify("NO");
          Serial.println("Notified");
        }
      }
    }
    else if (config_data.charAt(0) == 'F') {
      //certificate is not recieved completely in one-time, so once we found it's start byte, we will turn it's flag on
      //and recieve data according to length sent
      if (_certRec) {
        cert = "";
        key = "";
      }
      _cert = true;
      for (int i = 1; i < config_data.length() ; i++) {
        cert += (char)config_data.charAt(i);
        lengthCert--;
      }
      Serial.print("Remaining Length: ");
      Serial.println(lengthCert);
    }
    else if (config_data.charAt(0) == 'P') {
      //length is sent via app everytime before sending cert
      String len = "";
      if (config_data.charAt(config_data.length() - 1) == 0x0A) {
        for (int i = 1; i < config_data.length() - 1 ; i++) {
          len +=  (char)config_data.charAt(i);
        }
        lengthCert = (uint16_t)len.toInt();
        Serial.println("Length of Cert Rec");
        Serial.println(lengthCert);
        if (aws_data.notifyEnabled()) {
          aws_data.notify("YES");
          Serial.println("Notified");
        }
      }
      else {
        if (aws_data.notifyEnabled()) {
          aws_data.notify("NO");
          Serial.println("Notified");
        }
      }
    }
    else if (config_data.charAt(0) == 'Q') {
      //work similar to cert
      String len = "";
      if (config_data.charAt(config_data.length() - 1) == 0x0A) {
        for (int i = 1; i < config_data.length() - 1 ; i++) {
          len +=  (char)config_data.charAt(i);
        }
        Serial.println("Length of Key Sent");
        Serial.println(len);
        Serial.println("Length of Key Rec");
        lengthKey = (uint16_t)len.toInt();
        Serial.println(lengthKey);
        if (aws_data.notifyEnabled()) {
          aws_data.notify("YES");
          Serial.println("Notified");
        }
      }
      else {
        if (aws_data.notifyEnabled()) {
          aws_data.notify("NO");
          Serial.println("Notified");
        }
      }
    }
    else if (config_data.charAt(0) == 'G') {
      //works similar to cert
      _key = true;
      for (int i = 1; i < config_data.length() ; i++) {
        key += (char)config_data.charAt(i);
        lengthKey--;
      }
      Serial.print("Remaining Length: ");
      Serial.println(lengthKey);
    }
    else if (config_data.charAt(0) == 'H') {
      if (config_data.charAt(config_data.length() - 1) == 0x0A) {
        Serial.println("Sub Topic");
        Serial1.write('H');
        for (int i = 1; i < config_data.length() - 1 ; i++) {
          Serial.print((char)config_data.charAt(i));
          Serial1.write((char)config_data.charAt(i));
        }
        Serial.println();
        Serial1.write(0x06);
        if (aws_data.notifyEnabled()) {
          aws_data.notify("YES");
          Serial.println("Notified");
        }
      }
      else {
        if (aws_data.notifyEnabled()) {
          aws_data.notify("NO");
          Serial.println("Notified");
        }
      }
    }
    else if (config_data.charAt(0) == 'I') {
      if (config_data.charAt(config_data.length() - 1) == 0x0A) {
        Serial.println("Pub Topic");
        Serial1.write('I');
        for (int i = 1; i < config_data.length() - 1 ; i++) {
          Serial.print((char)config_data.charAt(i));
          Serial1.write((char)config_data.charAt(i));
        }
        Serial.println();
        Serial1.write(0x06);
        if (aws_data.notifyEnabled()) {
          aws_data.notify("YES");
          Serial.println("Notified");
        }
      }
      else {
        if (aws_data.notifyEnabled()) {
          aws_data.notify("NO");
          Serial.println("Notified");
        }
      }
    }
    else if (config_data.charAt(0) == 'K') {
      if (config_data.charAt(config_data.length() - 1) == 0x0A) {
        Serial1.write('J');
        Serial1.write("other");
        Serial1.write(0x06);
        Serial.println("Client ID");
        Serial1.write('K');
        for (int i = 1; i < config_data.length() - 1 ; i++) {
          Serial.print((char)config_data.charAt(i));
          Serial1.write((char)config_data.charAt(i));
        }
        Serial.println();
        Serial1.write(0x06);
        if (aws_data.notifyEnabled()) {
          aws_data.notify("YES");
          Serial.println("Notified");
        }
      }
      else {
        if (aws_data.notifyEnabled()) {
          aws_data.notify("NO");
          Serial.println("Notified");
        }
      }
    }
    else if (config_data.charAt(0) == 'L') {
      if (config_data.charAt(config_data.length() - 1) == 0x0A) {
        Serial.println("Username");
        Serial1.write('L');
        for (int i = 1; i < config_data.length() - 1 ; i++) {
          Serial.print((char)config_data.charAt(i));
          Serial1.write((char)config_data.charAt(i));
        }
        Serial.println();
        Serial1.write(0x06);
        if (aws_data.notifyEnabled()) {
          aws_data.notify("YES");
          Serial.println("Notified");
        }
      }
      else {
        if (aws_data.notifyEnabled()) {
          aws_data.notify("NO");
          Serial.println("Notified");
        }
      }
    }
    else if (config_data.charAt(0) == 'M') {
      if (config_data.charAt(config_data.length() - 1) == 0x0A) {
        Serial.println("Password");
        Serial1.write('M');
        for (int i = 1; i < config_data.length() - 1 ; i++) {
          Serial.print((char)config_data.charAt(i));
          Serial1.write((char)config_data.charAt(i));
        }
        Serial.println();
        Serial1.write(0x06);
        if (aws_data.notifyEnabled()) {
          aws_data.notify("YES");
          Serial.println("Notified");
        }
      }
      else {
        if (aws_data.notifyEnabled()) {
          aws_data.notify("NO");
          Serial.println("Notified");
        }
      }
    }
    else if (config_data.charAt(0) == 'N') {
      if (config_data.charAt(config_data.length() - 1) == 0x0A) {
        Serial.println("Broker Address");
        Serial1.write('N');
        for (int i = 1; i < config_data.length() - 1 ; i++) {
          Serial.print((char)config_data.charAt(i));
          Serial1.write((char)config_data.charAt(i));
        }
        Serial.println();
        Serial1.write(0x06);
        if (aws_data.notifyEnabled()) {
          aws_data.notify("YES");
          Serial.println("Notified");
        }
      }
      else {
        if (aws_data.notifyEnabled()) {
          aws_data.notify("NO");
          Serial.println("Notified");
        }
      }
    }
    else if (config_data.charAt(0) == 'O') {
      if (config_data.charAt(config_data.length() - 1) == 0x0A) {
        Serial.println("Port Number");
        Serial1.write('O');
        for (int i = 1; i < config_data.length() - 1 ; i++) {
          Serial.print((char)config_data.charAt(i));
          Serial1.write((char)config_data.charAt(i));
        }
        Serial.println();
        Serial1.write(0x06);
        if (aws_data.notifyEnabled()) {
          aws_data.notify("YES");
          Serial.println("Notified");
        }
      }
      else {
        if (aws_data.notifyEnabled()) {
          aws_data.notify("NO");
          Serial.println("Notified");
        }
      }
    }
    else if (config_data.charAt(0) == 'R') {
      if (config_data.charAt(config_data.length() - 1) == 0x0A) {
        Serial.println("Rssi Filter:");
        String val = "";
        for (int i = 1; i < config_data.length(); i++) {
          val += (char)config_data.charAt(i);
        }
        Serial.println(val);
        InternalFS.remove("/RssiFilter.txt");
        if ( file.open("/RssiFilter.txt", FILE_O_WRITE) )
        {
          char conv1[val.length() + 1];
          val.toCharArray(conv1, val.length() + 1);
          Serial.println("OK");
          file.write(conv1, strlen(conv1));
          file.close();
        }
        else
        {
          Serial.println("Failed!");
        }
        if (aws_data.notifyEnabled()) {
          aws_data.notify("YES");
          Serial.println("Notified");
        }
      }
      else {
        if (aws_data.notifyEnabled()) {
          aws_data.notify("NO");
          Serial.println("Notified");
        }
      }
    }
    else if (config_data.charAt(0) == 'S') {
      if (config_data.charAt(config_data.length() - 1) == 0x0A) {
        Serial.println("Antenna parameters(Tx power)");
        String val = "";
        for (int i = 1; i < config_data.length(); i++) {
          val += (char)config_data.charAt(i);
        }
        Serial.println(val);
        InternalFS.remove("/TxPower.txt");
        if ( file.open("/TxPower.txt", FILE_O_WRITE) )
        {
          char conv1[val.length() + 1];
          val.toCharArray(conv1, val.length() + 1);
          Serial.println("OK");
          file.write(conv1, strlen(conv1));
          file.close();
        }
        else
        {
          Serial.println("Failed!");
        }
        if (aws_data.notifyEnabled()) {
          aws_data.notify("YES");
          Serial.println("Notified");
        }
      }
      else {
        if (aws_data.notifyEnabled()) {
          aws_data.notify("NO");
          Serial.println("Notified");
        }
      }
    }
    else if (config_data.charAt(0) == 'T') {
      if (config_data.charAt(config_data.length() - 1) == 0x0A) {
        Serial.println("Scan Interval ");
        String val = "";
        int i;
        for (i = 1; i < config_data.length(); i++) {
          if (config_data.charAt(i) == 0x0A)
            break;
          val += (char)config_data.charAt(i);
        }
        Serial.println(val);
        InternalFS.remove("/ScanInterval.txt");
        if ( file.open("/ScanInterval.txt", FILE_O_WRITE) )
        {
          char conv1[val.length() + 1];
          val.toCharArray(conv1, val.length() + 1);
          Serial.println("OK");
          file.write(conv1, strlen(conv1));
          file.close();
        }
        else
        {
          Serial.println("Failed!");
        }
        Serial.println("Scan Duration");
        val = "";
        for (int j = i + 1; j < config_data.length(); j++) {
          val += (char)config_data.charAt(j);
        }
        Serial.println(val);
        InternalFS.remove("/ScanDuration.txt");
        if ( file.open("/ScanDuration.txt", FILE_O_WRITE) )
        {
          char conv1[val.length() + 1];
          val.toCharArray(conv1, val.length() + 1);
          Serial.println("OK");
          file.write(conv1, strlen(conv1));
          file.close();
        }
        else
        {
          Serial.println("Failed!");
        }
        if (aws_data.notifyEnabled()) {
          aws_data.notify("YES");
          Serial.println("Notified");
        }
      }
      else {
        if (aws_data.notifyEnabled()) {
          aws_data.notify("NO");
          Serial.println("Notified");
        }
      }
    }
    else {
      if (aws_data.notifyEnabled()) {
        aws_data.notify("NO");
        Serial.println("Notified");
      }
    }
    conDataRec = false;
    config_data = "";
  }
  else if (_certRec && _keyRec) {
    //flags are set when we have completely recieved key and cert
    Serial.print("Sending Certificate: ");
    Serial1.write('F');
    for (int i = 0; i < cert.length(); i++)
    {
      Serial.print(cert.charAt(i));
      Serial1.write(cert.charAt(i));
    }
    Serial.println();
    Serial1.write(0x06);
    delay(1000);
    Serial.print("Sending Private Key: ");
    Serial1.write('G');
    for (int i = 0; i < key.length(); i++)
    {
      Serial.print(key.charAt(i));
      Serial1.write(key.charAt(i));
    }
    Serial.println();
    Serial1.write(0x06);
    cert = "";
    key = "";
    _certRec = false;
    _keyRec = false;
  }
  if (!pressed) {
    //if we are in scan mode
    if (scanned ) {
      //if we scan any POM100 device with low flag
      present = false;
      //To have a time limit of duration
      if ((millis() - cur) < duration) {
        //if we are inside limit, we check whether currently scanned device is in record or not
        if (scan_resp) {
          for (int i = 0; i < devs; i++) {
            if (scan_resp_devices[i] != NULL && mac == scan_resp_devices[i])
              present = true;
          }
        }
        else {
          for (int i = 0; i < devs; i++) {
            if (adv_devices[i] != NULL && mac == adv_devices[i])
              present = true;
          }
        }

        if (!present) {
          //If scanned device is not in record, first add it in record and then concatenate it's data
          if (scan_resp) {
            for (int i = 0; i < devs; i++) {
              if (scan_resp_devices[i] == NULL) {
                scan_resp_devices[i] = mac;
                break;
              }
            }
          }
          else {
            for (int i = 0; i < devs; i++) {
              if (adv_devices[i] == NULL) {
                adv_devices[i] = mac;
                break;
              }
            }
          }

          if (scan_resp) {
            Serial.println("Scan Resp packet");
          }
          else {
            Serial.println("Adv packet");
          }
          str += mac;
          str += rss;
          str += len;
          str += hex_data;
          str += ";";
        }
      }
      else {
        //If duration is crossed, set records of devices to null and send concatenated string
        if (str.length() > 0) {
          Serial.println();
          Serial.println("Duration done");
          //Serial.println(str);
          Serial1.write('Y');
          Serial1.write(str.length());
          Serial1.write(str.length() / 256);
          for (int i = 0; i < str.length(); i++) {
            Serial1.write(str.charAt(i));
            Serial.write(str.charAt(i));
          }
          Serial1.write('\n');
          Serial.println();
        }

        str = "";
        for (int i = 0; i < devs; i++)
        {
          adv_devices[i] = "";
          scan_resp_devices[i] = "";
        }
        //update and save current time
        cur = millis();
      }
      scanned = false;
    }
    //In Normal mode, if we had a connection request through command but we could not find that
    //mac in time limit, then send message to esp and continue with scan
    if (con && !found && (millis() - cur > DEVICE_SEARCH_LIMIT)) {
      Serial.print("Time out searching for mac: ");
      Serial.println(write_mac);
      Serial1.print('E');
      Serial1.print('7');
      Serial1.print('\n');
      Bluefruit.Scanner.filterUuid(FILTER_SERVICE_UUID);
      con = false;
      sen = false;
      cur = millis();
      //reinitialize str and all enteries in saved addresses
      str = "";
      for (int i = 0; i < devs; i++)
      {
        adv_devices[i] = "";
        scan_resp_devices[i] = "";
      }
    }
    //If limit for notification messages is reached but complete data isn't recived, send buffer of BUFFSIZE
    if (_buffComplete) {
      Serial.println("Buffer Complete");
      String dataSen = "";
      char hexCar[2];
      if (_command == 3) {
        sprintf(hexCar, "%02X", bidN);
        dataSen += hexCar;
        sprintf(hexCar, "%02X", midN);
        dataSen += hexCar;
        midN++;
      }
      else {
        sprintf(hexCar, "%02X", bid);
        dataSen += hexCar;
        sprintf(hexCar, "%02X", mid);
        dataSen += hexCar;
        mid++;
        sprintf(hexCar, "%02X", (dataItems - CurrentItems));
        dataSen += hexCar;
      }
      dataSen += deciveMac;
      sprintf(hexCar, "%02X", BUFFSIZE);
      dataSen += hexCar;
      dataSen += buffRecs;
      Serial.print("Data Sent: ");
      Serial.println(dataSen);
      Serial1.write('U');
      Serial1.write(_command == 3 ? 'N' : 'R');
      Serial1.print(dataSen);
      Serial1.write('\n');
      _buffComplete = false;
      buffRecs = "";
    }
    //If data from notification is completed
    if (_dataComplete) {
      Serial.println("Data Complete");
      String dataSen = "";
      char hexCar[2];
      //See if it's due to command or flag HIGH
      if (_command == 3) {
        sprintf(hexCar, "%02X", bidN);
        dataSen += hexCar;
        sprintf(hexCar, "%02X", midN);
        dataSen += hexCar;
      }
      else {
        sprintf(hexCar, "%02X", bid);
        dataSen += hexCar;
        sprintf(hexCar, "%02X", mid);
        dataSen += hexCar;
        bid++;
        mid++;
        sprintf(hexCar, "%02X", (dataItems - CurrentItems));
        dataSen += hexCar;
      }
      dataSen += deciveMac;
      sprintf(hexCar, "%02X", ((CurrentItems % 10 == 0) ? 10 : CurrentItems % 10));
      dataSen += hexCar;
      dataSen += buffRecs2;
      Serial.print("Data Sent: ");
      Serial.println(dataSen);
      Serial1.write('U');
      Serial1.write(_command == 3 ? 'N' : 'R');
      Serial1.print(dataSen);
      Serial1.write('\n');
      strRecords = "";
      buffRecs = "";
      found = false;
      Bluefruit.disconnect(conHandle);
      _dataComplete = false;
      _buffComplete = false;
      _command = 0;
      buffRecs2 = "";
    }
    if (Serial1.available()) {
      //To recieve command from esp (device connected to internet)
      char c;
      c = Serial1.read();
      if (c == 's') {
        //Command is started
        inp = "";
        Serial.println("Stoppping Scanner");
        Bluefruit.Scanner.stop();
      }
      else if (c == '\n') {
        //Command is recieved completely
        Serial.print("Read from ESP: ");
        Serial.println(inp);
        char command = inp.charAt(0);
        Serial.print("Received Length : ");
        Serial.println(inp.length() - 1);
        String data = inp.substring(1, inp.length());
        _command = (int)command;
        Serial.print("Recieved command : ");
        Serial.println(_command);
        Bluefruit.Scanner.clearFilters();
        write_mac = data.substring(0, 12);
        String Service = data.substring(12, 44);
        String Charact = data.substring(44, 76);
        Str_to_Hex_Rev(Service_UUID, Service);
        Str_to_Hex_Rev(Charteristic_UUID, Charact);
        service_beacon.setUuid(Service_UUID);
        characteristic_beacon.setUuid(Charteristic_UUID);
        service_beacon.begin();
        characteristic_beacon.begin();
        Serial.print("Looking for mac: ");
        Serial.println(write_mac);
        con = true;
        val = data.substring(76, data.length());
        Serial.print("With Data: ");
        val.toUpperCase();
        Serial.println(val);
        Bluefruit.Scanner.setRxCallback(scan_callback);
        Bluefruit.Scanner.restartOnDisconnect(true);
        file.open("/ScanInterval.txt", FILE_O_READ);
        if (file) {
          uint32_t readlen;
          char buffer[64] = { 0 };
          readlen = file.read(buffer, sizeof(buffer));
          buffer[readlen] = 0;
          String val = String(buffer);
          scanInt = val.toInt();
          Serial.print("Found updated value for Scan Interval: ");
          Serial.println(scanInt);
          file.close();
        }
        else {
          Serial.println("Using default value for Scan Interval");
        }
        int interval = (int)scanInt / 0.625;
        Bluefruit.Scanner.setInterval(interval, interval / 2);     // in units of 0.625 ms
        Bluefruit.Scanner.useActiveScan(true);        // Request active data
        Bluefruit.Scanner.start(0);
        cur = millis();
      }
      else {
        inp += (char)c;
      }
    }
    else {
      delay(10);
    }
  }
}

//Random function to convert recieved string UUID to hex UUID and also in reverse
void Str_to_Hex_Rev (uint8_t out[16], String inp) {
  if (inp.length() != 32) {
    Serial.println("String size is less than 16*2");
    return;
  }
  int pt = 15;
  inp.toUpperCase();
  for (int i; i < inp.length(); i += 2) {
    int val1 = hexCharToInt(inp.charAt(i));
    int val2 = hexCharToInt(inp.charAt(i + 1));
    int _value = (((val1 % 16) << 4) & (0xF0)) + ((val2 % 16) & 0x0F);
    out[pt] = _value;
    pt--;
  }
}

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));

  Serial.print("Connected to ");
  Serial.println(central_name);
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  Serial.println();
  Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);
}

// callback invoked when device connects to peripheral client
void connect_callback2(uint16_t conn_handle)
{
  /*
     This Callback is invoked when we have either
     1) High POM100 flag (sen will be true)
     2) Write Command (sen will be false and _command will be 1)
     3) Read Command (sen will be false and _command will be 2)
     4) Subscibe Command (sen will be false and _command will be 3)
  */
  Serial.println("");
  Serial.println("Connect Callback ");
  Serial.println( conn_handle );
  if (sen) {
    Serial.print("Discovering Notify Service ... ");
    vTaskDelay(1000);
    if ( !notifyService.discover(conn_handle) )
    {
      Serial.println("No Service Found");
      // disconnect since we couldn't find service
      Bluefruit.disconnect(conn_handle);
      return;
    }
    Serial.println("Service Found");

    // Once service is found, we continue to discover the primary characteristic
    Serial.print("Discovering Notify Characteristic ... ");
    if ( !notifyCharacteristic.discover() )
    {
      // Measurement chr is mandatory, if it is not found (valid), then disconnect
      Serial.println("No Characteristic Found. Characteristic is mandatory but not found. ");
      Bluefruit.disconnect(conn_handle);
      return;
    }
    Serial.println("Characteristic Found");

    Serial.print("Discovering Data Collection Configuration Characteristic ... ");
    if ( !notifyCharacteristicEnabler.discover() )
    {
      Serial.println("No Characteristic Found. Characteristic is mandatory but not found.");
      Bluefruit.disconnect(conn_handle);
      return;
    }
    Serial.println("Characteristic Found");
    strRecords = "";

    Serial.print("Enabling Data Collection, using value: ");
    uint8_t key[] = {0x02, 0x7A};
    for (int i = 0; i < 2; i++)
      Serial.print(key[i], HEX);
    Serial.println();
    Serial.print("Wrote: ");
    Serial.print( notifyCharacteristicEnabler.write( key, 2 ) );
    Serial.println(" Bytes");
    vTaskDelay(1000);
    uint16_t val;
    if (notifyCharacteristicEnabler.read(&val, 2)) {
      Serial.print("Read vlaue for No. of records: ");
      Serial.print(val);
      dataItems = (int)val;
      CurrentItems = 0;
      Serial.println();
    }
    else {
      Serial.println("Read Failed");
      Bluefruit.disconnect(conn_handle);
      return;
    }
    if (val == 0) {
      Bluefruit.disconnect(conn_handle);
      return;
    }
    // Reaching here means we are ready to go, let's enable notification on measurement chr
    Serial.print("Enabling Notify on the Characteristic ... ");
    if ( notifyCharacteristic.enableNotify() )
    {
      Serial.println("enableNotify success, now ready to receive Characteristic values");
    } else
    {
      Serial.println("Couldn't enable notify for Characteristic. Increase DEBUG LEVEL for troubleshooting");
      Bluefruit.disconnect(conn_handle);
      return;
    }
    sen = false;
    con = false;
    conHandle = conn_handle;
    _dataComplete = false;
    _buffComplete = false;
  }
  else {
    // If Service is not found, disconnect and return
    Serial.print("Discovering Service ... ");
    if ( !service_beacon.discover(conn_handle) )
    {
      Serial.println("No Service Found");
      Serial1.print('E');
      Serial1.print('1');
      Serial1.print('\n');
      // disconnect since we couldn't find service
      Bluefruit.disconnect(conn_handle);
      return;
    }
    Serial.println("Service Found");

    // Once service is found, we continue to discover the primary characteristic
    Serial.print("Discovering Characteristic ... ");
    if ( !characteristic_beacon.discover() )
    {
      // Measurement chr is mandatory, if it is not found (valid), then disconnect
      Serial.println("No Characteristic Found. Characteristic is mandatory but not found. ");
      Serial1.print('E');
      Serial1.print('2');
      Serial1.print('\n');
      Bluefruit.disconnect(conn_handle);
      return;
    }
    Serial.println("Characteristic Found");
    delay(500);
    uint8_t props =  characteristic_beacon.properties();
    Serial.println(props, HEX);
    switch (_command) {
      case 1:
        {
          if (((props & 0x08) != 0x08) || (props & 0x04 != 0x04)) {
            Serial.println("Write Permissions not enabled...!");
            Serial1.print('E');
            Serial1.print('3');
            Serial1.print('\n');
            Bluefruit.disconnect(conn_handle);
            delay(100);
            return;
          }
          Serial.print("Writing Value: ");
          int _size = val.length() / 2;
          uint8_t key[_size];
          int pt = 0;
          val.toUpperCase();
          for (int i; i < val.length(); i += 2) {
            int val1 = hexCharToInt(val.charAt(i));
            int val2 = hexCharToInt(val.charAt(i + 1));
            int _value = (((val1) << 4) & (0xF0)) + ((val2) & 0x0F);
            key[pt] = _value;
            pt++;
          }
          for (int i = 0; i < _size; i++)
            Serial.print(key[i], HEX);
          Serial.println();
          Serial.print("Wrote: ");
          Serial.print( characteristic_beacon.write( key, _size ) );
          Serial.println(" Bytes");
          delay(200);
          Bluefruit.disconnect(conn_handle);
        }
        break;
      case 2:
        {
          if ((props & 0x02) != 0x02) {
            Serial.println("Read Permissions not enabled...!");
            Serial1.print('E');
            Serial1.print('4');
            Serial1.print('\n');
            Bluefruit.disconnect(conn_handle);
            delay(100);
            return;
          }
          int val1 = hexCharToInt(val.charAt(0));
          int val2 = hexCharToInt(val.charAt(1));
          uint8_t _readBytes = (((val1) << 4) & (0xF0)) + ((val2) & 0x0F);
          uint8_t valu[_readBytes];
          if (characteristic_beacon.read(&valu, _readBytes)) {
            outVal = "";
            char hexCar[2];
            for (int i = 0; i < _readBytes; i++) {
              sprintf(hexCar, "%02X", valu[i]);
              outVal += hexCar;
            }
            Serial.print("Read vlaue for ");
            Serial.print(val);
            Serial.print(" number of records: 0x");
            Serial.println(outVal);
            Serial1.write('W');
            Serial1.print(write_mac);
            Serial1.print(outVal);
            Serial1.print(";");
            Serial1.write('\n');
            _readData = true;
          }
          else {
            Serial.println("Read Fialed due to unknown reason");
            Serial1.print('E');
            Serial1.print('5');
            Serial1.print('\n');
          }
          delay(200);
          Bluefruit.disconnect(conn_handle);
        }
        break;
      case 3:
        {
          if ((props & 0x10) != 0x10) {
            Serial.println("Notification Property not found...!");
            Serial1.print('E');
            Serial1.print('6');
            Serial1.print('\n');
            Bluefruit.disconnect(conn_handle);
            delay(100);
            return;
          }
          bidN = 1;
          midN = 1;
          strRecords = "";
          buffRecs = "";
          characteristic_beacon.setNotifyCallback(notify_callback);
          uint16_t items;

          int val1 = hexCharToInt(val.charAt(0));
          int val2 = hexCharToInt(val.charAt(1));
          int val3 = hexCharToInt(val.charAt(2));
          int val4 = hexCharToInt(val.charAt(3));
          uint8_t _value1 = (((val1) << 4) & (0xF0)) + ((val2) & 0x0F);
          uint8_t _value2 = (((val3) << 4) & (0xF0)) + ((val4) & 0x0F);
          items = (((_value2) << 8) & (0xFF00)) + (((_value1) & 0x00FF));
          dataItems = items;
          CurrentItems = 0;
          Serial.print("Read vlaue for No. of records: ");
          Serial.println(dataItems);
          conHandle = conn_handle;
          _dataComplete = false;
          _buffComplete = false;
          deciveMac = write_mac;
          if ( characteristic_beacon.enableNotify() )
          {
            Serial.println("enableNotify success, now ready to receive Characteristic values");
          } else
          {
            Serial.println("Couldn't enable notify for Characteristic. Increase DEBUG LEVEL for troubleshooting");
            Bluefruit.disconnect(conn_handle);
            delay(100);
            return;
          }
          //Serial.println(item,HEX)
        }
        break;
      default:
        Serial.print("Value ");
        Serial.print(_command);
        Serial.print(" Has No Match!!");
        break;
    }
  }
}

//Function to convert character hex to actual hex
int hexCharToInt(char c) {
  int val = (int)c;
  if ((val >= 'A' && val <= 'F'))
    val = 9 + (val % 16);
  return val % 16;
}

void disconnect_callback2(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;
  Bluefruit.Scanner.filterUuid(FILTER_SERVICE_UUID);
  Serial.println();
  Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);
  con = false;
  sen = false;
  found = false;
}

//this call back is invoked when we reciev a notification through desired characteristic
void notify_callback(BLEClientCharacteristic* chr, uint8_t* data, uint16_t len)
{
  if (CurrentItems < dataItems) {
    Serial.print("Data # ");
    Serial.print(CurrentItems + 1);
    Serial.print(" :");
    char hexCar[2];
    for (int i = 0; i < len; i++) {
      sprintf(hexCar, "%02X", data[i]);
      Serial.print(hexCar);
      strRecords += hexCar;
    }
    Serial.println();
    strRecords += ';';
    CurrentItems++;
    if (CurrentItems >= dataItems) {
      _dataComplete = true;
      buffRecs2 = strRecords;
      strRecords = "";
    }
    else if (CurrentItems % BUFFSIZE == 0) {
      _buffComplete = true;
      buffRecs = strRecords;
      strRecords = "";
    }
  }
}
