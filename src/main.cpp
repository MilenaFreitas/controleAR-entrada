#include <Arduino.h>
#include <EEPROM.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Ticker.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <SPI.h>
#include <WiFiClient.h>
#include <OneWire.h>  
#include <DallasTemperature.h>

#define EEPROM_SIZE 1024
#define WIFI_NOME "Metropole" //rede wifi específica
#define WIFI_SENHA "908070Radio"
#define BROKER_MQTT "10.71.0.2"
#define DEVICE_TYPE "AR-redacao-entrada"
#define TOKEN "ib+r)WKRvHCGjmjGQ0"
#define ORG "n5hyok"
#define PUBLISH_INTERVAL 2000//intervalo de 5 min para publicar temperatura
OneWire pino(32);

DallasTemperature barramento(&pino);
DeviceAddress sensor;
uint64_t chipid=ESP.getEfuseMac(); // The chip ID is essentially its MAC address(length: 6 bytes).
uint16_t chip=(uint16_t)(chipid >> 32);
char DEVICE_ID[23];
char an=snprintf(DEVICE_ID, 23, "biit%04X%08X", chip, (uint32_t)chipid); // PEGA IP
String mac;
WiFiClient espClient;
PubSubClient client(espClient);
WebServer server(80);
WiFiUDP udp;
NTPClient ntp(udp, "a.st1.ntp.br", -3 * 3600, 60000); //Hr do Br
char topic1[]= "status3";          // topico MQTT
char topic3[]= "memoria3";  
char topic4[]= "tempideal3";
char topic5[]= "permissao3";
char topic6[]= "permissaoResposta3";
bool publishNewState = false; 
TaskHandle_t retornoTemp;
IPAddress ip=WiFi.localIP();
unsigned long tempo=1000*60*10; // verifica movimento a cada 15 min
const long intervalo=1000*60*5; //se tiver sem rede espera 1 min para tentar de novo
unsigned long ultimoGatilho = millis()+tempo;
unsigned long previousMillis=0;
unsigned long previousMillis1=0;
unsigned long previousMilliss=0;
float tempAtual=0;
struct tm data; //armazena data 
char data_formatada[64];
char hora_formatada[64];
int movimento=0;
int rede;
String comando;
int cont=0;
int vez=0;
int vez2=0;
std::string msg;
std::string msg1;
std::string msg2;
std::string msg3;
std::string msg4;
int tIdeal;
int data_semana;
int Hliga;
int Hdes;
int contTemp=0;
//=============================================================
const int pirPin1=33; 
const int con=25;  
const int eva=26;
////////////////////////////////////////////////////////////////
void dadosEEPROM(){
  //DEFINE OS DADOS EMERGENCIAIS DA EPROOM 
  if(EEPROM.read(0) != tIdeal){
    EEPROM.write(0, tIdeal);  //escreve tempIdeal no dress=0 vindo do mqtt
    Serial.println(tIdeal);
  } else if(EEPROM.read(2) != Hdes){
    EEPROM.write(1, Hdes);
    Serial.println(Hdes);
  } else if(EEPROM.read(4) != Hliga){
    EEPROM.write(2, Hliga);
    Serial.println(Hliga);
  }
  Serial.println("ESCREVEU NA EEPROM");
}
void callback(char* topicc, byte* payload, unsigned int length){
  //{"agenda":{"diaSemana":2,"horaLiga":7,"horaDesliga":19},"tempIdeal":24}
  String topicStr = topicc;
  if(topicStr=="memoria2"){ 
    Serial.println("ENTROU NO CALLBACK");
    Serial.print(topicc);
    Serial.print(": ");
    for (int i = 0; i < length; i++){
      Serial.print((char)payload[i]);
      msg += (char)payload[i];
    }
    if(msg == "false"){
      StaticJsonDocument<256> doc5;
      doc5["status"] = "ERROR-MQTTCALLBACK";
      char buffer[256];
      serializeJson(doc5, buffer);
      client.publish(topic4, buffer);
      dadosEEPROM();
      Serial.println("false MQTT");
      Serial.println(buffer);
    } else {
      Serial.println("TA NO NORMAAAAAAL");
      msg1= msg.substr(23,1);  //dia semana
    
      data_semana = atoi(msg1.c_str());

      msg2= msg.substr(36,1);  //hora liga
      Serial.print(msg2.c_str());
      Serial.println();
      Hliga = atoi(msg2.c_str());

      msg3= msg.substr(52,2);  //hora desliga
      Serial.print(msg3.c_str());
      Serial.println(); 
      Hdes = atoi(msg3.c_str());

      msg4= msg.substr(68,2);  //tideal
      Serial.print(msg4.c_str());
      Serial.println();
      tIdeal = atoi(msg4.c_str());
      dadosEEPROM(); //escreve na eeprom o valor
    }
  }else if(topicStr = "permissaoResposta2"){
    Serial.println("ENTROU NO CALLBACK");
    Serial.print(topicc);
    Serial.print(": ");
    for (int i = 0; i < length; i++){
      Serial.print((char)payload[i]);
      comando=(char)payload[i];
    }
    Serial.println();
  }
}
void conectaMQTT(){
  //Estabelece conexao c MQTT/WIFI
   if(!client.connected()){
    Serial.println("conectando...");
    if (client.connect("AR-redacao-entrada")){
      Serial.println("CONECTADO! :)");
      client.publish ("teste", "hello word"); 
      client.subscribe (topic1);
      client.subscribe (topic3);
      client.subscribe (topic4);
      client.subscribe (topic5);
      client.subscribe (topic6);
    } else {
      Serial.println("Falha na conexao");
      Serial.print(client.state());
      Serial.print("nova tentativa em 5s...");
      delay (5000);
    }
  }
}
void reconectaMQTT(){
  //reconecta MQTT caso desconecte
  if (!client.connected()){
    conectaMQTT();
  }
  client.loop();
}
void datahora(){
  time_t tt=time(NULL);
  data = *gmtime(&tt);
  strftime(data_formatada, 64, "%w - %d/%m/%y %H:%M:%S", &data);//hora completa
  strftime(hora_formatada, 64, "%H:%M", &data); //hora para o visor
  //Armazena na variável hora, o horário atual.
  int anoEsp = data.tm_year;
  if (anoEsp < 2020) {
    ntp.forceUpdate();
  }	
}
void iniciaWifi(){
  WiFi.begin(WIFI_NOME, WIFI_SENHA); 
  while (WiFi.status()!= WL_CONNECTED){//incia wifi ate q funcione
    Serial.print (".");
    delay(1000);
    cont++;
    if(cont==15){  //se n funcionar sai do loop e fica sem rede
      Serial.println("break");
      rede=0; //status da rede
      Serial.println(rede);
      break;
    }
  } if(WiFi.status() == WL_CONNECTED){
    rede=1;
    if (!client.connected()){  //aqui é while, mudei pra teste
      conectaMQTT();
    }
  }
}
void redee(){
  if(rede==1){  //está conectado
    //protocolo online via MQTT
    Serial.println("rede 1");
  } else if(rede==0){ //n esta conectado a rede
    //protocolo offline
    Serial.println("rede 0");
    EEPROM.begin(EEPROM_SIZE);
    tIdeal=EEPROM.read(0);
    Hdes=EEPROM.read(1);
    Hliga=EEPROM.read(2);
  }
}
void tentaReconexao(){ //roda assincrona no processador 0
  unsigned long currentMillis = millis();
  if (currentMillis-previousMillis<= (1000*60*5)) { //a cada 5min tenta reconectar
    Serial.print("*************************");
    Serial.print("TENTA RECONEXAO");
    Serial.println("***********************");
    previousMillis=currentMillis;
    iniciaWifi();
    ntp.forceUpdate();
  }
}
void sensorTemp(){
  unsigned long currentMilliss = millis();
  if ((currentMilliss-previousMilliss)>= (10000)){
    Serial.println ("sensorTemp inicio do LOOP");
    //busca temp enquanto estiver ligado
    barramento.requestTemperatures(); 
    float temperatura = barramento.getTempC(sensor);
    Serial.print("a Temperatura é: ");
    Serial.println(temperatura);
    if(temperatura>0 && temperatura<50){
      tempAtual=temperatura;
      contTemp=0;
    } else if(temperatura<0 || temperatura>50){
      contTemp++;
      if(contTemp==5){
        tempAtual=temperatura;
        Serial.print("temp atual erradaa: ");
        Serial.println(tempAtual);
      }
    }
    previousMilliss=currentMilliss;
  }
  
}
void IRAM_ATTR mudaStatusPir(){
  ultimoGatilho = millis()+tempo;
  movimento=1;
}
void payloadMQTT(){ 
  datahora();
  time_t tt=time(NULL);
  StaticJsonDocument<256> doc;
  doc["local"] = "AR-redacao-entrada";
  doc["ip"] = ip.toString();
  doc["mac"] = mac;
  doc["hora"]=tt;
  doc["temperatura"]=tempAtual;
  doc["movimento"]=movimento; 
  doc["evaporadora"]=!(digitalRead(eva));
  doc["condensadora"]=!(digitalRead(con));
  char buffer1[256];
  serializeJson(doc, buffer1);
  client.publish(topic1, buffer1);
}
void arLiga(){
  String hora;
  hora= data.tm_hour;
  //liga ar
  digitalWrite(eva, 0);
  Serial.println(tempAtual);
  Serial.println(tIdeal);
  if(tempAtual>=(tIdeal+1)){ //quente
    if(digitalRead(eva)==0){
      digitalWrite(con, 0);
      Serial.println("condensadora ligada");
    } else {
      digitalWrite(con, 0);
      digitalWrite(eva, 0);
      Serial.println("condensadora ligada");
    }		
  } else if(tempAtual<=(tIdeal-1)){ //frio
    digitalWrite(con, 1);
    digitalWrite(eva, 0);
    Serial.println("condensadora desligada");	

  } else if(tempAtual==tIdeal){
    Serial.println("temp ideal");	
  }
  delay(30000);
}
void perguntaMQTT(){  
    int Hora = data.tm_hour;
    int data_semana = data.tm_wday; //devolve em numero
    if(data_semana==6 || data_semana==0 ||(Hora<=Hliga && Hora>=Hdes)){
      //se foir sabado ou domingo ou antes de 7h ou depois de 20h 
      //se tiver movimento
      vez=vez+1;
      digitalWrite(con, 1);
      digitalWrite(eva, 1);
      if(vez==1){
        Serial.println("entrou para a parte que pergunta ao MQTT");
        StaticJsonDocument<256> doc;
        doc["perguntaMQTT"] = "Liga ar?";
        char buffer3[256];
        serializeJson(doc, buffer3);
        client.publish(topic5, buffer3);
        Serial.println(buffer3);
      }else if(comando=="1"){
        while(ultimoGatilho>millis()){   //enquanto tiver movimento vai rodar 
          //pode ligar o ar 
          Serial.print("liga o ar pelo MQTT");
          Serial.println(comando);
          arLiga();
          Serial.println("fica rodando no whilee");
          payloadMQTT();
          delay(5000);
          if((data_semana!=6 && data_semana!=0) || (Hora>=Hliga && Hora<=Hdes)){
            vez=0;
            break;
          }
        } 
      } else if(comando=="0"){
        digitalWrite(con, 1);
        digitalWrite(eva, 1);
        Serial.println("nao liga o ar pelo MQTT");
        Serial.println(comando);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
}
void verificaDia(void *pvParameters){
  while(1){ 
    int Hora = data.tm_hour;
    int Minutos	=	data.tm_min;
    //int data_semana = data.tm_wday; //devolve em numero
    if(data_semana != 0 && data_semana != 6){
      //se n for sabado nem domingo 
      if(Hora>=Hliga){
        //esta no horario de ligar
        if(ultimoGatilho>millis()){
          //tem movimento
          //LIGADO
          arLiga();
          Serial.println("estou dentro do horario");
        } else if(ultimoGatilho<millis()){
          //não tem movimento
          digitalWrite(con, 1);
          digitalWrite(eva, 1);
        }
      } else {
        //se fora do horario
        digitalWrite(con, 1);
        digitalWrite(eva, 1);
        perguntaMQTT();
      }
    } else{
      //se fora do dia
      digitalWrite(con, 1);
      digitalWrite(eva, 1);
      perguntaMQTT();
    }
    vTaskDelay(pdMS_TO_TICKS(30000));
  }
}
void PinConfig(){
  // config das portas i/o
	pinMode(pirPin1, INPUT_PULLUP);
  pinMode(eva, OUTPUT);
  pinMode(con, OUTPUT);
}
void setup(){
  Serial.begin (115200);
  iniciaWifi();
  client.setServer (BROKER_MQTT, 1883);//define mqtt
  client.setCallback(callback); 
  server.begin();
  ntp.begin ();
  ntp.forceUpdate();
  if (!ntp.forceUpdate ()){
    Serial.println ("Erro ao carregar a hora");
     delay (1000);
  } else {
    timeval tv; //define a estrutura da hora 
    tv.tv_sec = ntp.getEpochTime(); //segundos
    settimeofday (&tv, NULL);
  }
  redee();  //define as variaveis
  PinConfig();
  xTaskCreatePinnedToCore (verificaDia, "arliga", 10000, NULL, 1, NULL, 0);
  attachInterrupt (digitalPinToInterrupt(pirPin1), mudaStatusPir, RISING);
  datahora();
  ip=WiFi.localIP(); //pega ip
  mac=DEVICE_ID;     //pega mac
  barramento.begin();
  barramento.getAddress(sensor, 0);  // Start up the library
}
void loop(){
  datahora();
  server.handleClient();
  if(WL_DISCONNECTED || WL_CONNECTION_LOST){
    rede=0;
  }else if(rede==0){
    Serial.println(rede);
    tentaReconexao();
  }
  reconectaMQTT();
  int week = data.tm_wday; //devolve em numero
  unsigned long currentMillis1 = millis();
  if ((currentMillis1-previousMillis1)>= intervalo){
    Serial.println("entro no tempo do millis do payload");
    payloadMQTT();
    previousMillis1=currentMillis1;
  }
  if(ultimoGatilho<millis()){
    movimento=0;
  } 
  if(week != data_semana && vez2==0){
    StaticJsonDocument<256> doc5;
    doc5["local"] = "AR-redacao-entrada";
    doc5["mac"] =  mac;
    doc5["etapa"] =  "ligado";
    char buffer[256];
    serializeJson(doc5, buffer);
    client.publish (topic4, buffer);
    Serial.println("mandou mqtt");
    vez2++;
  }
  sensorTemp();
  delay(3000);
}
//mac 1 biitF4A6F9A3C9C8 redação reuniao
//mac 2 biitD8B3F9A3C9C8 redação entrada
