/* 
 *  --------------------------------------------------------------------------
      Código: Generical endDevice (mesh)
      Componentes:
        - NodeMCU ESP8266 v3 CH340
        - Plug and Play Sensors
      Topologia: Mesh/TCP Broadcast      
      Versão 1.0
      Start: 29/Apr/2022
      Por: Noberto Maciel / Gustavo Coelho
      PGCC Uefs: PGCC008 - Sistemas Computacionais
 *  ---------------------------------------------------------------------------
*/
// ----------------------------------------------------------------------------
// Inclusão das bibliotecas
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

// ----------------------------------------------------------------------------
// Inicializa os sensores
    typedef struct {
        uint8_t pinNum;
        bool pinVal;
    } pinInit_t;

// Define o array de pinos e valores iniciais dos sensores        
    pinInit_t pinDef[] {
        {2, 0.0},   //A0
        {22, LOW},  //GPIO01
        {17, LOW},  //GPIO02
        {21, LOW},  //GPIO03
        {19, LOW},  //GPIO04
        {20, LOW},  //GPIO05
        {14, LOW},  //GPIO06
        {10, LOW},  //GPIO07
        {13, LOW},  //GPIO08
        {9, LOW},   //GPIO09
        {12, LOW},  //GPIO10
        {11, LOW},  //GPIO11
        {6, LOW},   //GPIO12
        {7, LOW},   //GPIO13
        {5, LOW},   //GPIO14
        {16, LOW},  //GPIO15
        {4, LOW}    //GPIO16
    };

// Inicializa o array de definição dos sensores
    boolean pinSet[17] = {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    
// Inicializa o array de dados dos sensores
    float pinData[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  
// Período de leitura de dados do sensor
    float T_sensor = 1;

// Localização deste endDevice
    float latitude = -12.197422000000014;
    float longitude = -38.96674600000001;

// Define a time zone
    char* timezone = "America/Bahia";

// Inicializa o o scheduler em substituição ao método delay
    Scheduler userScheduler;

// Inicializa a rede mesh
    painlessMesh  mesh;

// Define a function para envio de mensagens
    void sendMessage();

// Define a tarefa de enviar mensagens e o tempo
    Task taskSendMessage(TASK_SECOND*1,TASK_FOREVER,&sendMessage);


// ----------------------------------------------------------------------------
// Functions
// ----------------------------------------------------------------------------
void setup(){
    Serial.begin(115200);
    DateTime.setTimeZone(timezone);
    DateTime.begin(/* timeout param */);
    if (!DateTime.isTimeValid()){
      Serial.println("Failed to get time from server.");
    }
    mesh.setDebugMsgTypes( ERROR | STARTUP );
    mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionCallback);
    mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
    userScheduler.addTask( taskSendMessage );
    taskSendMessage.enable();
}

// Printa as mensagens recebidas
void receivedCallback(uint32_t from, String &msg){
    // aqui
    // se a mensagem foi recebida do Sink, realize a leitura das configurações
    // se a mensagem recebida foi de um outro endDevice, retransmita para por broadcast
    // checar se o sink perdeu o contato com o endDevice que enviou a mensagem
    DynamicJsonDocument receivedJson(1024);
    deserializeJson(receivedJson, msg);
//    Serial.printf(receivedJson["device"]);
//    Serial.printf(receivedJson["nodeTime"]);
//    Serial.printf(receivedJson["timestamp"]);
//    Serial.printf(receivedJson["latitude"]);
//    Serial.printf(receivedJson["longitude"]);
    
    Serial.printf("endDevice: %u jsonData: %s\n", from, msg.c_str());
}

// Inicializa a conexão
void newConnectionCallback(uint32_t nodeId){
    //Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
}

// Tenta mudar a conexão em caso de perda
void changedConnectionCallback(){
    //Serial.printf("Changed connections\n");
}

// Ajusta o tempo de resposta do endDevice
void nodeTimeAdjustedCallback(int32_t offset){
    //Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
}

// Função de envio de mensagens
void sendMessage(){
    int i = 0;
    String msg = "";
    int timestamp = DateTime.now();
    DynamicJsonDocument jsonData(1024);
    jsonData["device"] = mesh.getNodeId();
    jsonData["nodeTime"] = mesh.getNodeTime();
    jsonData["timestamp"] = timestamp;
    jsonData["latitude"] = latitude;
    jsonData["longitude"] = longitude;

    Serial.println(mesh.getNodeId());
    
    // ???? se existem mensagens recebidas de outro endDevice que não foi lida pelo sink, retransmita ????
    
    readSensors();
    for(i=0;i<17;++i){
        if(pinSet[i] == 1){
            jsonData["pin"][i] = pinDef[i].pinNum;
            jsonData["val"][i] = pinData[i];
        }
    }
    serializeJson(jsonData, msg);
    mesh.sendBroadcast(msg);
    taskSendMessage.setInterval(random(TASK_SECOND*1,TASK_SECOND*5));
}

// Leitura dos sensores
void readSensors(){
    uint8_t i;
    int p = 0;  
    for(i = 0;i< sizeof(pinDef)/sizeof(pinInit_t);++i){
      if(pinSet[i] == 1){
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

// Loop
void loop(){
    mesh.update();
}
