#include <SPI.h>
#include <UIPEthernet.h> // for the module ENC28J60 enable this line. You have to install the library UIPEthernet first
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ArduinoJson.h> //Version 5.13.2 olmalı
#include <ArduinoHA.h>

// #define ARDUINOHA_DEBUG

// #define BROKER_ADDR IPAddress(192, 168, 1, 200)
const char *mqtt_server = "192.168.1.200";
const char *mqtt_username = "mqttuser";
const char *mqtt_password = "12345";
const int mqtt_port = 1883;

// Ethernet Tanımları
//---------------------------------------
byte mac[] = {0xDE, 0xED, 0xBA, 0x6E, 0xFE, 0xEB};
IPAddress ip(192, 168, 1, 222); // TODO: Add the IP address
EthernetClient client;

HADevice device(mac, sizeof(mac));
// HADevice device("ArduinoMega01");
HAMqtt mqtt(client, device, 40);

//---------------------------------------
unsigned long lastMsg = 0;  // millis kontrol tanımı
unsigned long lastMsg2 = 0; // millis kontrol tanımı
unsigned long lastMsg3 = 0; // millis kontrol tanımı
unsigned long lastMsg4 = 0; // millis kontrol tanımı
unsigned long lastMsg5 = 0; // millis kontrol tanımı
byte sayac = 0;

// SOL Mutfak tarafı tek kablo//------------------------
#define mutfak_isik_pin 49
#define kombi_role_pin 47
#define mutfak_led_isik_pin 45 // not
#define mutfak_pir_pin 43
#define koridor_pir1_pin 41
//-------------------------------------------

/// SAĞ Taraf-----------------------------------
#define koridor_pir2_pin 48
#define oturma_odasi_pir_pin 46
#define oda_lambasi_pin 44
#define ev_sicaklik_pin 42
#define oturma_odasi_priz_pin 40 // YATAK ODASINIDA KONTROL EDECEK
//----------------------------

// SOL MEGA ETRAFI-------
#define kapi_durum_pin 39
#define banyo_sicaklik_pin 37
#define banyo_havalandirma_pin 35
#define titresim_sensor_pin 33
#define koridor_led_pin 31
#define oda_led_pin 29

// SAĞ Arduino Yanı----------------------------
#define modem_on_off_pin 38 // Varsayılan açık durmalı
#define elektrik_onOff_pin 36
#define esp32_alarm_on_off_pin 34
#define alarm_siren_pin 32
#define rasberry_on_off_pin 30 // Varsayılan açık durmalı

#define reset_pin 3 // Bu pin Megadaki RESET pinine bağlanacak.
const int akim_sensor_pin = 0;
#define yatak_odasi_pir_pin 15
#define cocuk_odasi_pir_pin 16

//-----------------------------------------

#define PIN_ON LOW
#define PIN_OFF HIGH

// MQTT Topic Tanımları
//---------------------------------------
const char *kombi_role_topic = "aha/deedbafefeeb/kombi/stat_t";
const char *kapi_durum_topic = "sensor/kapi_durum";
const char *alarm_durum_topic = "alarmo/state"; //
const char *alarmed_home = "armed_home";
const char *alarmed_away = "armed_away";
const char *alarm_disabled = "disarmed";

const char *LIGHT_ON = "ON";
const char *LIGHT_OFF = "OFF";
const char *ROLE_ON = "ON";
const char *ROLE_OFF = "OFF";

// Sıcaklık Ve Nem Kod Tanımları ve değişkenleri
//---------------------------------------
#define DHTTYPE DHT11
DHT dht_ev_ici(ev_sicaklik_pin, DHTTYPE);
float temp;
float hum;
//---------------------------------------
DHT dht_banyo(banyo_sicaklik_pin, DHTTYPE);
float temp_banyo;
float hum_banyo;

// PIR TANIMLARI VE DEĞİŞKENLERİ
//---------------------------------------
bool koridor_pir1_durum = false; // Pırların başlangıç durumu: hareket_yok
bool koridor_pir2_durum = false; // Pırların başlangıç durumu: hareket_yok
bool mutfak_pir_durum = false;   // Pırların başlangıç durumu: hareket_yok
bool yatak_odasi_pir_durum = false;
bool oturma_odasi_pir_durum = false;
bool cocuk_odasi_pir_durum = false;
bool elektrik_onOff_durum = true;
bool esp32_alarm_on_off_durum = false;
bool modem_on_off_durum = true;
//---------------------------------------

// HALL SENSOR TANIMLARI VE DEĞİŞKENİ
//---------------------------------------
bool kapi_durum = false;
// int _kapi_durum_value = 0;

// TITRESIM SENSOR TANIMLARI VE DEĞİŞKENİ
//---------------------------------------
bool titresim_durum = false;
bool alarm_durum = false;

// mqtt den gelen mesajları alma Tanımları
//---------------------------------------
const char *Topic;
bool Rflag = false;

String message;
//---------------------------------------
//---------------------------------------
// AKIM ÖLÇER TANIMLARI
int voltaj_min_degeri = 1250;
int mVperAmp = 66; // 185 5A MODÜL İÇİN , 100 20A MODÜL İÇİN VE 66  30A MODÜL İÇİN
int RawValue = 0;
int ACSoffset = 2500;
double voltaj = 0; // VOLT HESABI
double amper = 0;  // AMPER HESABI

void akim_volt_olcer()
{
  RawValue = analogRead(akim_sensor_pin);    // MODUL ANALOG DEĞERI OKUNUYOR
  voltaj = (RawValue / 1024.0) * 5000;       // VOLT HESABI YAPILIYOR
  amper = ((voltaj - ACSoffset) / mVperAmp); // AKIM HESAPLA
  Serial.print("=>>>Voltaj: ");
  Serial.print(voltaj);
  Serial.print(" =>Amper: ");
  Serial.println(amper);
  elektrik_onOff_durum = digitalRead(elektrik_onOff_pin);
}
int internet_baglanti_durum = 0;

/* JSON Örnek
StaticJsonBuffer<128> jsonBuffer;
char json[] = "{\"member_id\": \"007\",\"member_name\":\"George Lathenby\",\"roles\":[1,3,9,11]}";
{
  "member_id": "007",
  "member_name": "George Lathenby",
  "roles": [
    1,
    3,
    9,
    11
  ]
}
*/

//================================================ setup

/*
void publishData(float p_temperature, float p_humidity) {
  // create a JSON object
  // doc : https://github.com/bblanchon/ArduinoJson/wiki/API%20Reference
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  // INFO: the data must be converted into a string; a problem occurs when using floats...
  root["temperature"] = (String)p_temperature;
  root["humidity"] = (String)p_humidity;
  root.prettyPrintTo(Serial);
  Serial.println("");

     {
        "temperature": "23.20" ,
        "humidity": "43.70"
     }

  char data[200];
  root.printTo(data, root.measureLength() + 1);
  mqtt.publish(dh11_topic, data, true);
  yield();
  //data => {"temperature":"30", "humidity":"20"}
}
*/

void onMessage(const char *topic, const uint8_t *payload, uint16_t length)
{

  for (int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }
  // message = messageTemp;
  //  İşlemler
  Serial.print("Topic: ");
  Serial.print(topic);
  Serial.print(" =>Message: ");
  Serial.println(message);
  //---------------

  if (strcmp(topic, alarm_durum_topic) == 0)
  {
    if (message != alarm_disabled)
    {
      Serial.print("Alarm Kuruldu.=> Alarm Durum: ");
      alarm_durum = 1;
      Serial.println(alarm_durum);
    }
    else if (message == alarm_disabled)
    {
      Serial.println("Alarm Kapatıldı.");
      alarm_durum = 0;
    }
  }
  if (strcmp(topic, kapi_durum_topic) == 0)
  {
    kapi_durum = payload;
  }
  message = "";
}

void onConnected()
{
  // mqtt.subscribe(alarm_siren_topic);
  mqtt.subscribe(alarm_durum_topic);
}
HABinarySensor mutfak_pir("mutfak_pir");
HABinarySensor koridor_pir1("koridor_pir1");
HABinarySensor koridor_pir2("koridor_pir2");
HABinarySensor yatak_odasi_pir("yatak_odasi_pir");
HABinarySensor oturma_odasi_pir("oturma_odasi_pir");
HABinarySensor cocuk_odasi_pir("cocuk_odasi_pir");

HABinarySensor kapi_sensor("kapi_sensor");
HABinarySensor titresim_sensor("titresim_sensor"); // alarm kutusu içindeki titreşim sensörü

HASensor temperature_home("sicaklik_ev");
HASensor humanity_home("nem_ev");

HASensor temperature_banyo("sicaklik_banyo");
HASensor humanity_banyo("nem_banyo");

HABinarySensor elektrik_onOff("elektrik_onOff");
HASensor akim_olcer_sensor("akim_olcer");
HASensor voltaj_olcer_sensor("voltaj_olcer");
HASwitch modem_on_off("modem_on_off");

HASwitch kombi("kombi");
HALight mutfak_isik("mutfak_isik");
HALight ana_oda_isik("ana_oda_isik");
HALight koridor_led("koridor_led");
HALight oda_led("oda_led");
HASwitch alarm_siren("alarm_siren");
HASwitch banyo_havalandirma("banyo_havalandirma");
HASwitch oturma_odasi_priz("oturma_odasi_priz");

void alarm_isik_uyarilari(bool uyari_tip)
{
  if (uyari_tip == 1)
  {
    for (size_t i = 0; i < 5; i++)
    {
      Serial.println("Işıklar Açılıp Kapatılıyor. Alarm Tetiklendi.");
      digitalWrite(mutfak_isik_pin, PIN_OFF);
      digitalWrite(koridor_led_pin, PIN_OFF);
      digitalWrite(oda_lambasi_pin, PIN_OFF);
      delay(2000);
      digitalWrite(mutfak_isik_pin, PIN_ON);
      digitalWrite(koridor_led_pin, PIN_ON);
      digitalWrite(oda_lambasi_pin, PIN_ON);
    }
  }
  if (uyari_tip == 0)
    Serial.println("Alarm Kapatıldı Uyarı Işıkları....");
  for (size_t i = 0; i < 2; i++)
  {
    digitalWrite(mutfak_isik_pin, PIN_OFF);
    digitalWrite(koridor_led_pin, PIN_OFF);
    digitalWrite(oda_lambasi_pin, PIN_OFF);
    delay(1000);
    digitalWrite(mutfak_isik_pin, PIN_OFF);
    digitalWrite(koridor_led_pin, PIN_OFF);
    digitalWrite(oda_lambasi_pin, PIN_OFF);
  }
}

void onSwitchCommand(bool state, HASwitch *sender)
{
  if (sender == &kombi)
  {
    digitalWrite(kombi_role_pin, (state ? PIN_ON : PIN_OFF));
    Serial.print("Kombi Durum: ");
    Serial.println(state);
    sender->setState(state);
  }
  if (sender == &alarm_siren)
  {
    digitalWrite(alarm_siren_pin, (state ? PIN_ON : PIN_OFF));
    Serial.print("Alarm Durum: ");
    Serial.println(state);
    alarm_isik_uyarilari(state ? 1 : 0);
    sender->setState(state);
  }
  if (sender == &banyo_havalandirma)
  {
    digitalWrite(banyo_havalandirma_pin, (state ? PIN_ON : PIN_OFF));
    Serial.print("Banyo Havalandırma Durum: ");
    Serial.println(state);
    sender->setState(state);
  }
  if (sender == &oturma_odasi_priz)
  {
    digitalWrite(oturma_odasi_priz_pin, (state ? PIN_ON : PIN_OFF));
    Serial.print("Oturma Odasi Priz : ");
    Serial.println(state);
    sender->setState(state);
  }
  if (sender == &modem_on_off)
  {
    digitalWrite(modem_on_off_pin, (state ? PIN_ON : PIN_OFF));
    if (state == 1)
    {
      modem_on_off_durum = true;
    }
    else
    {
      modem_on_off_durum = false;
    }
    sender->setState(state);
  }

  // report state back to the Home Assistant
}

void onLightCommand(bool state, HALight *sender)
{
  if (sender == &mutfak_isik)
  {
    digitalWrite(mutfak_isik_pin, (state ? PIN_ON : PIN_OFF));
    Serial.print("mutfak isik Durum: ");
    Serial.println(state);
    sender->setState(state);
  }
  if (sender == &ana_oda_isik)
  {
    digitalWrite(oda_lambasi_pin, (state ? PIN_ON : PIN_OFF));
    Serial.print("oturma odası isik Durum: ");
    Serial.println(state);
    sender->setState(state);
  }
  if (sender == &koridor_led)
  {
    digitalWrite(koridor_led_pin, (state ? PIN_ON : PIN_OFF));
    Serial.print("koridor led isik Durum: ");
    Serial.println(state);
    sender->setState(state);
  }
  // report state back to the Home Assistant
}

void setup()
{
  Serial.begin(9600);
  Serial.println("Ayarlar Başlıyor");
  Ethernet.begin(mac);
  Serial.print("My IP address: ");

  Serial.println(Ethernet.localIP());
  // const char* byte = ;
  device.setName("Arduino Mega-SmartHome");
  device.setManufacturer("Serkan Demirhan");
  device.setModel("Model 1.0.1");
  device.enableSharedAvailability();
  device.enableLastWill();

  pinMode(koridor_pir1_pin, INPUT);
  pinMode(koridor_pir2_pin, INPUT);
  pinMode(mutfak_pir_pin, INPUT);
  pinMode(titresim_sensor_pin, INPUT_PULLUP);
  pinMode(yatak_odasi_pir_pin, INPUT);
  pinMode(oturma_odasi_pir_pin, INPUT);
  pinMode(cocuk_odasi_pir_pin, INPUT);
  pinMode(esp32_alarm_on_off_pin, INPUT);

  pinMode(elektrik_onOff_pin, INPUT_PULLUP);

  digitalWrite(titresim_sensor_pin, LOW);
  digitalWrite(koridor_pir1_pin, LOW);
  digitalWrite(koridor_pir2_pin, LOW);
  digitalWrite(mutfak_pir_pin, LOW);
  digitalWrite(yatak_odasi_pir_pin, LOW);
  digitalWrite(oturma_odasi_pir_pin, LOW);
  digitalWrite(cocuk_odasi_pir_pin, LOW);
  // digitalWrite(esp32_alarm_on_off_pin, LOW);

  digitalWrite(elektrik_onOff_pin, LOW);

  digitalWrite(reset_pin, HIGH);
  delay(200);
  pinMode(reset_pin, OUTPUT);

  pinMode(kapi_durum_pin, INPUT);
  digitalWrite(kapi_durum_pin, PIN_ON);
  kapi_durum = digitalRead(kapi_durum_pin);

  titresim_durum = digitalRead(titresim_sensor_pin);

  pinMode(kombi_role_pin, OUTPUT);
  pinMode(mutfak_isik_pin, OUTPUT);
  pinMode(koridor_led_pin, OUTPUT);
  pinMode(oda_led_pin, OUTPUT);
  pinMode(oda_lambasi_pin, OUTPUT);
  pinMode(alarm_siren_pin, OUTPUT);
  pinMode(banyo_havalandirma_pin, OUTPUT);
  pinMode(oturma_odasi_priz_pin, OUTPUT);
  pinMode(modem_on_off_pin, OUTPUT);
  pinMode(rasberry_on_off_pin, OUTPUT);

  digitalWrite(kombi_role_pin, PIN_OFF);
  digitalWrite(mutfak_isik_pin, PIN_OFF);
  digitalWrite(koridor_led_pin, PIN_OFF);
  digitalWrite(oda_led_pin, PIN_OFF);
  digitalWrite(oda_lambasi_pin, PIN_OFF);
  digitalWrite(alarm_siren_pin, PIN_OFF);
  digitalWrite(banyo_havalandirma_pin, PIN_OFF);
  digitalWrite(oturma_odasi_priz_pin, PIN_OFF);
  digitalWrite(modem_on_off_pin, PIN_ON);
  digitalWrite(rasberry_on_off_pin, PIN_ON);

  // BINARY SENSORLER
  //---------------------------------------------------------
  mutfak_pir.setCurrentState(mutfak_pir_durum);
  mutfak_pir.setName("Mutfak Pır");
  mutfak_pir.setDeviceClass("motion"); // mutfak_pir.setIcon("mdi:motion-sensor");

  koridor_pir2.setCurrentState(koridor_pir2_durum);
  koridor_pir2.setName("Koridor Pır 2");
  koridor_pir2.setDeviceClass("motion");
  // koridor_pir2.setIcon("mdi:motion-sensor");

  koridor_pir1.setCurrentState(koridor_pir1_durum);
  koridor_pir1.setName("Koridor Pır 1");
  koridor_pir1.setDeviceClass("motion");

  yatak_odasi_pir.setCurrentState(yatak_odasi_pir_durum);
  yatak_odasi_pir.setName("Yatak Odasi Pir");
  yatak_odasi_pir.setDeviceClass("motion");

  cocuk_odasi_pir.setCurrentState(cocuk_odasi_pir_durum);
  cocuk_odasi_pir.setName("Çocuk Odası Pır");
  cocuk_odasi_pir.setDeviceClass("motion");

  oturma_odasi_pir.setCurrentState(oturma_odasi_pir_durum);
  oturma_odasi_pir.setName("Oturma Odasi Pir");
  oturma_odasi_pir.setDeviceClass("motion");

  kapi_sensor.setCurrentState(kapi_durum);
  kapi_sensor.setName("Kapı Durum");
  kapi_sensor.setDeviceClass("door");

  titresim_sensor.setCurrentState(titresim_durum);
  titresim_sensor.setName("Alarm Sabotaj");
  titresim_sensor.setDeviceClass("vibration");

  elektrik_onOff.setCurrentState(elektrik_onOff_durum);
  elektrik_onOff.setName("Elektrik");
  elektrik_onOff.setDeviceClass("power");
  //---------------------------------------------------------
  // Switch ler
  //---------------------------------------------------------
  kombi.setName("Kombi Durum");
  kombi.setDeviceClass("switch");
  // kombi.setRetain(true);
  kombi.isAvailabilityConfigured();
  kombi.setIcon("mdi:water-boiler");

  alarm_siren.setName("Siren Durum");
  alarm_siren.setDeviceClass("switch");
  // alarm_siren.setRetain(true);
  alarm_siren.isAvailabilityConfigured();
  alarm_siren.setIcon("mdi:alarm-light");

  koridor_led.setName("Koridor Led ");
  // koridor_led.setRetain(true);
  koridor_led.isAvailabilityConfigured();

  oda_led.setName("Oda Led ");
  // koridor_led.setRetain(true);
  oda_led.isAvailabilityConfigured();

  mutfak_isik.setName("Mutfak Işık");
  // mutfak_isik.setRetain(true);
  mutfak_isik.isAvailabilityConfigured();

  ana_oda_isik.setName("Oturma Odası Işık");
  // ana_oda_isik.setRetain(true);
  ana_oda_isik.isAvailabilityConfigured();

  banyo_havalandirma.setName("Banyo Havalandırma");
  banyo_havalandirma.setDeviceClass("switch");
  // banyo_havalandirma.setRetain(true);
  banyo_havalandirma.setIcon("mdi:fan");

  oturma_odasi_priz.setName("Oturma Odası Priz");
  oturma_odasi_priz.setDeviceClass("switch");
  // oturma_odasi_priz.setRetain(true);

  modem_on_off.setName("Modem ve Raspberry");
  modem_on_off.setDeviceClass("switch");
  // modem_on_off.setRetain(true);

  //---------------------------------------------------------
  // SENSORLER
  //---------------------------------------------------------
  temperature_home.setName("Ev Sıcaklık Durumu");
  temperature_home.setDeviceClass("temperature");
  temperature_home.setUnitOfMeasurement("°C");

  humanity_home.setName("Ev Nem Durumu");
  humanity_home.setDeviceClass("humidity");
  humanity_home.setUnitOfMeasurement("%");

  temperature_banyo.setName("Banyo Dışı Sıcaklık Durumu");
  temperature_banyo.setDeviceClass("temperature");
  temperature_banyo.setUnitOfMeasurement("°C");

  humanity_banyo.setName("Banya Dışı Nem Durumu");
  humanity_banyo.setDeviceClass("humidity");
  humanity_banyo.setUnitOfMeasurement("%");

  akim_olcer_sensor.setName("Akü Akım");
  akim_olcer_sensor.setDeviceClass("current");
  akim_olcer_sensor.setUnitOfMeasurement("A");

  voltaj_olcer_sensor.setName("Akü Voltaj");
  voltaj_olcer_sensor.setDeviceClass("voltage");
  voltaj_olcer_sensor.setUnitOfMeasurement("mV");

  // Role durumlarını Kontrol Etme
  mqtt.onMessage(onMessage);
  mqtt.onConnected(onConnected);

  kombi.onCommand(onSwitchCommand);
  mutfak_isik.onStateCommand(onLightCommand);
  ana_oda_isik.onStateCommand(onLightCommand);
  koridor_led.onStateCommand(onLightCommand);
  oda_led.onStateCommand(onLightCommand);
  alarm_siren.onCommand(onSwitchCommand);
  banyo_havalandirma.onCommand(onSwitchCommand);
  oturma_odasi_priz.onCommand(onSwitchCommand);
  modem_on_off.onCommand(onSwitchCommand);

  dht_ev_ici.begin();
  dht_banyo.begin();
  mqtt.begin(mqtt_server, mqtt_username, mqtt_password);
  // device.publishAvailability();
  // mqtt.begin(BROKER_ADDR);
  Serial.println("Setup Bitti");
}
void dbug()
{
  Serial.print("Mutfak Pir: ");
  Serial.print(mutfak_pir_durum);
  Serial.print(" |Koridor Pir1: ");
  Serial.print(koridor_pir1_durum);
  Serial.print(" |Koridor Pir2: ");
  Serial.print(koridor_pir2_durum);
  Serial.print(" |Kapi : ");
  Serial.print(kapi_durum);
  Serial.print(" |YatakOdasi: ");
  Serial.print(yatak_odasi_pir_durum);
  Serial.print(" |Titreşim: ");
  Serial.print(titresim_durum);
  Serial.print(" |CocukOdasi: ");
  Serial.print(cocuk_odasi_pir_durum);
  Serial.print(" |OturmaOdasi: ");
  Serial.print(oturma_odasi_pir_durum);
  Serial.print(" |ModemOnOFF: ");
  Serial.print(modem_on_off_durum);
  Serial.print(" |ESP Alarm_Durum:");
  Serial.print(esp32_alarm_on_off_durum);
  Serial.print(" |Elektrik Durum:  ");
  Serial.print(elektrik_onOff_durum);
  Serial.print(" |intnt Durum: ");
  Serial.print(internet_baglanti_durum);
  Serial.print(" |Alarm Durum: ");
  Serial.println(alarm_durum);
  Serial.print("Reset Pin Durum: ");
  Serial.println(digitalRead(reset_pin));
}
//================================================ loop
void reconnect()
{
  // MQTT sunucusuna bağlan
  // Ethernet.begin(mac);
  Serial.print("Reconnect=> mqtt.isconnected: ");
  Serial.println(mqtt.isConnected());

  if (Ethernet.linkStatus() != 2 && mqtt.isConnected() != 1) //! mqtt.isConnected()
  {
    // akim_volt_olcer();
    Serial.print("MQTT sunucusuna bağlanılıyor... IP: ");
    Serial.println(Ethernet.localIP());
    // mqtt.onConnected(onConnected);
    mqtt.begin(mqtt_server, mqtt_username, mqtt_password);
    // mqtt.disconnect();
    delay(1000);
    Serial.print("Linkstatus: ");
    Serial.print(internet_baglanti_durum);
    Serial.print("   Client status: ");
    Serial.println(mqtt.isConnected());
  }
}

//================================================
void alarmMod()
{
  if (internet_baglanti_durum != 1)
  {
    if (alarm_durum) // alarm aktif ve internet bağlantısı yok ise
    {
      if (digitalRead(alarm_siren_pin) == PIN_OFF || mutfak_pir_durum || koridor_pir1_durum || koridor_pir2_durum || oturma_odasi_pir_durum || yatak_odasi_pir_durum)
      {
        // Serialde pır durumlarını gösterir.
        delay(3000);
        if (mutfak_pir_durum || koridor_pir1_durum || koridor_pir2_durum || oturma_odasi_pir_durum || yatak_odasi_pir_durum)
        {
          Serial.println("Alarm ON , İntenet bağlantı yok, sensorlerde haraket var. Sireniniii çalıyorum.");
          digitalWrite(alarm_siren_pin, PIN_ON);
          alarm_isik_uyarilari(1);
        }
      }
    }
    else if (!alarm_durum && digitalRead(alarm_siren_pin) == PIN_ON)
    {
      Serial.println("İnternet yok, ESP'den Alarm kapa geldi. Siren Kapatılıyor.");
      digitalWrite(alarm_siren_pin, PIN_OFF);
      alarm_isik_uyarilari(0);
    }
  }
}

void loop()
{
  unsigned long now = millis();

  // Elektrik yokken, Akü voltajı "voltaj_min_degeri" altına düştüğünde modem ve rasperry pi kapatılır.
  //--------------------------------------------------
  /*
    if (voltaj < voltaj_min_degeri && !elektrik_onOff_durum && modem_on_off_durum)
    {
      Serial.println("Elektrik yok ve Akü zayıf. Modem kapatılıyor...");
      modem_on_off.setCurrentState(false);
      delay(10000);
      digitalWrite(modem_on_off_pin, PIN_OFF);
      digitalWrite(rasberry_on_off_pin, PIN_OFF);
      modem_on_off_durum = false;
    }
  */
  //------------------------------------------------

  if (now - lastMsg4 > 10000)
  {

    Serial.print("Bağlantı Kontrol => ");
    Serial.println(Ethernet.linkStatus());
    Serial.print("mqtt.isconnected: ");
    Serial.println(mqtt.isConnected());
    if (elektrik_onOff_durum && modem_on_off_durum && (internet_baglanti_durum == 1 || mqtt.isConnected() == 0))
    {
      Serial.println("Yeniden bağlanılıyor.");
      reconnect();
    }
    if (internet_baglanti_durum == 2)
    {
      Serial.println("İnternet kablosu takılı değil");
    }

    if (internet_baglanti_durum == 2 && mqtt.isConnected() == 1)
    {
      Serial.println("Kablo takılı değil mqtt baglantı KOPARILIYORR...");
      mqtt.disconnect();
    }

    lastMsg4 = now;
  }
  if (internet_baglanti_durum == 1 && mqtt.isConnected() == 0)
  {

    if (now - lastMsg5 > 1000)
    {
      Serial.print("Reset Pin Durum: ");
      Serial.println(digitalRead(reset_pin));
      Serial.println("Bağlanılamıyor. YENİDEN BAŞLATACAĞIM MEGA yı");
      Serial.print("Sayac Durum:");
      Serial.println(sayac);
      if (sayac == 10)
      {
        Serial.print("Sayac: ");
        Serial.println(sayac);
        digitalWrite(reset_pin, LOW);
        delay(100);
        Serial.print("Reset Pin Durum: ");
        Serial.println(digitalRead(reset_pin));
      }
      sayac = sayac + 1;
      lastMsg5 = now;
    }
  }
  if (now - lastMsg3 > 50)
  {
    Ethernet.maintain();
    if (internet_baglanti_durum == 1)
    {
      // Serial.println("Mqtt loop");
      mqtt.loop();
    }
    lastMsg3 = now;
  }

  // Sıcaklık ve Nem Kontrol BAŞ.
  if (now - lastMsg > 60000)
  {
    // akim_volt_olcer();
    temp = dht_ev_ici.readTemperature();
    hum = dht_ev_ici.readHumidity();
    Serial.print("=>>>Ev Sıcaklık : ");
    Serial.print(temp);
    Serial.print(" Ev Nem: ");
    Serial.println(hum);

    temp_banyo = dht_banyo.readTemperature();
    hum_banyo = dht_banyo.readHumidity();
    Serial.print("=>>>Banyo Sıcaklık : ");
    Serial.print(temp_banyo);
    Serial.print(" Banyo Nem: ");
    Serial.println(hum_banyo);
    if (modem_on_off_durum && internet_baglanti_durum == 1)
    {
      temperature_home.setValue(String(temp).c_str());
      humanity_home.setValue(String(hum).c_str());
      temperature_banyo.setValue(String(temp_banyo).c_str());
      humanity_banyo.setValue(String(hum_banyo).c_str());
      akim_olcer_sensor.setValue(String(amper).c_str());
      voltaj_olcer_sensor.setValue(String(voltaj).c_str());
    }
    // publishData(_temp, _hum);
    lastMsg = now;
  }
  // Sıcaklık ve Nem Kontrol SON

  // HAREKET SENSORLERI KODLARI
  //----------- PIR KOD BAŞ. -----------------
  if (now - lastMsg2 > 1000)
  {

    esp32_alarm_on_off_durum = digitalRead(esp32_alarm_on_off_pin);
    mutfak_pir_durum = digitalRead(mutfak_pir_pin);
    koridor_pir1_durum = digitalRead(koridor_pir1_pin);
    koridor_pir2_durum = digitalRead(koridor_pir2_pin);
    kapi_durum = digitalRead(kapi_durum_pin);
    titresim_durum = digitalRead(titresim_sensor_pin);
    yatak_odasi_pir_durum = digitalRead(yatak_odasi_pir_pin);
    cocuk_odasi_pir_durum = digitalRead(cocuk_odasi_pir_pin);
    oturma_odasi_pir_durum = digitalRead(oturma_odasi_pir_pin);
    elektrik_onOff_durum = digitalRead(elektrik_onOff_pin);
    internet_baglanti_durum = Ethernet.linkStatus();
    // modem_on_off_durum = false;

    // dbug();
    // akim_volt_olcer();

    // Serial.print("mqtt.isconnected: ");
    // Serial.println(mqtt.isConnected());
    if (!modem_on_off_durum && elektrik_onOff_durum)
    {
      Serial.println("Elektrik geldi, rasperyy ve modem kapalıdan => Açık duruma Getirildi!!!");
      digitalWrite(modem_on_off_pin, PIN_ON);
      digitalWrite(rasberry_on_off_pin, PIN_ON);
      modem_on_off_durum = true;
    }
    if (!alarm_durum && esp32_alarm_on_off_durum == 1 && (!modem_on_off_durum || internet_baglanti_durum != 1))
    {
      Serial.println("Modem Kapalı VEYA İNTERNET YOK ve ESP'den Alarm Aç geldi. Alarm Açılıyor.");
      alarm_durum = true;
    }
    else if (alarm_durum && esp32_alarm_on_off_durum == 0 && (!modem_on_off_durum || internet_baglanti_durum != 1))
    {
      Serial.println("Modem Kapalı ve ESP'den Alarm Kapa Geldi. Alarm Alarm Kapatılıyor.");
      alarm_durum = false;
    }
    alarmMod(); // İnternet olmadığı durumda bu alarmı kontrol ediyoruz.
    if (modem_on_off_durum == true && internet_baglanti_durum == 1 && mqtt.isConnected() == 1)
    {
      mutfak_pir.setState(mutfak_pir_durum);
      koridor_pir1.setState(koridor_pir1_durum);
      koridor_pir2.setState(koridor_pir2_durum);
      kapi_sensor.setState(kapi_durum);
      titresim_sensor.setState(titresim_durum);
      yatak_odasi_pir.setState(yatak_odasi_pir_durum);
      cocuk_odasi_pir.setState(cocuk_odasi_pir_durum);
      oturma_odasi_pir.setState(oturma_odasi_pir_durum);
      elektrik_onOff.setState(elektrik_onOff_durum);
      modem_on_off.setState(modem_on_off_durum);
      sayac = 0;
    }
    lastMsg2 = now;
  }
}
