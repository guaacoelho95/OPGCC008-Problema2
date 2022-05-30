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
   <img width="960px" src="https://github.com/angeload/pgcc008_2022-1_Probl1/blob/main/diagrams/PGCC008_Atividade-1_physical.png">
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
</ul>


### Configuração
<ol>
    <li>      
      Para desenvolver e carregar o código fonte no nodeMCU ESP8266 foi usado o Visual Studio Code com a extensão <a href="https://platformio.org/">PlatformIO</a>.
   </li>

    <li>      
       Para usar a IDE do Arduino, utilize o arquivo main.cpp mudando a extensão para <b>.ino</b> e retirando a biblioteca <b><Arduino.h></b>.
   </li>
   <li>
      Você pode precisar instalar o driver ch340: <a href="https://github.com/angeload/pgcc008_2022-1_Probl1/tree/main/drivers">download aqui</a> (linux - testado em debian) ou <a href="https://github.com/angeload/pgcc008_2022-1_Probl1/blob/main/drivers/CH341SER_windows.zip">aqui</a> (windows).
   </li>
   <li>
      Tutorial para instalar o ESP8266 na IDE Arduino <a href="https://github.com/angeload/pgcc008_2022-1_Probl1/blob/main/tutorials/nodeMcu_on_Arduino_IDE.md">aqui</a>
   </li>
</ol>
