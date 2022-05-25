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
    
// Parâmetros da rede mesh
    #define   MESH_PREFIX       "pgcc008"
    #define   MESH_PASSWORD     "atividade2"
    #define   MESH_PORT         5555
    #define   WIFI_TYPE         WIFI_AP_STA
    #define   WIFI_HIDE         1
    #define   WIFI_CHANNEL      1

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
    
    // #define   ANALOG            A0
    // #define   GPIO00            D3
    // #define   GPIO01            TX
    // #define   GPIO02            D4
    // #define   GPIO03            RX
    // #define   GPIO04            D2
    // #define   GPIO05            D1
    // #define   GPIO06            SK  //unsed
    // #define   GPIO07            S0  //unsed
    // #define   GPIO08            S1  //unsed
    // #define   GPIO09            S2  //unsed
    // #define   GPIO10            S3  //unsed
    // #define   GPIO11            SC  //unsed
    // #define   GPIO12            D6  //unsed
    // #define   GPIO13            D7
    // #define   GPIO14            D5
    // #define   GPIO15            D8
    // #define   GPIO16            D0

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

// Inicializa o array de dados dos sensores
    float pinData[PINS_NUM] = {0};

// contagem de teste para impressao na tela
    int c = 0;

// message type: broadcast=3, nodes list=2, to node master=1
    uint8_t sendType  = 3;

// list of nodes to send msg
    uint32_t nodeToSend;

// this node id
    uint32_t nodeOrigin = 0;

// node list
    //String nodelist;
    SimpleList<uint32_t> nodeList;

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

// Localização deste endDevice
    float latitude = -12.197422000000014;
    float longitude = -38.96674600000001;
  
// Período de leitura de dados do sensor (Sink received parameter)
    int T_send = 1;

// Timestamp adjust
    int timestamp = 0;

// Mensagem a ser enviada
    char meshMsg[1024];
    //String meshMsg; // falhando, retorna null

// Mensagem a ser repassada pelo node master
    String meshExternalMsg;

// Mensagem a ser convertida em object json
    String data;

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
    timestamp = t;
    //Serial.printf("\nDEBUG timestampAdjust(int t) Timestamp received: %d\n",timestamp); // debug
    DateTime.setTime(timestamp);
}

// atualiza o tempo de envio das mensagens
void sendTimeAdjust(){    
    taskSendMessage.setInterval(TASK_SECOND*T_send);
    taskSendMessage.enable();
    Serial.printf("\nDEBUG sendTimeAdjust() T_send adjusted to %d seconds.\n",T_send); // debug
}

// habilita os pinos recebidos por parâmetro
void pinEnable(){
    for(int i = 0;i < PINS_NUM-1;++i){
        uint8_t pin = pinDef[i].pinNum;
        if(pinDef[i].pinSet == true){
            pinMode(pin, INPUT);
            Serial.printf("\nDEBUG Pin %d input mode defined.\n",pin); // debug
        }
        else{
            Serial.printf("\nDEBUG Pin %d don't used.\n",pin); // debug
        }
    }
}

// testa se a chave json existe e atribui à respectiva variável global
bool getParameters(){    
    Serial.printf("\nDEBUG getParameters() n: %s.......\n",data.c_str()); // debug
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, data);
    JsonObject receivedJsonData = doc.as<JsonObject>();
    if(receivedJsonData.containsKey("nodeToSend")){
        uint32_t n = receivedJsonData["nodeToSend"];
        nodeToSend = n;
        //Serial.printf("\nDEBUG getParameters() nodeToSend: %u.......\n",nodeToSend); // debug
        //Serial.printf("\nDEBUG getParameters() n: %u.......\n",n); // debug
    }
    if(nodeOrigin == nodeToSend){
        if(receivedJsonData.containsKey("send")){
            meshSend = receivedJsonData["send"];
            //Serial.printf("\nDEBUG getParameters() send/meshSend: %d.......\n",meshSend); // debug
        }
        if(receivedJsonData.containsKey("type")){
            sendType = receivedJsonData["type"];
            //Serial.printf("\nDEBUG getParameters() type/sendType: %d.......\n",sendType); // debug
        }
        if(receivedJsonData.containsKey("timestamp")){
            unsigned long t = receivedJsonData["timestamp"];
            //Serial.printf("\nDEBUG getParameters() timestamp: %lu.......\n",t); // debug
            timestampAdjust(t);
        }
        if(receivedJsonData.containsKey("t_send")){
            T_send = receivedJsonData["t_send"];
            Serial.printf("\nDEBUG getParameters() t_send/T_send: %d.......\n",T_send); // debug
            sendTimeAdjust();
        }
        if(receivedJsonData.containsKey("pinDef")){
            if(nodeToSend == nodeOrigin){
                for(int i=0;i<PINS_NUM;++i){
                    bool v = receivedJsonData["pinDef"][i];
                    pinDef[i].pinSet = v;
                    //Serial.printf("\nDEBUG pinDef[%d].pinSet: %d\n",i,pinDef[i].pinSet); // debug
                }
                pinEnable();
            }
        }
        if(receivedJsonData.containsKey("node_master")){
            uint32_t node_master = receivedJsonData["node_master"];
            sendType = 3;
            meshSend = true;
            NODE_MASTER = node_master;
            Serial.printf("\nDEBUG New node master id: %u.......\n",node_master); // debug
        }
        return true;
    }
    else{
        return false;
    }
}

// Dispara quando uma mensagem é recebida
void receivedCallback(uint32_t from, String &msg){
    //Serial.printf("\nDEBUG RECEIVED from %u meshMsg=%s\n", from, msg.c_str()); // debug
    Serial.printf("\nDEBUG RECEIVED receivedCallback() %s\n: ",msg.c_str()); // debug
    // data = msg.c_str();
    data = msg;
    if(nodeOrigin == NODE_MASTER){
        //Serial.printf("\nDEBUG RECEIVED if_1 %s\n",msg.c_str()); \\ debug
        Serial.println(msg);
    }
    else{
        Serial.println(msg); // debug
        Serial.println(data); // debug
        getParameters();
    }
    // if(nodeOrigin == nodeToSend){
    //     //Serial.printf("\nDEBUG RECEIVED  if_2 receivedCallback() %s\n: ",msg.c_str()); // debug
    //     getParameters();
    // }
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
    //mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT,WIFI_TYPE,WIFI_CHANNEL,WIFI_HIDE);
    //mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT,WIFI_AP_STA,1,1);
    mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionCallback);
    mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
    userScheduler.addTask( taskSendMessage );
    taskSendMessage.enable();
    nodeOrigin = mesh.getNodeId();
    nodeList = mesh.getNodeList();
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
}

// passagem dos dados para json
void jsonParse(){
    DynamicJsonDocument sendJsonData(1024);
    sendJsonData["device"] = nodeOrigin;
    sendJsonData["node_master"] = NODE_MASTER;
    sendJsonData["nodeToSend"] = NODE_MASTER;
    sendJsonData["nodeTime"] = mesh.getNodeTime();
    sendJsonData["timestamp"] = timestamp;
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

// Função de envio de mensagens
void sendMessage(){
    jsonParse();
    //Serial.printf("\nDEBUG sendMessage() meshMsg: %s\n",meshMsg); // debug

// dividir a function de envio de msgs, sendMeshMsg()?
// 	sendMessage() -> cria o json e envia para a mesh ou serial
// 	 |----- sendMesh() se eu!=node_master | eu!=nodeToSend (OU: se a msg não for para mim, eu!=nodeToSend)
// 	 |----- sendSerial() se eu for o node_master e a mensagem não for para outro node, eu==nodeToSend

    if(nodeOrigin == NODE_MASTER){
        //Serial.printf("\nDEBUG sendMessage() if meshMsg: %s\n",meshMsg); // debug
        if(nodeToSend == NODE_MASTER){
            if(sendType == 1){
                Serial.println(meshMsg);
            }
        }
        else{
            if(meshSend){
                if(sendType == 3){
                    //Serial.printf("\nDEBUG sendMessage() BROADCAST\n"); // debug
                    mesh.sendBroadcast(meshExternalMsg);
                }
                int len = nodeToSend?0:1;
                while (nodeToSend) {len++; nodeToSend/=10;}
                if(sendType == 2 && len == 10){
                    //Serial.printf("\nDEBUG sendMessage() SINGLE\n"); // debug
                    returnSendSingle = mesh.sendSingle(nodeToSend,meshExternalMsg);
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
                meshExternalMsg = "";
                meshSend = false;
                sendType = 1;
            }
        }
    }
    else{
        Serial.printf("\nDEBUG sendMessage() else meshMsg: %s\n",meshMsg); // debug
        //Serial.printf("\nDEBUG sendMessage() else len(meshMsg): %u\n",strlen(meshMsg)); // debug
        if(strlen(meshMsg) > 0){
            mesh.sendSingle(NODE_MASTER, meshMsg);
        }
    }
}

// lê a serial - apenas para node master
void readSerialData(){
    if(Serial.available() > 0) {
        String jsonRec = Serial.readString();
        // DynamicJsonDocument doc(1024);
        // deserializeJson(doc, jsonRec);
        // JsonObject receivedJsonData = doc.as<JsonObject>();
        
        //Serial.println("\nDEBUG readSerialData() jsonRec: "+jsonRec); // debug
        
        // if(receivedJsonData.containsKey("nodeToSend")){
        //     nodeToSend = receivedJsonData["nodeToSend"];
            //uint32_t n = receivedJsonData["nodeToSend"];
            //nodeToSend = n;

            //Serial.println(n); // debug
            //Serial.println(nodeOrigin); // debug
            //Serial.printf("\nDEBUG readSerialData() n: %u\n",n); // debug
            //Serial.printf("\nDEBUG readSerialData() nodeOrigin: %u\n",nodeOrigin); // debug

            //if(nodeToSend == nodeOrigin){
            if(getParameters()){
                //Serial.printf("\nDEBUG readSerialData() if: %u == %u, n: %u, nodeOrigin: %u\n",n,nodeOrigin,sizeof(n),sizeof(nodeOrigin)); // debug
                data = jsonRec;
                //getParameters();
            }
            else{
                meshExternalMsg = jsonRec;
                //Serial.printf("\nDEBUG readSerialData() meshMsg: %s,",meshMsg); // debug
            }
        //}
    }
}

void sendSerial(){
    if(NODE_MASTER != nodeOrigin){
        int i = 0;
        String firstMsg;
        DynamicJsonDocument firstJson(1024);
        SimpleList<uint32_t>::iterator node = nodeList.begin();
        while (node != nodeList.end()){
            Serial.printf(" %u", *node); // debug
            firstJson["listaNodes"][i];
            node++;
            i++;
        }

        // std::list<uint32_t> nodeList = mesh.getNodeList();
        // int t = nodeList.size();
        // for(int i=0;i<t;++i){
        //     firstJson["listaNodes"][i] = nodeList;
        // }
        firstJson["id_node"] = nodeOrigin;
        serializeJson(firstJson, firstMsg);
        if(c == 5000){
            //Serial.printf("\nNum nodes: %d\n", nodeList.size()); // debug
            //Serial.printf("\nConnection list:"); // debug
            //SimpleList<uint32_t>::iterator node = nodeList.begin();
            //while (node != nodeList.end()){
                //Serial.printf(" %u", *node); // debug
            //    node++;
            //}
            //Serial.printf("\n");
            //Serial.printf("{\"id_node\":%u}",nodeOrigin);
            Serial.println(firstMsg);
            c = 0;
        }
        c += 1;
    }
}

// config
void setup(){
    Serial.begin(115200);
    DateTime.setTimeZone("America/Bahia");
    DateTime.begin(/* timeout param */);
    timestamp = DateTime.now();
    meshInit();
    Serial.printf("{\"id_node\":%u}",nodeOrigin);
    pinEnable();
}

// Loop
void loop(){
    mesh.update();
    readSerialData();
    sendSerial();
}