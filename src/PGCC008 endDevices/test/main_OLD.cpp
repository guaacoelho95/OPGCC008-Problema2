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
    #define   MESH_PREFIX     "pgcc008"
    #define   MESH_PASSWORD   "atividade2"
    #define   MESH_PORT       5555

// this node id
    uint32_t node_id = 0;

// ----------------------------------------------------------------------------
// id do nodeMcu centralizador (Sink received parameter)
    uint32_t NODE_MASTER = 0;
    
// Inicializa os sensores (Sink received parameter)
    typedef struct {
        uint8_t pinNum;
        bool pinSet;
    } pinInit_t;
    pinInit_t pinDef[] {
        {2, true},    //A0
        {22, true},   //GPIO01
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

// Localização deste endDevice
    float latitude = -12.197422000000014;
    float longitude = -38.96674600000001;

// Período de leitura de dados do sensor (Sink received parameter)
    float T_sensor = 1;
    
// Inicializa o array de dados dos sensores
    float pinData[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

// Inicializa a rede mesh
    painlessMesh  mesh;

// Define o scheduler em substituição ao método delay
    Scheduler userScheduler;

// Define a function para envio de mensagens da painlessmesh
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
    
    // se a mensagem foi endereçada a este endDevice, realize a atualização das configurações dos parâmetros recebidos
    Serial.printf("This endDevice id: %u jsonData: %s\n", from, msg.c_str());
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

// // Leitura dos sensores
void readSensors(){
    uint8_t i;
    int p = 0;  
    for(i = 0;i< sizeof(pinDef)/sizeof(pinInit_t);++i){
      if(pinDef[i].pinSet == true){
          pinMode(pinDef[i].pinNum, INPUT);
          if(i == 0){
              pinData[p] = analogRead(pinDef[i].pinNum);
          }
          else{
              pinData[p] = digitalRead(pinDef[i].pinNum); 
          }
          p += 1;
      }
    }
}

// Função de envio de mensagens
void sendMsg(int type = 1, String msg = ""){
   if(node_id == NODE_MASTER){
       if(type == 3){
           mesh.sendBroadcast(msg);
       }
       else{
           mesh.sendSingle(NODE_MASTER, msg); 
       }
       Serial.println(msg);
   }
   else{
       taskSendMessage.setInterval(random(TASK_SECOND*1,TASK_SECOND*5));
       mesh.sendSingle(NODE_MASTER, msg);
   }
}

// leitura dos dados da porta serial
void readSerialData(){
 if(Serial.available() > 0) {
   String jsonRec = Serial.readString();
   
   Serial.println(jsonRec);
   
   DynamicJsonDocument docSerialRec(1024);
   deserializeJson(docSerialRec, jsonRec);

   NODE_MASTER = docSerialRec["node_master"];
   if(NODE_MASTER == node_id){       
      sendMsg(3);
      Serial.println("I'm a new master node now!");
   }
   
   // verificar se deve ser criada uma nova function para concentrar os recebimentos de parâmetros do sink
   boolean meshSend  = docSerialRec["send"];       // true/false - o sink informa se a mensagem recebida pela serial deve ser repassada
   //uint8_t meshType  = docSerialRec["type"];       // broadcast=3, nodes list=2, to node master=1
   //uint32_t meshList = docSerialRec["nodeslist"];  // nodes list do send msg
   unsigned long t   = docSerialRec["timestamp"];  // Sink timestamp adjust
   DateTime.setTime(t);
   T_sensor = docSerialRec["t_sensor"];            // tempo de disparo das msgs
   if(meshSend){
       // retransmite mensagens para a rede mesh
   }
 }
}

// config
void setup(){
    Serial.begin(115200);
    DateTime.setTimeZone("America/Bahia");
    DateTime.begin(/* timeout param */);
    if (!DateTime.isTimeValid()){
        Serial.println("Timezone Server Error!");
    }
    meshInit();
    Serial.println(node_id);
}

// Loop
void loop(){
    mesh.update();
    readSerialData();
}
