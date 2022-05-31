// Inclusão das bibliotecas
    #include <Arduino.h>
    #include <ArduinoJson.h>
    #include <tcp_axtls.h>
    #include <async_config.h>
    #include <DebugPrintMacros.h>
    #include <SyncClient.h>
    #include <ESPAsyncTCPbuffer.h>
    #include <AsyncPrinter.h>
    #include <ESPAsyncTCP.h>
    #include <DateTime.h>
    #include <TimeElapsed.h>
    #include <DateTimeTZ.h>
    #include <ESPDateTime.h>
    #include <painlessMesh.h>
    #include <string.h>
    #include <bits/stdc++.h>
    #include <SimpleDHT.h>
    #include <Wire.h>
    #include <SPI.h>
    #include <Adafruit_BMP280.h>
    #include <Adafruit_BMP085.h>

 // Define name space   
    using namespace std;

// Parâmetros da rede mesh
    #define   MESH_PREFIX       "pgcc008"
    #define   MESH_PASSWORD     "atividade2"
    #define   MESH_PORT         5555
    #define   WIFI_TYPE         WIFI_AP_STA
    #define   WIFI_HIDE         1
    #define   WIFI_CHANNEL      1

// bmp280 config lib Adafruit_BMP280
    #define BMP_SCK  (13)
    #define BMP_MISO (12)
    #define BMP_MOSI (11)
    #define BMP_CS   (10)

// dht11 config lib SimpleDHT
    int pinDHT11 = 2; //D4
    int dhtPinDef = 3; // elemento 3 do array pinDef
    int dhtMeasure = 4; // tipo de medida que o DHT deve retornar
    SimpleDHT11 dht11(pinDHT11);

// bmp280 config lib Adafruit_BMP280
    int bmp280PinDef = 5; // D2
    int bmp280Measure = 4;
    Adafruit_BMP280 bmp280;

// bmp180 config lib Adafruit_BMP085
    int bmp180PinDef = 6; // D1
    int bmp180Measure = 4;
    Adafruit_BMP085 bmp180;

// Flame sensor
    int flamePinDef = 15;

// UV sensor
    int uvMeasure = 0;

// define os pinos             pin | array |real pin 
    #define   ANALOG            A0  // 0    A0
    #define   GPIO00            D3  // 1    D3
    #define   GPIO01            D3  // 2    TX
    #define   GPIO02            D4  // 3    D4 DHT11
    #define   GPIO03            D4  // 4    RX
    #define   GPIO04            D2  // 5    D2
    #define   GPIO05            D1  // 6    D1
    #define   GPIO06            D1  // 7    SK unused
    #define   GPIO07            D0  // 8    S0 unused
    #define   GPIO08            D1  // 9    S1 unused
    #define   GPIO09            D2  // 10   S2 unused
    #define   GPIO10            D3  // 11   S3 unused
    #define   GPIO11            D3  // 12   SC unused
    #define   GPIO12            D6  // 13   D6 unused
    #define   GPIO13            D7  // 14   D7
    #define   GPIO14            D5  // 15   D5
    #define   GPIO15            D8  // 16   D8
    #define   GPIO16            D0  // 17   D0

// número de pinos definidos no array pinDef
    #define   PINS_NUM   18

// Inicializa os sensores (Sink received parameter)
    typedef struct {
        uint8_t pinNum;
        bool pinSet;
    } pinInit_t;
    pinInit_t pinDef[] {
        {ANALOG, false},
        {GPIO00, false},
        {GPIO01, false},
        {GPIO02, false},
        {GPIO03, false},
        {GPIO04, false},
        {GPIO05, false},
        {GPIO06, false},
        {GPIO07, false},
        {GPIO08, false},
        {GPIO09, false},
        {GPIO10, false},
        {GPIO11, false},
        {GPIO12, false},
        {GPIO13, false},
        {GPIO14, false},
        {GPIO15, false},
        {GPIO16, false}
    };

// Inicializa o json para envio
    DynamicJsonDocument sendJsonData(1024);

// Inicializa o array de dados dos sensores
    float pinData[PINS_NUM] = {0};

// contagem de teste para impressao na tela
    int c = 0;

// message type: broadcast=3, nodes list=2, to node master=1
    uint8_t sendType  = 3;

// list of nodes to send msg
    uint32_t nodeDestiny;

// this node id
    uint32_t nodeOrigin = 0;

// node id to sendSingle
    uint32_t nodeDestination = 0;

// retorno do send single
    boolean returnSendSingle = false;

// fixa o número máximo de tentativas de enviar uma msg
    int maxTries = 5;

// conta o número de tentativas de enviar uma msg
    int countTries = 0;

// Se true, envia msg do sink recebida pela serial para a mesh
    bool meshSend = false;

// id do nodeMcu centralizador (Sink received parameter)
    uint32_t NODE_MASTER = 0;

// Localização deste endDevice
    float latitude = -12.197422000000014;
    float longitude = -38.96674600000001;

// Pressão local em milibar
    float currentPressure = 1013.25;
  
// Período de leitura de dados do sensor (Sink received parameter)
    int T_send = 1;
    int Old_T_send = 1;

// Timestamp adjust
    unsigned long timestamp = 0;
    unsigned long adjusterTime = 0;

// Mensagem a ser enviada
    char meshMsg[1024];

// Mensagem a ser repassada pelo node master
    String meshExternalMsg;

// Inicializa a rede mesh
    painlessMesh  mesh;

// Define o scheduler em substituição ao método delay
    Scheduler userScheduler;

// Define a function para envio de mensagens
    void sendMessage();

// Define a tarefa de enviar mensagens e o tempo
    Task taskSendMessage( TASK_SECOND*T_send, TASK_FOREVER, &sendMessage );

// ----------------------------------------------------------------------------
// Functions
// ----------------------------------------------------------------------------

// atualiza o timestamp deste node
void timestampAdjust(int t){
    adjusterTime = millis();
    timestamp = t;
}

// atualiza o tempo de envio das mensagens
void sendTimeAdjust(){    
    taskSendMessage.setInterval(TASK_SECOND*T_send);
    taskSendMessage.enable();
    Serial.printf("sendTimeAdjust:........................................... %d\n",T_send);
}

// habilita os pinos recebidos por parâmetro
void pinEnable(){
    for(int i = 0;i < PINS_NUM-1;++i){
        uint8_t pin = pinDef[i].pinNum;
        if(pinDef[i].pinSet == true){
            pinMode(pin, INPUT);
        }
    }
}

// testa se a chave json existe e atribui à respectiva variável global se o nó for destino
bool getParameters(String toGet){
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, toGet);
    JsonObject receivedJsonData = doc.as<JsonObject>();
    if(receivedJsonData.containsKey("nodeDestiny")){
        uint32_t n = receivedJsonData["nodeDestiny"];
        nodeDestiny = n;
    }
    if(receivedJsonData.containsKey("type")){
        sendType = receivedJsonData["type"];
    }
    if(receivedJsonData.containsKey("send")){
        meshSend = receivedJsonData["send"];
    }
    if(nodeOrigin == nodeDestiny or sendType==3){
        if(receivedJsonData.containsKey("timestamp")){
            unsigned long t = receivedJsonData["timestamp"];
            timestampAdjust(t);
        }
        if(receivedJsonData.containsKey("t_send")){
            T_send = receivedJsonData["t_send"];
            sendTimeAdjust();
        }
        if(receivedJsonData.containsKey("pinDef")){
            if(nodeDestiny == nodeOrigin){
                for(int i=0;i<PINS_NUM;++i){
                    bool v = receivedJsonData["pinDef"][i];
                    pinDef[i].pinSet = v;
                }
                pinEnable();
            }
        }
        if(receivedJsonData.containsKey("uvMeasure")){
            uvMeasure = receivedJsonData["uvMeasure"];
        }
        if(receivedJsonData.containsKey("dhtPinDef")){
            dhtPinDef = receivedJsonData["dhtPinDef"];
        }
        if(receivedJsonData.containsKey("dhtMeasure")){
            dhtMeasure = receivedJsonData["dhtMeasure"];
        }
        if(receivedJsonData.containsKey("bmp280PinDef")){
            bmp280PinDef = receivedJsonData["bmp280PinDef"];
        }
        if(receivedJsonData.containsKey("bmp280Measure")){
            bmp280Measure = receivedJsonData["bmp280Measure"];
        }
        if(receivedJsonData.containsKey("bmp180PinDef")){
            bmp180PinDef = receivedJsonData["bmp180PinDef"];
        }
        if(receivedJsonData.containsKey("bmp180Measure")){
            bmp180Measure = receivedJsonData["bmp180Measure"];
        }
        if(receivedJsonData.containsKey("flamePinDef")){
            flamePinDef = receivedJsonData["flamePinDef"];
        }
        if(receivedJsonData.containsKey("node_master")){
            uint32_t node_master = receivedJsonData["node_master"];
            NODE_MASTER = node_master;
        }
        Serial.println("Parameters changed in this node....................");
        if(sendType==3){
            return false;
        }
        else{
            return true;
        }
    }
    else{
        Serial.println("Parameters don't changed in this node..............");
        return false;
    }
}

// Dispara quando uma mensagem é recebida
void receivedCallback(uint32_t from, String &msg){
    if(nodeOrigin == NODE_MASTER){
        Serial.println(msg);
    }
    else{
        getParameters(msg.c_str());
        if(sendType == 4){
            Old_T_send = T_send;
            T_send = 1;
            sendTimeAdjust();
        }
    }
}

// Inicializa a conexão da rede mesh
void newConnectionCallback(uint32_t nodeId){
    Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
}

// Tenta mudar a conexão em caso de perda
void changedConnectionCallback(){
    Serial.printf("Changed connections\n");
}

// Ajusta o tempo de resposta do endDevice
void nodeTimeAdjustedCallback(int32_t offset){
    Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
}

// inicialização da mesh
void meshInit(){
    mesh.setDebugMsgTypes( ERROR | STARTUP );
    mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT,WIFI_TYPE,WIFI_CHANNEL,WIFI_HIDE);
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionCallback);
    mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
    userScheduler.addTask( taskSendMessage );
    taskSendMessage.enable();
    nodeOrigin = mesh.getNodeId();
}

// Leitura do dht11
float readDht11(int measure = 4){
    float temperature = 0;
    float humidity = 0;
    unsigned char data[40] = {0};
    int err = SimpleDHTErrSuccess;
    if ((err = dht11.read2(&temperature, &humidity, data)) != SimpleDHTErrSuccess) {
        return 0;
    }
    if(measure == 1){
        return temperature;
    }
    else if(measure == 2){
        return humidity;
    }
    else if(measure == 4){
        sendJsonData["temperature"] = temperature;
        sendJsonData["humidity"] = humidity;
        return 0;
    }
    else{
        return -1;
    }
}

// Leitura do bmp280
float readBmp280(int measure = 4){
    if (bmp280.takeForcedMeasurement()) {
        if(measure == 1){
            return bmp280.readTemperature();
        }
        else if(measure == 2){
            return bmp280.readPressure();
        }
        else if(measure == 3){
            return bmp280.readAltitude(currentPressure);
        }
        else if(measure == 4){
            sendJsonData["temperature"] = bmp280.readTemperature();
            sendJsonData["pressure"] = bmp280.readPressure();
            return 0;
        }
        else{
            return -1;
        }
    }
    else{
        return -1;
    }
}

// Leitura do bmp180
float readBmp180(int measure = 4){
    if(measure == 1){
        return bmp180.readTemperature();
    }
    else if(measure == 2){
        return bmp180.readPressure();
    }
    else if(measure == 3){
        return bmp180.readAltitude(currentPressure);
    }
    else if(measure == 5){
        return bmp180.readSealevelPressure();
    }
    else if(measure == 4){
        sendJsonData["temperature"] = bmp180.readTemperature();
        sendJsonData["pressure"] = bmp180.readPressure();
        return 0;
    }
    else{
        return -1;
    }
}

// read flame sensor trigger
bool readFlame(int pin){
    bool flame = digitalRead(pin);
    if(flame){
        meshSend = false;
        sendJsonData["flame"] = true;
        sendMessage();
    }
    return flame;
}

float readUv(int pin,int measure = 0){
    float a = analogRead(pin);
    if(measure == 1){
        sendJsonData["uv"] = a;
    }
    return a;
}

// Leitura de todos os sensores
void readSensors(){
    for(int i = 0;i < PINS_NUM-1;++i){
      if(pinDef[i].pinSet == true){
            uint8_t pin = pinDef[i].pinNum;
            if(i == 0){
                pinData[i] = readUv(pin,uvMeasure);
                uvMeasure = 0;
            }
            else{
                if(i == dhtPinDef){
                    pinData[i] = readDht11(dhtMeasure);
                    dhtMeasure = 4;
                }
                else if(i == bmp280PinDef){
                    pinData[i] = readBmp280(bmp280Measure);
                    bmp280Measure = 4;
                }
                else if(i == bmp180PinDef){
                    pinData[i] = readBmp180(bmp180Measure);
                    bmp180Measure = 4;
                }
                else if(i == flamePinDef){
                    pinMode(pin, INPUT_PULLUP);
                    pinData[i] = readFlame(pin);
                }
                else{
                    pinData[i] = digitalRead(pin);
                }
            }
      }
      else{
            pinData[i] = 0;
      }
    }
}

// passagem dos dados para json
void jsonParse(){
    sendJsonData["device"] = nodeOrigin;
    sendJsonData["node_master"] = NODE_MASTER;
    sendJsonData["nodeDestiny"] = NODE_MASTER;
    sendJsonData["nodeTime"] = mesh.getNodeTime();
    sendJsonData["timestamp"] = timestamp+((millis()-adjusterTime)/1000); // DateTime.now(); // 
    sendJsonData["latitude"] = latitude;
    sendJsonData["longitude"] = longitude;
    for(int i=0;i<PINS_NUM;++i){
        sendJsonData["pinDef"][i] = pinDef[i].pinSet;
    }
    readSensors();
    for(int i=0;i<PINS_NUM-1;++i){
        if(pinDef[i].pinSet == true){
            sendJsonData["data"][i] = pinData[i];
        }
        else{
            sendJsonData["data"][i] = 0;
        }
    }    
    serializeJson(sendJsonData, meshMsg);
}

// reseta os dados enviados
void resetJsonData(){
    sendJsonData.remove("temperature");
    sendJsonData.remove("pressure");
    sendJsonData.remove("humidity");
    sendJsonData.remove("flame");
    sendJsonData.remove("uv");
}

// Função de envio de mensagens
void sendMessage(){
    jsonParse();
    if(nodeOrigin == NODE_MASTER){
            if(meshSend){
                if(sendType == 3){
                    mesh.sendBroadcast(meshExternalMsg);
                }
                else{
                    returnSendSingle = mesh.sendSingle(nodeDestiny,meshExternalMsg);
                    if(returnSendSingle){
                        countTries = 0;
                    }
                    else{
                        countTries += 1;
                        if(countTries >= maxTries){
                            countTries = 0;
                        }
                    }
                }
                meshSend = false;
                sendType = 1;
                nodeDestiny = NODE_MASTER;
            }
            else{
                Serial.println(meshMsg);
            }
    }
    else{
        // para desativar o sendType = 4
        if(strlen(meshMsg) > 0){
            mesh.sendSingle(NODE_MASTER, meshMsg);
            if(sendType == 4){
                T_send = Old_T_send;
                sendTimeAdjust();
                resetJsonData();
                sendType = 1;
            }
        }
    }
}

// envia as mensagens recebidas e formuladas para a serial (Sink) - apenas para node master
void sendSerial(){
    if(NODE_MASTER != nodeOrigin){
        if(c == 5000){
            String firstMsg = "";
            firstMsg += "{";
            firstMsg += "\"id_node\":";
            firstMsg += nodeOrigin;
            firstMsg += ",\"nodeList\":[";
            int i = 0;
            std::list<uint32_t> nodeList = mesh.getNodeList();
            std::list<uint32_t>::iterator node = nodeList.begin();
            while (node != nodeList.end()){
                firstMsg += *node;
                firstMsg += i>=0?",":"";
                node++;
                i++;
            }
            firstMsg += "]}";
            Serial.println(firstMsg);
            c = 0;
        }
        c += 1;
    }
}

// lê a serial - apenas para node master
void readSerial(){
    if(Serial.available() > 0) {
        String jsonRec = Serial.readString();
        if(getParameters(jsonRec)){
            Serial.println("Changing internal parameters..................");
        }
        else{
            meshExternalMsg = jsonRec;
            Serial.println("Redirecting message from sink.........................");
        }
    }
}

// configura o bmp280
void bmp280Config(){
    bmp280.begin();
    bmp280.setSampling( Adafruit_BMP280::MODE_FORCED, /* Operating Mode. */
                    Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                    Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                    Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                    Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
}

// configura o bmp180
void bmp180Config(){
    bmp180.begin();
}

// config
void setup(){
    Serial.begin(115200);
    DateTime.setTimeZone("America/Bahia");
    DateTime.begin(/* timeout param */);
    meshInit();
    pinEnable();
    bmp280Config();
    bmp180Config();
}

// Loop
void loop(){
    mesh.update();
    readSerial();
    sendSerial();
}