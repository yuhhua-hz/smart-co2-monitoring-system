/**
 * @file    main.ino
 * @brief   Sensor CO2 con comunicación inalámbrica MQTT
 * 
 * Sistema de medición de CO2, temperatura y humedad con
 * comunicación MQTT y configuración mediante portal web.
 *
 * @see https://github.com/WifWaf/MH-Z19 (Sensor CO2)
 * @see https://github.com/adafruit/Adafruit_BME280_Library (Sensor BME280)
 * @see https://github.com/knolleary/pubsubclient (PubSubClient)
 * @see https://github.com/prampec/IotWebConf (Portal configuración)
 *  
 */

//===============================================================================

#include <IotWebConf.h>                                  // Libreria para el portal externo de configuraciones
#include <IotWebConfUsing.h>                             
#include <ESP8266WiFi.h>                                 // Libreria WiFi
#include <PubSubClient.h>                                // Libreria para el cliente MQTT
#include <MHZ19.h>                                       // Libreria del sensor de CO2
#include <SoftwareSerial.h>                              // Libraria para permitir comunicación serie en otros pines
#include <Ticker.h>                                      // Libreria para llamar funciones cada cierto tiempo
#include <Wire.h>                                        // Libreria I2C                                       
#include <Adafruit_BME280.h>                             // Libreria del sensor BME280
#include <Adafruit_Sensor.h> 

//===============================================================================

/**
 * @def STRING_LEN
 * @brief Define la longitud máxima de las cadenas de texto utilizadas
 */
#define STRING_LEN 128                                   // Longitud máxima de cadenas
#define CONFIG_VERSION "Tfg-2021-yhz"                    // Versión del portal de configuración

// Pines ESP12
#define SCL_ESP12 5                                      // GPIO5 - SCL
#define SDA_ESP12 4                                      // GPIO4 - SDA
#define TX_ESP12  12                                     // GPIO12 - TX
#define RX_ESP12  14                                     // GPIO14 - RX

//===============================================================================
// Declaración de los Objetos

WiFiClient espClient;
PubSubClient client(espClient);
SoftwareSerial VirtualSerial(RX_ESP12, TX_ESP12);         // Serial virtual (RX=GPIO14, TX=GPIO12)
Ticker ticker_co2;                                        // Temporizador para sensor CO2
Ticker ticker_bme;                                        // Temporizador para sensor BME280
DNSServer dnsServer;
WebServer server(80);                                     // Servidor web en puerto 80
MHZ19 sensor_CO2;                                         // Sensor de CO2
Adafruit_BME280 sensor_BME;                               // Sensor de temperatura/humedad

//===============================================================================

// Configuración del Access Point inicial 
const char AcessPoint[] = "ESP8266AP";                    // Nombre del AP
const char AP_pass[] = "TFG2021YHZ";                      // Contraseña del AP

// Parámetros personalizados para configuración MQTT
char mqtt_server[STRING_LEN];                             // Dirección del servidor
char mqtt_port[STRING_LEN];                               // Puerto
char mqtt_user[STRING_LEN];                               // Usuario
char mqtt_password[STRING_LEN];                           // Contraseña
char mqtt_Id[STRING_LEN];                                 // ID del cliente
char mqtt_prefix[STRING_LEN];                             // Prefijo para topics (tfg/2021/)

bool needMqttConnect = false;                             // Flag para reconexión MQTT

// Variables para lecturas de sensores
unsigned int lectura_CO2;                                 // Valor CO2 (ppm)
float lectura_temp, lectura_hum;                          // Valores temperatura y humedad

// Variables para conversión de valores a cadenas
char co2String[8];                                        // Cadena para valor CO2
char tempString[8];                                       // Cadena para temperatura
char humString[8];                                        // Cadena para humedad

// Sufijos para topics MQTT
char* sufijo_Control_co2 = "control/sensor/co2";          // Control sensor CO2
char* sufijo_Datos_co2 = "datos/sensor/co2";              // Datos sensor CO2
char* sufijo_Control_bme = "control/sensor/bme";          // Control sensor BME280
char* sufijo_temp_bme = "datos/sensor/bme/temperatura";   // Datos temperatura
char* sufijo_hum_bme = "datos/sensor/bme/humedad";        // Datos humedad

// Variables para almacenar los topics completos
char* topic_CO2_1;                                        // Topic control CO2
char* topic_CO2_2;                                        // Topic datos CO2
char* topic_bme_1;                                        // Topic control BME280
char* topic_bme_2;                                        // Topic temperatura
char* topic_bme_3;                                        // Topic humedad

// Variables temporales para construcción de topics
char tmp1[STRING_LEN];                                    // Temporal para topic 1
char tmp2[STRING_LEN];                                    // Temporal para topic 2
char tmp3[STRING_LEN];                                    // Temporal para topic 3
char tmp4[STRING_LEN];                                    // Temporal para topic 4
char tmp5[STRING_LEN];                                    // Temporal para topic 5

IotWebConf iotWebConf(AcessPoint, &dnsServer, &server, AP_pass, CONFIG_VERSION);

// Grupo de parámetros para configuración MQTT
IotWebConfParameterGroup group1 = IotWebConfParameterGroup("group1", "Parámetros MQTT");
IotWebConfTextParameter mqttServer = IotWebConfTextParameter("Servidor MQTT", "mqttServer", mqtt_server, STRING_LEN);
IotWebConfTextParameter mqttPort = IotWebConfTextParameter("Puerto MQTT", "mqttPort", mqtt_port, STRING_LEN);
IotWebConfTextParameter mqttId = IotWebConfTextParameter ("MQTT ID", "mqttId", mqtt_Id, STRING_LEN);
IotWebConfTextParameter mqttUser = IotWebConfTextParameter("Usuario MQTT", "mqttUser", mqtt_user, STRING_LEN);
IotWebConfPasswordParameter mqttPass = IotWebConfPasswordParameter("Contraseña MQTT", "mqttPass", mqtt_password, STRING_LEN);
IotWebConfTextParameter mqttPrefix = IotWebConfTextParameter("Prefix", "mqttPrefix", mqtt_prefix, STRING_LEN);

//===============================================================================

/**
 * @brief Gestiona la página inicial del portal de configuración
 */
void handleRoot(){

  // Let IotWebConf test and handle captive portal requests
  if (iotWebConf.handleCaptivePortal())
  {
    return;                                           // Captive portal request were already served
  }
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>Home page</title></head><body>";
  s += "<h1>Bienvenido al portal de configuraci&oacute;n</h1>";
  s += "<h2>Trabajo fin de grado. Autor: Yuhao Huang Zheng.</h2>";
  s += "<h3>&Uacute;ltimas configuraciones MQTT: </h3>";
  s += "<ul>";
  s += "<li>Servidor MQTT: ";
  s += mqtt_server;
  s += "<li>Puerto MQTT: ";
  s += mqtt_port;
  s += "<li>ID dispositivo: ";
  s += mqtt_Id;
  s += "</ul>";
  s += "Ir a <a href='config'>configure page</a> para cambiar la configuraci&oacute;n.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);                   // Enviar página HTML
}

//===============================================================================

/**
 * @brief Callback llamado cuando se guardan las configuraciones
 */
void configSaved(){
  Serial.println("Configuration was updated.");       // Notificar actualización
}

//===============================================================================

/**
 * @brief Valida el formulario de configuración
 * 
 * @return true  Si el formulario es válido
 */
bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper){
  Serial.println("Validating form.");                 // Notificar validación
  bool valid = true;
  return valid;                                       // Formulario siempre válido
}

//===============================================================================

/**
 * @brief Callback llamado cuando WiFi se conecta
 */
void wifiConnected(){
  needMqttConnect = true;                             // Activar flag de reconexión MQTT
}

//===============================================================================

/**
 * @brief Establece la conexión con el servidor MQTT
 * 
 * @return true  Si la conexión fue exitosa
 */
bool connectMqtt(){
  
  // Realizar la conexión MQTT
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    String MQTT_ID = (String)mqtt_Id;                 // Convertir char a string el parámetro ID
    MQTT_ID += String(random(0xffff), HEX);           // ID cliente = ID_portal + número HEX aleatorio
    
    if (client.connect(MQTT_ID.c_str(), mqtt_user, mqtt_password)) {  // Parámetros: Id, usuario, contraseña
      Serial.println("Conectado con el servidor MQTT");
      
      // Construir los topics con prefijo y sufijo
      strcpy(tmp1, mqtt_prefix);                      // Copiar el prefijo en variables temporales
      strcpy(tmp2, mqtt_prefix);                  
      strcpy(tmp3, mqtt_prefix);
      strcpy(tmp4, mqtt_prefix);
      strcpy(tmp5, mqtt_prefix);
      
      // Construir topics completos
      topic_CO2_1 = strcat(tmp1, sufijo_Control_co2); // Topic: tfg/2021/control/sensor/co2 
      topic_CO2_2 = strcat(tmp2, sufijo_Datos_co2);   // Topic: tfg/2021/datos/sensor/co2
      topic_bme_1 = strcat(tmp3, sufijo_Control_bme); // Topic: tfg/2021/control/sensor/bme
      topic_bme_2 = strcat(tmp4, sufijo_temp_bme);    // Topic: tfg/2021/datos/sensor/bme/temperatura
      topic_bme_3 = strcat(tmp5, sufijo_hum_bme);     // Topic: tfg/2021/datos/sensor/bme/humedad

      // Suscribirse a los topics de control
      client.subscribe(topic_CO2_1);                  // Suscrito a tfg/2021/control/sensor/co2
      client.subscribe(topic_bme_1);                  // Suscrito a tfg/2021/control/sensor/bme
    } else {
      Serial.print("Fallido, rc = ");                 // Error en conexión
      Serial.print(client.state());                   // Ver códigos en PubSubClient API
      Serial.println(" Intentar de nuevo en 2s");
      delay(2000);                                    // Esperar 2s antes de reintentar
    }
  }
   
  return true;                                        // Conexión establecida
}

//===============================================================================

/**
 * @brief Configura el portal web con parámetros personalizados
 */
void addConfig(){
  
  // Añadir parámetros al grupo
  group1.addItem(&mqttServer);
  group1.addItem(&mqttPort);
  group1.addItem(&mqttId);
  group1.addItem(&mqttUser);
  group1.addItem(&mqttPass);
  group1.addItem(&mqttPrefix);

  iotWebConf.addParameterGroup(&group1);              // Añadir grupo al portal
  
  // Configurar callbacks
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  
  // Inicializar configuraciones
  bool validConfig = iotWebConf.init();
  if (!validConfig){                                  // Si no hay config válida, inicializar vacíos
    mqtt_server[0] = '\0';
    mqtt_port[0] = '\0';
    mqtt_user[0] = '\0';
    mqtt_password[0] = '\0';
    mqtt_Id[0] = '\0';
    mqtt_prefix[0] = '\0';
  }

  // Configurar rutas del servidor web
  server.on("/", handleRoot);                              // Página principal
  server.on("/config", []{ iotWebConf.handleConfig(); });  // Página config
  server.onNotFound([](){ iotWebConf.handleNotFound(); }); // 404
}

//===============================================================================

/**
 * @brief Lee el sensor CO2 y publica el valor
 */
void leer_co2(){
  lectura_CO2 = sensor_CO2.getCO2();                  // Obtener valor CO2
  sprintf(co2String, "%d", lectura_CO2);              // Convertir entero a cadena
  client.publish(topic_CO2_2, co2String);             // Publicar en MQTT
}

//===============================================================================

/**
 * @brief Lee temperatura y humedad del BME280
 */
void leer_bme(){
  lectura_temp = sensor_BME.readTemperature();        // Leer temperatura
  dtostrf(lectura_temp, 1, 2, tempString);            // Convertir a cadena
  client.publish(topic_bme_2, tempString);            // Publicar temperatura
  
  lectura_hum = sensor_BME.readHumidity();            // Leer humedad
  dtostrf(lectura_hum, 1, 2, humString);              // Convertir a cadena
  client.publish(topic_bme_3, humString);             // Publicar humedad
}

//===============================================================================

/**
 * @brief Procesa mensajes de topics suscritos
 * 
 * @param topic    Topic del mensaje recibido
 * @param payload  Contenido del mensaje
 * @param length   Longitud del mensaje
 */
void callback(char* topic, byte* payload, unsigned int length){
  
  Serial.print("Mensaje llegado en el topic: ");
  Serial.println(topic);
  for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
  }
  Serial.println();
    
  // Control del sensor CO2
  if (strstr(topic, topic_CO2_1)){
    if ((char)payload[0] == '1'){                     // Si payload = 1: activar
      ticker_co2.attach(2, leer_co2);                 // Leer cada 2 segundos
    } else {                                          // Si payload = otro: desactivar
      ticker_co2.detach();                            // Detener lecturas
      client.publish(topic_CO2_2, "---");             // Limpiar valor
    }
  }

  // Control del sensor BME280
  if(strstr(topic, topic_bme_1)){
    if ((char)payload[0] == '1'){                     // Si payload = 1: activar
      ticker_bme.attach(2, leer_bme);                 // Leer cada 2 segundos
    } else {                                          // Si payload = otro: desactivar  
      ticker_bme.detach();                            // Detener lecturas
      client.publish(topic_bme_2, "---");             // Limpiar temperatura             
      client.publish(topic_bme_3, "---");             // Limpiar humedad            
    }
  }
}

//===============================================================================

/**
 * @brief Configura componentes del sistema
 */
void setup(){
  Serial.begin(115200);                               // Consola depuración
  VirtualSerial.begin(9600);                          // Serie sensor CO2
  sensor_CO2.begin(VirtualSerial);                    // Iniciar sensor CO2
  addConfig();                                        // Configurar portal web
  
  Wire.begin(SDA_ESP12, SCL_ESP12);                   // Iniciar I2C
  
  if(!sensor_BME.begin()){                            // Verificar sensor BME280
    Serial.println("Sensor BME280 no encontrado.");   // Error si no responde
    while(1);                                         // Detener ejecución
  }
  
  // Configurar cliente MQTT
  client.setServer(mqtt_server, atoi(mqtt_port));     // Servidor y puerto
  client.setCallback(callback);                       // Función callback
}

//===============================================================================

/**
 * @brief Bucle principal
 */
void loop(){
  iotWebConf.doLoop();                                // Procesar portal web
  client.loop();                                      // Procesar MQTT
  
  // Gestionar reconexión MQTT
  if (needMqttConnect){
    if (connectMqtt()){
      needMqttConnect = false;                        // Conexión establecida
    }
  }
  // Reconectar si se pierde la conexión
  else if ((iotWebConf.getState() == IOTWEBCONF_STATE_ONLINE) && 
           (!client.connected())) {
    Serial.println("MQTT reconnect");
    connectMqtt();                                    // Intentar reconexión
  }
}

//===============================================================================
