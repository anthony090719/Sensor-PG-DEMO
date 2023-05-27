#include <WiFi.h>
#include "SPIFFS.h"
#include <ESPAsyncWebServer.h>       
#include <ESPAsyncWiFiManager.h>     
#include <ArduinoJson.h>             
#include <SimpleTimer.h>             
#include "ThingSpeak.h"              
#include <FS.h>                      
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// se define tamaño de la pantalla
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// se inicia la pantalla oled
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
float t = 0;

//se definen las variables char con un valor de 99 caracteres
char limite_malo[100];
char limite_bueno[100];
char api_key[100]; 
char numero_canal[100]; 
char offset_calibracion[100];

//se utiliza para indicar si se deben guardar o no los datos de configuración. El valor inicial de esta variable es false.
bool shouldSaveConfig = false;

//Esta función se ejecutará cuando se necesite guardar los datos de configuración.
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// Este objeto se utiliza para manejar las solicitudes HTTP entrantes.
AsyncWebServer server(80);
DNSServer dns;
SimpleTimer timer;


//rutina de reinicio
void reinicio() {
  ESP.restart();
}


// se inicia la comunicacion serial a la velocidad definida el valor ya es predeterminado y es el comun utilizado
void setup() {
  Serial.begin(115200);

  //pines utilizados para comunicacion con la pantalla
  Wire.begin(23, 19);

 // se verifica si se pudo iniciar la comunicacion con la pantalla oled
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
   // se limpia pantalla, se define el tamaño, color, posicion, mensaje y se da instruccion de mostrar
   display.clearDisplay();
   display.setTextSize(1);
   display.setTextColor(WHITE);
   display.setCursor(30, 30);
   display.println("BIENVENIDOS");
   display.display();
   delay(5000); // Mostrar el mensaje de bienvenida por 5 segundos

   // se limpia pantalla, se define el tamaño, color, posicion, mensaje y se da instruccion de mostrar
   display.clearDisplay();
   display.setTextSize(1);
   display.setTextColor(WHITE);
   display.setCursor(20, 30);
   display.println("SENSOR DE CO2, UMG");
   display.display();
   delay(5000); // Mostrar el mensaje de sensor de CO2 por 5 segundos 


// esta parte del código se utiliza para cargar la configuración guardada previamente en el archivo "config.json".

  //leer la configuración de FS json
  Serial.println("mounting FS...");
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //El archivo existe, lectura y carga
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Asigne un búfer para almacenar el contenido del archivo.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(limite_malo, json["limite_malo"]); 
          strcpy(limite_bueno, json["limite_bueno"]);
          strcpy(api_key, json["api_key"]);
          strcpy(numero_canal, json["numero_canal"]);
          strcpy(offset_calibracion, json["offset_calibracion"]);
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  // fin de la lectura

  //al cargar el codigo aparecera una red wifi a la que debemos conectar la placa y asignamos los parametros que se pediran
  AsyncWiFiManagerParameter custom_limite_bueno("limite_bueno", "900", limite_bueno, 100);
  AsyncWiFiManagerParameter custom_limite_malo("limite_malo", "1500", limite_malo, 100);
  AsyncWiFiManagerParameter custom_api_key("api_key", "7Q49OISGCTDWLNAV", api_key, 100); 
  AsyncWiFiManagerParameter custom_numero_canal("numero_canal", "2061125", numero_canal, 100); 
  AsyncWiFiManagerParameter custom_offset_calibracion("offset_calibracion", "0", offset_calibracion, 100);

  //se utilizará para manejar la conexión a la red WiFi.
  AsyncWiFiManager wifiManager(&server, &dns);

  //Aquí se establece la función saveConfigCallback() como callback para guardar la configuración. Es decir cuando se llame a esta función se guardará la configuración.
  wifiManager.setSaveConfigCallback(saveConfigCallback);

// Aquí se agregan los campos personalizados 
  wifiManager.addParameter(&custom_limite_bueno);
  wifiManager.addParameter(&custom_limite_malo);
  wifiManager.addParameter(&custom_api_key);
  wifiManager.addParameter(&custom_numero_canal);
  wifiManager.addParameter(&custom_offset_calibracion);

  // Aquí se configura un temporizador que ejecutará la función reinicio() cada cierto número de horas (en este caso, cada hora)
  int horas = 1; // aqui se estableceran las horas que va estar trabajando el dispositivo para no sobrecargarlo
  timer.setInterval(horas * 3600000, reinicio);

  //conexion a wifi
  AsyncWiFiManagerParameter custom_text("<p>Seleccione la red WiFi para conectarse.</p>");
  wifiManager.addParameter(&custom_text);

  
  wifiManager.autoConnect("PROYECTO CO2"); //nombre de la red wifi para conectarse 
  Serial.println("connectado :)"); //  cuando se establece la conexión WiFi, se imprime ese mensaje
  display.clearDisplay();

  //Leer parámetros actualizados
  strcpy(limite_malo, custom_limite_malo.getValue());
  strcpy(limite_bueno, custom_limite_bueno.getValue());
  strcpy(api_key, custom_api_key.getValue());
  strcpy(numero_canal, custom_numero_canal.getValue());
  strcpy(offset_calibracion, custom_offset_calibracion.getValue());

  //guardar los parámetros personalizados 
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["limite_malo"] = limite_malo;
    json["limite_bueno"] = limite_bueno;
    json["api_key"] = api_key;
    json["numero_canal"] = numero_canal;
    json["offset_calibracion"] = offset_calibracion;


    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }
    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //guardar y finalizar
  }
  Serial.println("local ip");
  Serial.println(WiFi.localIP());
}

void loop() {
  // aqui ira el codigo para hacer funcionar el sensor y que esos datos se muestren en pantalla
  WiFiClient client;
  
  ThingSpeak.begin(client); // establecer conexion a wifi y conexion a thingspeak

  timer.run();
  int limite_malo_convertido = atoi(limite_malo);
  int limite_bueno_convertido = atoi(limite_bueno);
  int offset_calibracion_convertido = atoi(offset_calibracion);
  int offstet_mq_135 = 90; // calibracion inicial del sensor

  unsigned long numero_canal_convertido = strtoul(numero_canal, NULL, 10);

// se realiza un cálculo para obtener la concentración de CO2 en ppm, sumando el valor de offset_mq_135, que es un valor constante definido como 90 en este código
// y el valor de 'offset_calibracion_convertido', que es un valor configurable en la configuración del dispositivo. Finalmente, se almacena la concentración de CO2 calculada en la variable 't'
  int medicion = analogRead(A0); 
  int t = medicion + offstet_mq_135 + offset_calibracion_convertido;
  int icono;
  Serial.print("Medicion raw = ");
  Serial.println(medicion);
  Serial.print("Offset = ");
  Serial.println(offset_calibracion_convertido);
  Serial.print("Co2 ppm = ");
  Serial.println(t);
  String analisis;

// luego del analisis se verifica si el valor de t es bueno, malo o regular
  if (t <= limite_bueno_convertido)
  {
    Serial.println("Aire puro");
    analisis = "BUENO ";
    icono = 0x03;
  }
  else if ( t >= limite_bueno_convertido && t <= limite_malo_convertido )
  {
    Serial.println("Aire regular");
    analisis = "REGULAR ";
    icono = 0x21;
  }
  else if (t >= limite_malo_convertido )
  {
    Serial.println("Aire peligroso");
    analisis = "MALO ";
    icono = 0x13;

  }


  ThingSpeak.setField(1, t); 

  int x = ThingSpeak.writeFields(2061125, "7Q49OISGCTDWLNAV");

  //inicio oled

  display.setTextSize(1);             // establece el tamaño de la fuente
  display.setTextColor(SSD1306_WHITE);        // color del texto
  display.setCursor(0, 0);            // donde inicia el cursor

  //Obtener la señal de wifi y convertirlo en barras
  long rssi = WiFi.RSSI();
  Serial.print("rssi:");
  Serial.println(rssi);
  int bars;

  // se establece el número de barras usando una serie de if/else
  if (rssi >= -55) {
    bars = 5;
  } else if (rssi <= -55 & rssi >= -65) {
    bars = 4;
  } else if (rssi <= -65 & rssi >= -70) {
    bars = 3;
  } else if (rssi <= -70 & rssi >= -78) {
    bars = 2;
  } else if (rssi <= -78 & rssi >= -82) {
    bars = 1;
  } else {
    bars = 0;
  }

// se establece la posicion del cursor y que muestre el siguiente mensaje 
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Cantidad de CO2");
 
 // aqui se dibujan las barras de la intensidad de la señal
   
  display.fillRect(64,28,3,5,WHITE);
  display.fillRect(69,23,3,10,WHITE);
  display.fillRect(74,18,3,15,WHITE);
  display.fillRect(79,13,3,20,WHITE);
  display.fillRect(84,8,3,25,WHITE);
  display.setTextColor(WHITE);
  display.setTextSize(1);

// aqui se colocara todo lo demas que se quiera mostrar en la pantalla

  //valor co2
  display.setTextSize(4);             
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 17);
  display.print(t); // se mostrara el valor que se almaceno en t

  //ppm 
  display.setTextSize(1);             
  display.setTextColor(SSD1306_WHITE);        
  display.setCursor(100, 17);
  display.println(F("PPM"));

  //calidad
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); 
  display.setCursor(0, 55);
  display.print(" AIRE ");
  display.println(analisis);
// la palabra fija en AIRE y la palabra que le sigue dependera del analisis si es bueno, malo o regular

  //icono
  display.setTextSize(4);            
  display.setTextColor(SSD1306_WHITE);       
  display.setCursor(100, 32);
  display.write(icono);
  display.display();
  // fin codigo relacionado a la pantalla

  delay(15000); // esperar 15 segundos para actualizar los datos en thingspeak

// si x es igual a 200 se actualizo la informacion sino mostrara mensaje de error
  if (x == 200) {
    Serial.println("Channel update successful.");
  }
  else {
    Serial.println("Problem updating channel. HTTP error code " + String(x));

  }
}

