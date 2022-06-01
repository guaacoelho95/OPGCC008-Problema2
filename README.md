# PGCC008 - Sistemas Computacionais 2022-1 <br>Atividade #2: Detecção de Emergências (rede mesh)

## Descrição:
<!-- ## Description: -->
<ul>
   <li>
      <a href="https://github.com/guaacoelho95/OPGCC008-Problema2/blob/main/job_description.pdf">job_description.pdf</a>
   </li>
</ul>

## Diagramas do projeto:
<!-- ## Project diagrams: -->
<p>
   <img src="https://github.com/guaacoelho95/OPGCC008-Problema2/blob/main/diagrams/PGCC008_Atividade-2_Diagrams-diagrama%20estrutural.drawio.png">
</p>
<p>
   <img width="960px" src="https://github.com/guaacoelho95/OPGCC008-Problema2/blob/main/diagrams/PGCC008_Atividade-2_Diagrams-diagrama%20f%C3%ADsico.drawio.png">
</p>


## NodeMCU ESP8266 v3:
### Códigos
<!-- ### Source codes -->
<ul>
    <li>
       <a href="https://github.com/guaacoelho95/OPGCC008-Problema2/tree/main/PGCC008%20endDevices">
         Código fonte genérico (para todos os endDevices da rede).
       </a>
    </li>
    <li>
       <a href="https://github.com/guaacoelho95/OPGCC008-Problema2/blob/main/PGCC008%20endDevices/src/rasp-server.py">
         Código do servidor implementado no Raspberry Pi.
       </a>
   </li>
</ul>

### Configuração
<ol>
    <li>      
      Para desenvolver e carregar o código fonte no nodeMCU ESP8266 foi usado o Visual Studio Code com a extensão <a href="https://platformio.org/">PlatformIO</a>.
   </li>
    <li>      
       Para usar o código na IDE do Arduino, utilize o arquivo <a href="https://github.com/guaacoelho95/OPGCC008-Problema2/blob/main/PGCC008%20endDevices/src/main.cpp">main.cpp</a> mudando a extensão para <b>.ino</b> e retirando a biblioteca <b>Arduino.h</b>.
   </li>
   <li>
      Você pode precisar instalar o driver ch340: <a href="https://github.com/angeload/pgcc008_2022-1_Probl1/tree/main/drivers">download aqui</a> (linux - testado em debian) ou <a href="https://github.com/angeload/pgcc008_2022-1_Probl1/blob/main/drivers/CH341SER_windows.zip">aqui</a> (windows).
   </li>
   <li>
      Tutorial para usar o ESP8266 na PlatformIO Vs Code <a href="https://www.youtube.com/watch?v=0poh_2rBq7E">aqui</a>
   </li>
   <li>
      Tutorial para usar o ESP8266 na IDE Arduino <a href="https://github.com/angeload/pgcc008_2022-1_Probl1/blob/main/tutorials/nodeMcu_on_Arduino_IDE.md">aqui</a>
   </li>
</ol>

### Descrição da implementação

<p>
   Inicialmente é definido um nodeMCU para funcionar como node master, sendo responsável por obter todas as mensagens provenientes dos outros Nodes e enviar, via comunicação serial, para o Raspberry Pi. Assim que o NodeMCU master se conecta via porta serial com o raspberry, ele começa a enviar pacote de dados contendo se id e a lista de nodes. Assim que o Raspberry recebe ele retorna uma mensagem que deve ser repassada para os outros nós. Essa mensagem contém:
   <ul>
    <li>      
       timestamp: Para que os nodes sincronizem seu timestamp com o do Rasberry
   </li>
    <li>
       id do nó master: Para que os outros nós saibam para quem enviar as informações obtidas pelos sensores
   </li>
   <li>
      tempo de envio das mensagens
   </li>
   <li>
      lista de ativação de pinos: Uma lista com 18 campos (um pra cada pino do NodeMCU) onde somente os que representam pinos com sensroes conectados a eles receberão valor 1. Essa definição faz que o nodeMCU pegue as informações apenas dos pinos com sensores, além de permitir uma possívelalteração na disposição dos mesmos.
   </li>
</ul>
</p>   

<p>
   Feito isso todos os nós ajustam seus parâmetros e começam imediatamente a enviar seus pacotes de dados para o nodeMaster, que por sua vez irá enviar, via porta serial, para o Raspberry PI. O Raspberry PI ao receber as mensagens de dados armazena os valores captados em uma estrutura de dados, de modo que seja possível processa-los se desejado. Além desse armazenamento, também é realizado uma verificação de valores anormais, a detecção ou não desses valores também é armazenada na estrutura de dados citada anteriormente.
   
   Além disso, a cada 15 mensagens recebidas o Raspberry realiza um processo de backup, salvando a estrutura de dados em um arquivo binário.
</p>
