#include <Arduino.h>
#include <LiquidCrystal.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>

#define bleServerName "DAN_BLE"
LiquidCrystal lcd(19, 23, 18, 17, 16, 15); // Define LCD display pins RS, E, D4, D5, D6, D7
bool deviceConnected = false;
BLECharacteristic *pcharacteristic;

#define SERVICE_UUID "f60cef1e-4ddf-4813-b228-9b29445e2407"
#define CHARACTERISTIC_UUID "a62d8949-478e-465b-969e-fcce05565953"
BLECharacteristic dataCharacteristic(CHARACTERISTIC_UUID);

BLECharacteristic characteristic(
  CHARACTERISTIC_UUID,
  BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
);
BLEDescriptor *characteristicDescriptor = new BLEDescriptor(BLEUUID((uint16_t)0x2902));

// Setup callbacks onConnect and onDisconnect (no change necessary)
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Device connected");
  };
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Device disconnected");
  }
};

void getLCDs(BLECharacteristic* pcharacteristic, const String& teamId){
  DynamicJsonDocument resdoc(15000);
  JsonObject resobj = resdoc.to<JsonObject>();
  //LCD
  JsonObject lcd = resobj.createNestedObject("lcd");
  resobj["type"]="16x2";
  resobj["interface"]="Parallel 4-bit";
  resobj["resolution"]="16x2";
  resobj["id"]=1;

  resobj["teamId"]=teamId;
  //serialize the response to a JSON String
  String resstring;
  serializeJson(resdoc, resstring);
  //set the response string to the ble characteristic
  pcharacteristic->setValue(resstring.c_str());
  pcharacteristic->notify();
}

void setText(BLECharacteristic* pcharacteristic, JsonObject& req, const String& teamId){
  int lcdId = req["id"].as<int>();
  JsonArray textArray = req["text"].as<JsonArray>();
  lcd.clear();
  // print text on lcd
  for (int i = 0; i < textArray.size() && i < 2; i++) {
    String text = textArray[i].as<String>();
    lcd.setCursor(0, i);
    lcd.print(text);
  }
  
  // Prepare the response object
  DynamicJsonDocument resdoc(15000);
  JsonObject resobj = resdoc.to<JsonObject>();
  resobj["id"] = lcdId;
  resobj["text"] = textArray;
  resobj["teamId"] = teamId;

  // Serialize the response to a JSON string
  String resstring;
  serializeJson(resdoc, resstring);

  // Set the response string to the BLE characteristic
  pcharacteristic->setValue(resstring.c_str());
  pcharacteristic->notify();
}

void setIconsreq(BLECharacteristic* pcharacteristic, JsonObject& reqobj, const String& teamId) {
  int lcdId = reqobj["id"].as<int>();
  JsonArray iconsArray = reqobj["icons"].as<JsonArray>();

  // Process the icons
  int numIcons = iconsArray.size();
  // Perform the necessary operations to set the icons on the LCD
  lcd.begin(16,2);
  lcd.clear(); 
  int currentColumn = 0;
    for (const auto& icon : iconsArray) {
    // Extract the icon name and data
    String iconName = icon["name"].as<String>();
    JsonArray iconData = icon["data"].as<JsonArray>();

    // Create the custom character using the icon data
    byte iconBytes[8];
    for (int i = 0; i < 8; i++) {
      iconBytes[i] = iconData[i].as<int>();
    }

    // Create the custom character
    lcd.createChar(currentColumn, iconBytes);
    
    // Display the custom character on the LCD
    lcd.setCursor(currentColumn, 0);
    lcd.write((byte)(currentColumn));
    currentColumn++;
  }
  // Prepare the response
  DynamicJsonDocument resdoc(15000);
  JsonObject resobj = resdoc.to<JsonObject>();
  resobj["id"] = lcdId;
  resobj["number_icons"] = numIcons;
  resobj["teamId"] = teamId;

  // Serialize the response to a JSON string
  String resstring;
  serializeJson(resdoc, resstring);

  // Send the response to the app
  pcharacteristic->setValue(resstring.c_str());
  pcharacteristic->notify();
}

bool isScrolling = false;
bool DirectionLeft = true;

void scrollText() { {
    if (DirectionLeft) {
      lcd.scrollDisplayLeft();
    } else {
      lcd.scrollDisplayRight();
    }
    delay(200);
  }
}

void scrollreq(BLECharacteristic *pcharacteristic, JsonObject &reqobj, const String& teamId){
  int lcdId = reqobj["id"].as<int>();
  String Direction = reqobj["direction"].as<String>();

  // Process the scroll direction
  if (Direction == "Left") {
    // Start scrolling the text
    DirectionLeft = true;
    isScrolling = true;
  } else if (Direction == "Right") {
    // Start scrolling the text
    DirectionLeft = false;
    isScrolling = true;
  } else if (Direction == "Off") {
    // Stop scrolling the text
    isScrolling = false;
  }

  // Prepare the response
  DynamicJsonDocument resdoc(15000);
  JsonObject resobj = resdoc.to<JsonObject>();
  resobj["id"] = lcdId;
  resobj["scrolling"] = Direction;
  resobj["teamId"] = teamId;

  // Serialize the response to a JSON string
  String resstring;
  serializeJson(resobj, resstring);

  // Send the response to the app
  pcharacteristic->setValue(resstring.c_str());
  pcharacteristic->notify();
}

class CharacteristicsCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pcharacteristic) {
      std::string value = pcharacteristic->getValue();

      if (value.length() > 0) {
      // Parse the received JSON req
        DynamicJsonDocument reqDoc(15000);
        DeserializationError error = deserializeJson(reqDoc, value.c_str());

        if (error) {
          Serial.print("JSON parsing error: ");
          Serial.println(error.c_str());
          return;
        }
        JsonObject reqobj = reqDoc.as<JsonObject>();
        String action = reqobj["action"].as<String>();
        String teamID = reqobj["teamId"].as<String>();
        String teamId = "A39";
        Serial.println(teamId);
        if (action == "getLCDs") {
          getLCDs(pcharacteristic, teamId);
        }
        else 
          if (action == "setText") {
            setText(pcharacteristic, reqobj, teamId);
          }
          else
            if(action == "setIcons"){
              setIconsreq(pcharacteristic, reqobj, teamId);
            }
            else
              if(action == "scroll"){
                scrollreq(pcharacteristic, reqobj, teamId);
              }
      }
    }
};     
void setup() {
  // Start serial communication 
  Serial.begin(115200);

  // BEGIN DON'T CHANGE
  // Create the BLE Device
  BLEDevice::init(bleServerName);

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  // Set server callbacks
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *bleService = pServer->createService(SERVICE_UUID);

  // Create BLE characteristics and descriptors
  bleService->addCharacteristic(&characteristic);  
  characteristic.addDescriptor(characteristicDescriptor);

  // Set characteristic callbacks
  characteristic.setCallbacks(new CharacteristicsCallbacks());

  // Start the service
  bleService->start();
  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
  // END DON'T CHANGE
}

void loop() {
  if (isScrolling) {
    scrollText();
  }
}
