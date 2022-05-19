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
    
// Parâmetros da rede mesh
    #define   MESH_PREFIX       "pgcc008"
    #define   MESH_PASSWORD     "atividade2"
    #define   MESH_PORT         5555

// define os pinhos
    #define   ANALOG            A0
    #define   GPIO00            D3
    #define   GPIO01            D3
    #define   GPIO02            D4
    #define   GPIO03            D4
    #define   GPIO04            D2
    #define   GPIO05            D1
    #define   GPIO06            D1  //unused
    #define   GPIO07            D0  //unused
    #define   GPIO08            D1  //unused
    #define   GPIO09            D2  //unused
    #define   GPIO10            D3  //unused
    #define   GPIO11            D3  //unused
    #define   GPIO12            D6  //unused
    #define   GPIO13            D7
    #define   GPIO14            D5
    #define   GPIO15            D8
    #define   GPIO16            D0

// número de pinos definidos no array pinDef
    #define   PINS_NUM          18

// contagem de teste para impressao na tela
    int c = 0;

// message type: broadcast=3, nodes list=2, to node master=1
    uint8_t sendType  = 3;

// list of nodes to send msg
    uint32_t nodeToSend;

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
    boolean meshSend = false;

// id do nodeMcu centralizador (Sink received parameter)
    uint32_t NODE_MASTER = 0;

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

// Inicializa o array para definição dos pinos por sendMessage
    uint8_t extPinDef[PINS_NUM] = {0};

// Inicializa o array de dados dos sensores
    float pinData[PINS_NUM] = {0}; //{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

// Localização deste endDevice
    float latitude = -12.197422000000014;
    float longitude = -38.96674600000001;
  
// Período de leitura de dados do sensor (Sink received parameter)
    float T_sensor = 1;

// Inicializa a rede mesh
    painlessMesh  mesh;

// Define o scheduler em substituição ao método delay
    Scheduler userScheduler;

// Define a function para envio de mensagens
    void sendMessage();

// inicializa o json
    DynamicJsonDocument sendJsonData(1024);

// Define a tarefa de enviar mensagens e o tempo
    Task taskSendMessage(TASK_SECOND*T_sensor,TASK_FOREVER,&sendMessage);

// ----------------------------------------------------------------------------
// Functions
// ----------------------------------------------------------------------------

// Definição dos sensores em funcionamento
void defSensors(int pins){
    for(uint8_t i = 0;i < PINS_NUM-1;++i){
        pinDef[i].pinSet = pins[&i];
        Serial.println(pinDef[i].pinSet);
    }
}

// Dispara quando uma mensagem é recebida
void receivedCallback(uint32_t from, String &msg){
    DynamicJsonDocument receivedJson(1024);
    deserializeJson(receivedJson, msg);
    if(receivedJson.containsKey("pinDef")){
        defSensors(receivedJson["pinDef"]);
    }
    if(receivedJson.containsKey("timestamp")){
        int t = receivedJson["timestamp"];
        DateTime.setTime(t);
    }
    if(receivedJson.containsKey("node_master")){
        NODE_MASTER = receivedJson["node_master"];
        if(nodeOrigin == NODE_MASTER){
            Serial.println(msg);
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
    mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionCallback);
    mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
    userScheduler.addTask( taskSendMessage );
    taskSendMessage.enable();
    nodeOrigin = mesh.getNodeId();
}

// Leitura dos sensores
void readSensors(){
    for(int i = 0;i < PINS_NUM-1;++i){
      if(pinDef[i].pinSet == true){
            uint8_t pin = pinDef[i].pinNum;
            if(i == 0){
                pinData[i] = analogRead(pin);
            }
            else{
                pinData[i] = digitalRead(pin); 
            }
      }
      else{
            pinData[i] = 0;
      }
    }
    for(int i=0;i<PINS_NUM-1;++i){
        if(pinDef[i].pinSet == true){
            sendJsonData["pin"][i] = pinDef[i].pinNum;
            sendJsonData["val"][i] = pinData[i];
        }
        else{
            sendJsonData["pin"][i] = 0;
            sendJsonData["val"][i] = 0;
        }
    }
}

// Função de envio de mensagens
void sendMessage(){
    int s = 0;
    String msg = "";
    int timestamp = DateTime.now();
    sendJsonData["device"] = nodeOrigin;
    sendJsonData["node_master"] = NODE_MASTER;
    sendJsonData["nodeToSend"] = NODE_MASTER;
    sendJsonData["nodeTime"] = mesh.getNodeTime();
    sendJsonData["timestamp"] = timestamp;
    sendJsonData["latitude"] = latitude;
    sendJsonData["longitude"] = longitude;
    for(unsigned int i=0;i<sizeof(extPinDef);++i){
        s += extPinDef[i];
    }
    if(s>0){
        sendJsonData["pinDef"] = extPinDef;
    }
    readSensors();
    serializeJson(sendJsonData, msg);
    if(nodeOrigin == NODE_MASTER){
        Serial.println("-------------------------------------------------------");
        Serial.println("------------> This endDevice (nodeMaster) <------------");
        Serial.println("-------------------------------------------------------");
        Serial.println(msg);
        Serial.println("-------------------------------------------------------");
        Serial.println("-------------------------------------------------------");
        if(meshSend){
            if(sendType == 3){
                mesh.sendBroadcast(msg);
                sendType = 1;
            }
            if(sendType == 2 && nodeToSend>99){
                returnSendSingle = mesh.sendSingle(nodeToSend,msg);
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
        }
    }
    else{
        mesh.sendSingle(NODE_MASTER, msg);
    }
}

void readSerialData(){
    if(Serial.available() > 0) {
        String jsonRec = Serial.readString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, jsonRec);
        JsonObject docSerialRec = doc.as<JsonObject>();
        meshSend = docSerialRec["send"];
        sendType = docSerialRec["type"];
        if(docSerialRec.containsKey("timestamp")){
            int t = docSerialRec["timestamp"];
            DateTime.setTime(t);
        }
        if(docSerialRec.containsKey("nodeToSend") && !docSerialRec["nodeToSend"].isNull() && sendType == 2){
            uint32_t n = docSerialRec["nodeToSend"];
            nodeToSend = n;
            if(docSerialRec.containsKey("pinDef")){
                defSensors(docSerialRec["pinDef"]);
            }
            if(docSerialRec.containsKey("t_sensor")){
                T_sensor = docSerialRec["t_sensor"];
            }
        }
        if(docSerialRec.containsKey("node_master") && !docSerialRec["node_master"].isNull()){
            uint32_t node_master = docSerialRec["node_master"];
            NODE_MASTER = node_master;
            sendType = 3;
            meshSend = true;
        }
    }
    else{
        if(NODE_MASTER != nodeOrigin){
            c += 1;
            if(c == 5000){
                Serial.println(nodeOrigin);
                c = 0;
            }
        }
    }
}

// config
void setup(){
    Serial.begin(115200);
    DateTime.setTimeZone("America/Bahia");
    DateTime.begin(/* timeout param */);
    meshInit();
    Serial.println(nodeOrigin);
    for(int i = 0;i < PINS_NUM-1;++i){
      if(pinDef[i].pinSet == true){
          uint8_t pin = pinDef[i].pinNum;
          pinMode(pin, INPUT);
      }
    }
}

// Loop
void loop(){
    mesh.update();
    readSerialData();
}