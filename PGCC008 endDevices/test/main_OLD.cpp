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

// número de pinos definidos no array pinDef
    #define   PINS_NUM          18

    #define   pinA0             A0

// contagem de teste para impressao na tela
    int c = 0;

// message type: broadcast=3, nodes list=2, to node master=1
    uint8_t sendType  = 3;

// list of nodes to send msg
    uint32_t meshList[4] = {0,0,0,0};

// this node id
    uint32_t node_id = 0;

// node id to sendSingle
    uint32_t nodeDestination = 0;

// retorno do send single
    boolean returnSendSingle = false;

// conta o número de tentativas de enviar uma msg
    int countTries = 0;

// indice de array
    unsigned int j = 0;

// Se true, envia msg do sink recebida pela serial para a mesh
    boolean meshSend = false;

// id do nodeMcu centralizador (Sink received parameter)
    uint32_t NODE_MASTER = 0;

// Inicializa os sensores (Sink received parameter)
    typedef struct {
        const uint8_t pinNum;
        bool pinSet;
    } pinInit_t;
    pinInit_t pinDef[] {
        {pinA0, true},    //A0
        {18, false},  //GPIO00
        {22, false},  //GPIO01
        {17, false},  //GPIO02
        {21, false},  //GPIO03
        {19, false},  //GPIO04
        {20, false},  //GPIO05
        {14, false},  //GPIO06
        {10, false},  //GPIO07
        {13, false},  //GPIO08
        {9, false},   //GPIO09
        {12, false},  //GPIO10
        {11, false},  //GPIO11
        {6, false},   //GPIO12
        {7, false},   //GPIO13
        {5, false},   //GPIO14
        {16, false},  //GPIO15
        {4, false}    //GPIO16
    };

// Inicializa o array de dados dos sensores
    float pinData[PINS_NUM] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

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

// Define a tarefa de enviar mensagens e o tempo
    Task taskSendMessage(TASK_SECOND*T_sensor,TASK_FOREVER,&sendMessage);

// ----------------------------------------------------------------------------
// Functions
// ----------------------------------------------------------------------------

// Dispara quando uma mensagem é recebida
void receivedCallback(uint32_t from, String &msg){
    DynamicJsonDocument receivedJson(1024);
    deserializeJson(receivedJson, msg);
    // se a mensagem foi recebida do Sink, realize a atualização das configurações dos parâmetros recebidos
    // recebe pin_def e passa as configurações para pinDef[] ?? aparentemente, não funciona, pois pinDef[] precisa estar no setup
    boolean pins = receivedJson["pinDef"];
    if(sizeof(pins) > 0){
        uint8_t i = 0;
        for(i = 0;i < PINS_NUM-1;++i){
            pinDef[i].pinSet = pins[&i];
            //pinDef[i].pinNum;
        }
        // chamar setup(); para fazer nova configuração?
    }
    NODE_MASTER = receivedJson["node_master"];
    if(node_id == NODE_MASTER){
        Serial.println(msg);
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
    node_id = mesh.getNodeId();
}

// Leitura dos sensores
void readSensors(){
    uint8_t i;
    for(i = 0;i < PINS_NUM-1;++i){
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

// Função de envio de mensagens
void sendMessage(){
    uint8_t i = 0;
    String msg = "";
    int timestamp = DateTime.now();
    DynamicJsonDocument jsonData(1024);
    jsonData["device"] = node_id;
    jsonData["node_master"] = NODE_MASTER;
    jsonData["node_list"] = NODE_MASTER; // deve ser o array de nodes
    jsonData["nodeTime"] = mesh.getNodeTime();
    jsonData["timestamp"] = timestamp;
    jsonData["latitude"] = latitude;
    jsonData["longitude"] = longitude;
    readSensors();
    for(i=0;i<PINS_NUM-1;++i){
        if(pinDef[i].pinSet == true){
            jsonData["pin"][i] = pinDef[i].pinNum;
            jsonData["val"][i] = pinData[i];
        }
        else{
            jsonData["pin"][i] = 0;
            jsonData["val"][i] = 0;
        }
    }
    serializeJson(jsonData, msg);
    
    Serial.println("-------> This endDevice: "+msg);

    if(node_id == NODE_MASTER){
        if(meshSend){
            if(sendType == 3){
                mesh.sendBroadcast(msg);
                sendType = 1;
            }
            if(sendType == 2 and j < sizeof(meshList)){
                returnSendSingle = mesh.sendSingle(meshList[j],msg);
                if(returnSendSingle){
                    countTries = 0;
                    j += 1;
                }
                else{
                    countTries += 1;
                    if(countTries >= 5){
                        countTries = 0;
                        j += 1;
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
        DynamicJsonDocument docSerialRec(1024);
        deserializeJson(docSerialRec, jsonRec);
        uint32_t node_master = docSerialRec["node_master"];
        meshSend = docSerialRec["send"];
        sendType = docSerialRec["type"];
        uint32_t l = docSerialRec["nodeslist"];
        unsigned long t = docSerialRec["timestamp"];
        DateTime.setTime(t);
        T_sensor = docSerialRec["t_sensor"];
        if(sizeof(l) > 0 and sendType == 2){
            j = 0;
            unsigned int i = 0;
            for(i=0;i<sizeof(l);++i){
                meshList[i] = l[&i];
            }
        }
        if(node_master > 99){
            NODE_MASTER = node_master;
            sendType = 3;
            meshSend = true;
        }
    }
    else{
        if(NODE_MASTER != node_id){
            c += 1;
            if(c == 4000){
                Serial.println(node_id);
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
    Serial.println(node_id);
    int i = 0;
    for(i = 0;i < PINS_NUM-1;++i){
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