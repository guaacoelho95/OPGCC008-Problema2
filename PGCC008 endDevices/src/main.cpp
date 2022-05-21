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
    boolean extPinDef[PINS_NUM] = {0};

// Inicializa o array de dados dos sensores
    float pinData[PINS_NUM] = {0};

// Localização deste endDevice
    float latitude = -12.197422000000014;
    float longitude = -38.96674600000001;
  
// Período de leitura de dados do sensor (Sink received parameter)
    float T_sensor = 1;
    float T_sensorSend = 0;

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

// habilita os pinos recebidos por parâmetro
void pinEnable(){
    for(int i = 0;i < PINS_NUM-1;++i){
        uint8_t pin = pinDef[i].pinNum;
        if(pinDef[i].pinSet == true){
            pinMode(pin, INPUT);
            Serial.printf("Pin %d Enabled\n",pin);
        }
        else{
            Serial.printf("Pin %d Disabled\n",pin);
        }
    }
}

// Dispara quando uma mensagem é recebida
void receivedCallback(uint32_t from, String &msg){
    DynamicJsonDocument receivedJson(1024);
    deserializeJson(receivedJson, msg);
    if(receivedJson.containsKey("nodeToSend") && sendType == 2){
        uint32_t n = receivedJson["nodeToSend"];
        nodeToSend = n;
    }
    if(receivedJson.containsKey("pinDef")){
        for(int i = 0;i < PINS_NUM-1;++i){
            bool v = receivedJson["pinDef"][i];
            pinDef[i].pinSet = v;
            Serial.printf("%d\n",pinDef[i].pinSet);
        }
        if(nodeToSend != nodeOrigin){
            pinEnable();
        }
    }
    if(receivedJson.containsKey("timestamp")){
        int t = receivedJson["timestamp"];
        DateTime.setTime(t);
        Serial.printf("MESH Timestamp received: %d",t);
    }
    if(receivedJson.containsKey("t_sensor")){
        T_sensor = receivedJson["t_sensor"];
        Serial.printf("MESH Time msg received: %6.2f\n",T_sensor);
    }
    if(receivedJson.containsKey("node_master")){
        NODE_MASTER = receivedJson["node_master"];
        if(nodeOrigin == NODE_MASTER){
            Serial.println("MESH "+msg);
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
            sendJsonData["data"][i] = pinData[i];
        }
        else{
            sendJsonData["data"][i] = 0;
        }
    }
}

// Função de envio de mensagens
void sendMessage(){
    String msg = "";
    int timestamp = DateTime.now();
    sendJsonData["device"] = nodeOrigin;
    sendJsonData["node_master"] = NODE_MASTER;
    sendJsonData["nodeToSend"] = NODE_MASTER;
    sendJsonData["nodeTime"] = mesh.getNodeTime();
    sendJsonData["timestamp"] = timestamp;
    sendJsonData["latitude"] = latitude;
    sendJsonData["longitude"] = longitude;
    readSensors();
    if(nodeOrigin == NODE_MASTER){
        serializeJson(sendJsonData, msg);
        for(int i=0;i<PINS_NUM;++i){
            sendJsonData["pinDef"][i] = extPinDef[i];
        }
        Serial.println("-----------------------------------------------------------------------");
        Serial.println("SENDED BY THIS endDevice (nodeMaster)");
        Serial.println("-----------------------------------------------------------------------");
        Serial.println(msg);
        if(meshSend){
            if(sendType == 3){
                mesh.sendBroadcast(msg);
            }
            int len = nodeToSend?0:1;
            while (nodeToSend) {len++; nodeToSend/=10;}
            if(sendType == 2 && len == 10){
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
            meshSend = false;
            sendType = 3;
        }
    }
    else{
        serializeJson(sendJsonData, msg);
        mesh.sendSingle(NODE_MASTER, msg);
    }
}

// lê a serial - apenas para node master
void readSerialData(){
    if(Serial.available() > 0) {
        String jsonRec = Serial.readString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, jsonRec);
        JsonObject docSerialRec = doc.as<JsonObject>();
        meshSend = docSerialRec["send"];
        sendType = docSerialRec["type"];

        // ver a possibilidade abaixo
        // if nodeToSend == nodeOrigin
        //      receba os parâmetros e atribua às variáveis deste node
        // else
        //      envie para o json de sendMessage()
        // 
        if(docSerialRec.containsKey("timestamp")){
            int t = docSerialRec["timestamp"]; // para global
            DateTime.setTime(t); // para function sendMessage decidir se é interno ou externo
        }
        if(docSerialRec.containsKey("t_sensor")){
            T_sensorSend = docSerialRec["t_sensor"]; // para global e trocar para T_sensor
            Serial.printf("SERIAL T_sensor: %6.2f\n",T_sensor);
        }
        if(docSerialRec.containsKey("nodeToSend") && sendType == 2){
            uint32_t n = docSerialRec["nodeToSend"]; // para global
            nodeToSend = n;
        }
        if(docSerialRec.containsKey("pinDef")){
            for(int i=0;i<PINS_NUM;++i){
                bool v = docSerialRec["pinDef"][i];
                extPinDef[i] = v; // para sendMessage decidir se é interno ou externo
            }
            if(nodeToSend == nodeOrigin){
                pinEnable();
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
                Serial.printf("{\"id_node\":%u}",nodeOrigin);
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
    Serial.printf("{\"id_node\":%u}",nodeOrigin);
    pinEnable();
}

// Loop
void loop(){
    mesh.update();
    readSerialData();
}