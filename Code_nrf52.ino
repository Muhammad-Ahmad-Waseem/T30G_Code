#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

using namespace Adafruit_LittleFS_Namespace;
#define devs 100
#define MAX_PRPH_CONNECTION   2
#define BUFFSIZE 10

//uint8_t BEACON_SERVICE_UUID[] =               {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB0, 0x00, 0x40, 0x51, 0x04, 0xD0, 0xFF, 0x00, 0xF0};
//uint8_t BEACON_CHARACTERISTIC_UUID[] =        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB0, 0x00, 0x40, 0x51, 0x04, 0xD1, 0xFF, 0x00, 0xF0};

uint8_t BEACON_SERVICE_UUID_D[] =               {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t BEACON_CHARACTERISTIC_UUID_D[] =        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

uint8_t FILTER_SERVICE_UUID[] =               {0x31, 0x2d, 0x44, 0x4b, 0x50, 0x2d, 0x50, 0x45, 0x2d, 0x30, 0x30, 0x31, 0x2d, 0x4d, 0x4f, 0x50};
uint8_t NOTIFY_SERVICE_UUID[] =               {0x33, 0x4d, 0x44, 0x4b, 0x50, 0x2d, 0x50, 0x45, 0x2d, 0x30, 0x30, 0x31, 0x2d, 0x4d, 0x4f, 0x50};
uint8_t NOTIFY_CHARACTERISTIC_UUID[] =        {0x33, 0x4d, 0x44, 0x4b, 0x50, 0x2d, 0x50, 0x45, 0x2d, 0x30, 0x30, 0x31, 0x4d, 0x5d, 0x4f, 0x50};
uint8_t NOTIFY_CHARACTERISTIC_UUID_ENABLER[] = {0x33, 0x4d, 0x44, 0x4b, 0x50, 0x2d, 0x50, 0x45, 0x2d, 0x30, 0x30, 0x31, 0x3d, 0x5d, 0x4f, 0x50};

BLEClientService        notifyService(NOTIFY_SERVICE_UUID);
BLEClientCharacteristic notifyCharacteristic(NOTIFY_CHARACTERISTIC_UUID);
BLEClientCharacteristic notifyCharacteristicEnabler(NOTIFY_CHARACTERISTIC_UUID_ENABLER);

uint8_t Service_UUID[16];
uint8_t Charteristic_UUID[16];

File file(InternalFS);

BLEClientService        service_beacon(BEACON_SERVICE_UUID_D);
BLEClientCharacteristic characteristic_beacon(BEACON_CHARACTERISTIC_UUID_D);

//const char characteristic_beacon[] =  "f000ffd1-0451-4000-b000-000000000000";

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
BLEService        aws(AWS_UUID_SERVICE);
BLECharacteristic aws_data(AWS_UUID_CHR_DATA);

//This boolean will tell whther the scanned mac address is present in adresses or not. Con is used to keep track of whther we have a connection request or not
bool present, con = false, pressed = false, scanned = false, conDataRec = false, scan_resp, _cert = false, _key = false, _certRec = false, _keyRec = false, sen = false, _dataComplete = false, _buffComplete = false, found = false, _readData = false;

//Antennna Paremeters
int scanInt = 100, duration = 1000; //(in ms) (scan int (multiple of 5), duration int (1000,10000))

uint16_t lengthCert, lengthKey, conHandle;
uint8_t notifyKey[2];

//Tx power
int TxPwr = 8; //(-40, -20, -16, -12, -8, -4, 0, 2, 3, 4, 5, 6, 7, 8)

//Rssi filter
int filtRss = -100;//(integer between -50 to -100)

//An array to keep track of all scanned mac adresses in one second, str is used to concatenate all data of all unique adresses. macadd will contain required
// mac address to which we need to make a connection. mac will temporarily have scanned peripheral and will compare it with macadd. inp will be a string
// recieved from esp containing mac adress and value.
String adv_devices[devs], scan_resp_devices[devs], str = "", mac = "", rss, inp, hex_data, len, config_data, write_mac, cert = "", key = "", strRecords, buffRecs = "", outVal = "", val = "",  deciveMac; // = "d094ef9c4619";

// constants won't change. They're used here to set pin numbers:
const int buttonPin = 5;     // the number of the pushbutton pin(P0.05)

// variables will change:
int buttonState = 0;         // variable for reading the pushbutton status

// curr is used to kepp track of time. i is used in for loops. val contains the value which we need to write at specfiied mac address. tries keep a track of
// multiple tries to connect to specific mac and also tries to detect the attributes.
int cur , i, tries = 0, dataItems, CurrentItems, bid = 1, mid = 1, bidN, midN , _command = 0;


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
    if (report->rssi > filtRss) {
      /* Display the timestamp and device address */
      if (report->type.scan_response)
      {
        scan_resp = true;
      }
      else
      {
        scan_resp = false;
      }
      /* RSSI value */
      int8_t _addr = report->rssi;
      //Serial.println(_addr);
      rss = String((char)_addr, HEX);
      rss.toUpperCase();

      sprintf(hexCar, "%02X", report->data.len);
      len = hexCar;


      uint8_t* data = report->data.p_data;
      hex_data = "";
      if (report->data.len)
      {
        //Serial.print("Hex_char_data: " );
        for (int i = 0; i < report->data.len; i++) {
          sprintf(hexCar, "%02X", data[i]);
          hex_data += hexCar;
          //Serial.print(hexCar);
        }
        Serial.println(data[29], HEX);
        Serial.println(((data[29] & 0x80) == 0x80) ? "High" : "LOW");
        if ((data[29] & 0x80) == 0x80) {
          sen = true;
          deciveMac = mac;
          Bluefruit.Central.connect(report);
        }
        else {
          Bluefruit.Scanner.resume();
          scanned = true;
        }
      }
    }
    else {
      // For Softdevice v6: after received a report, scanner will be paused
      // We need to call Scanner resume() to continue scanning
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


void setup() {
  pressed = false;

  //while(!Serial);
  pinMode(buttonPin, INPUT);
  delay(5000);
  // Initialize both serial coms
  Serial.begin(115200);

  // Initialize Internal File System
  InternalFS.begin();
  // read the state of the pushbutton value:
  buttonState = digitalRead(buttonPin);
  if (buttonState == HIGH) {
    // turn LED on:
    Serial.println("Configuration Mode");
    pressed = true;
  }
  //pressed = true;
  Serial1.begin(115200);  //Rx=P1.01, Tx = P1.02
  delay(500);
  Serial1.print('\n');
  if (pressed) {
    Serial1.write('Z');
    Serial1.print("conf");
    Serial1.write(0x06);
    Bluefruit.begin(MAX_PRPH_CONNECTION, 0);
    Bluefruit.Periph.setConnectCallback(connect_callback);
    Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
    Bluefruit.setName("T30G");
    // Note: You must call .begin() on the BLEService before calling .begin() on
    // any characteristic(s) within that service definition.. Calling .begin() on
    // a BLECharacteristic will cause it to be added to the last BLEService that
    // was 'begin()'ed!
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
    // Initialize Bluefruit with maximum connections as Peripheral = 0, Central = 1
    // SRAM usage required by SoftDevice will increase dramatically with number of connections
    file.open("/TxPower.txt", FILE_O_READ);
    if (file) {
      uint32_t readlen;
      char buffer[64] = { 0 };
      readlen = file.read(buffer, sizeof(buffer));
      buffer[readlen] = 0;
      String val = String(buffer);
      TxPwr = val.toInt();
      Serial.print("Found updated value for tx");
      Serial.println(TxPwr);
      file.close();
    }
    else {
      Serial.println("Using default value for Tx: ");
    }
    Bluefruit.begin(0, 1);
    Bluefruit.setTxPower(TxPwr);    // Check bluefruit.h for supported values

    // Initialize Service
    notifyService.begin();

    // set up callback for receiving measurement
    notifyCharacteristic.setNotifyCallback(notify_callback);
    notifyCharacteristic.begin();

    // set up the characteristic to enable the optical component on the device
    notifyCharacteristicEnabler.begin();

    // Increase Blink rate to different from PrPh advertising mode
    Bluefruit.setConnLedInterval(250);

    // Callbacks for Central
    Bluefruit.Central.setDisconnectCallback(disconnect_callback2);
    Bluefruit.Central.setConnectCallback(connect_callback2);

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
    Bluefruit.Scanner.filterUuid(FILTER_SERVICE_UUID);
    Bluefruit.Scanner.start(0);                   // 0 = Don't stop scanning after n seconds

    //Serial.println("Scanning ...");
    //capture current time, just before running loop
    cur = millis();
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
      Serial.println("Using default value for Rss filter");
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
      Serial.println("Using default value for Scan Duration");
    }

  }
}
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


void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include HRM Service UUID
  Bluefruit.Advertising.addService(aws);

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();

  /* Start Advertising
     - Enable auto advertising if disconnected
     - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
     - Timeout for fast mode is 30 seconds
     - Start(timeout) with timeout = 0 will advertise forever (until connected)

     For recommended advertising interval
     https://developer.apple.com/library/content/qa/qa1931/_index.html
  */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
}



void loop() {
  if (pressed && conDataRec) {
    if (_cert) {
      for (int i = 0; i < config_data.length(); i++) {
        if ((char)config_data.charAt(i) == '\t') {
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
          cert += (char)config_data.charAt(i);
          lengthCert--;
        }
      }
      Serial.print("Remaining Length: ");
      Serial.println(lengthCert);

    }
    else if (_key) {
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
    }
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
      if (_certRec) {
        cert = "";
        key = "";
      }
      //Serial1.write('F');
      _cert = true;
      //Serial1.write(0x06);
      for (int i = 1; i < config_data.length() ; i++) {
        //Serial.print((char)config_data.charAt(i));
        cert += (char)config_data.charAt(i);
        lengthCert--;
      }
      Serial.print("Remaining Length: ");
      Serial.println(lengthCert);
    }
    else if (config_data.charAt(0) == 'P') {
      String len = "";
      if (config_data.charAt(config_data.length() - 1) == 0x0A) {
        for (int i = 1; i < config_data.length() - 1 ; i++) {
          len +=  (char)config_data.charAt(i);
        }
        Serial.println("Length of Cert Sent");
        Serial.println(len);
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
      _key = true;
      for (int i = 1; i < config_data.length() ; i++) {
        //Serial.print((char)config_data.charAt(i));
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
    if (scanned ) {
      //At start we assume mac is not present in out list of sending data per second
      present = false;
      //To have a time limit of one second
      if ((millis() - cur) < duration) {
        if (scan_resp) {
          for (int i = 0; i < devs; i++) {
            if (scan_resp_devices[i] != NULL && mac == scan_resp_devices[i])
              present = true;
          }
        }
        else {
          //checks whether mac adress of scanned obj is presnt in array or not
          for (int i = 0; i < devs; i++) {
            if (adv_devices[i] != NULL && mac == adv_devices[i])
              present = true;
          }
        }

        // If not present, then update list of mac addresses and concatenate str
        if (!present) {
          //Enter current mac address in array
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
      //if one second is past
      else {
        if (str.length() > 0) {
          // printing to see working
          Serial.println();
          Serial.println("Duration done");
          Serial.println(str);
          //Serial.println(str.length());
          //Send that data to esp via serial1
          Serial1.write('Y');
          Serial1.write(str.length());
          Serial1.write(str.length() / 256);
          for (int i = 0; i < str.length(); i++)
            Serial1.write((char)str[i]);
          //Serial1.write('\n');
        }
        //Serial1.print(str);

        //update and save current time
        cur = millis();

        //reinitialize str and all enteries in saved addresses
        str = "";
        for (int i = 0; i < devs; i++)
        {
          adv_devices[i] = "";
          scan_resp_devices[i] = "";
        }
      }
      scanned = false;
    }
    if (con && !found && (millis() - cur > 10000)) {
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
    if (!_dataComplete && _buffComplete) {
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

      sprintf(hexCar, "%02X", 10);
      dataSen += hexCar;
      dataSen += buffRecs;
      Serial.print("Data Sent: ");
      Serial.println(dataSen);
      Serial1.write('U');
      Serial1.write(_command == 3 ? 'N' : 'R');
      Serial1.print(dataSen);
      Serial1.write('\n');
      _buffComplete = false;
    }
    if (_dataComplete) {
      Serial.println("Data Complete");
      String dataSen = "";
      char hexCar[2];
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
      dataSen += buffRecs;
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
    }
    if (Serial1.available()) {
      //read address which we need to conect to and also data
      char c;

      c = Serial1.read();

      if (c == 's') {
        inp = "";
        Serial.println("Stoppping Scanner");
        Bluefruit.Scanner.stop();
      }
      else if (c == '\n') {
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
void Str_to_Hex_Rev (uint8_t out[16], String inp) {
  if (inp.length() != 32) {
    Serial.println("String size is less than 16*2");
    return;
  }
  int pt = 15;
  inp.toUpperCase();
  for (int i; i < inp.length(); i += 2) {
    int val1 = (int)inp.charAt(i);
    int val2 = (int)inp.charAt(i + 1);
    if ((val1 >= 'A' && val1 <= 'F'))
      val1 = 9 + (val1 % 16);
    if ((val2 >= 'A' && val2 <= 'F'))
      val2 = 9 + (val2 % 16);
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

/**
   Callback invoked when a connection is dropped
   @param conn_handle connection where this event happens
   @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
*/
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  Serial.println();
  Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);
}

// callback invoked when central connects
void connect_callback2(uint16_t conn_handle)
{
  Serial.println("");
  Serial.println("Connect Callback ");
  Serial.println( conn_handle );
  if (sen) {
    Serial.print("Discovering Notify Service ... ");
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

    // Data Collection Charactistic. Find and enable.
    // You enable data collection on the Characteristic before the peripheral will start measuring values.
    // This is different than setting the Notify descriptor, which is handled below.
    Serial.print("Discovering Data Collection Configuration Characteristic ... ");
    if ( !notifyCharacteristicEnabler.discover() )
    {
      Serial.println("No Characteristic Found. Characteristic is mandatory but not found.");
      Bluefruit.disconnect(conn_handle);
      return;
    }
    Serial.println("Characteristic Found");
    strRecords = "";

    // Found it, now write 0x01 to turn on optical data collection
    Serial.print("Enabling Data Collection, using value: ");
    uint8_t key[] = {0x02, 0x7A};
    for (int i = 0; i < 2; i++)
      Serial.print(key[i], HEX);
    Serial.println();
    Serial.print("Wrote: ");
    Serial.print( notifyCharacteristicEnabler.write( key, 2 ) );
    Serial.println(" Bytes");
    delay(1000);
    uint16_t val;
    if (notifyCharacteristicEnabler.read(&val, 2)) {
      Serial.print("Read vlaue for No. of records: ");
      Serial.print(val);
      dataItems = (int)val;
      CurrentItems = 0;
      Serial.println();
    }
    else {
      Serial.println("Read Fialed");
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
    Serial.println(props,HEX);
    switch (_command) {
      case 1:
        {
          if (((props & 0x08) != 0x08) || (props & 0x04 != 0x04)) {
            Serial.println("Write Permissions not enabled...!");
            Serial1.print('E');
            Serial1.print('3');
            Serial1.print('\n');
            Bluefruit.disconnect(conn_handle);
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
            return;
          }
          bidN = 1;
          midN = 1;
          strRecords = "";
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

int hexCharToInt(char c) {
  int val = (int)c;
  if ((val >= 'A' && val <= 'F'))
    val = 9 + (val % 16);
  return val % 16;
}

/**
   Callback invoked when a connection is dropped
   @param conn_handle connection where this event happens
   @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
*/
void disconnect_callback2(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  Serial.println();
  delay(100);
  Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);
  con = false;
  sen = false;
  found = false;
  Bluefruit.Scanner.filterUuid(FILTER_SERVICE_UUID);
}

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
      buffRecs = strRecords;
      strRecords = "";
    }
    else if (CurrentItems % BUFFSIZE == 0) {
      _buffComplete = true;
      buffRecs = strRecords;
      strRecords = "";
    }
  }
}
