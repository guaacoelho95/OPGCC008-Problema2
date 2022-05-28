import socket
import pickle
import numpy as np
import json
import serial

import time



#{"id_node":4208803281,"nodeList":[4208793911, 4208790561,4208779354,]}

pin_temperatura = 1
pin_humidade = 3
pin_chama = 5
pin_uv = 6
pin_pressao = 14

# Sempre ter o nó que vai enviar, o send (pra saber se envia para outros nós) é False se é só pro nó master,
# type: 2 single e 3 broadcast (só pra avisar que é o master).

# Na mensagem da definição do master enviar o id do master e o timestamp.

#t_sensor = float em segundos

# É possível alterar o tempo de envio de cada nó específico na rede? 
# Ideia: Se nao acessar o sink, entao envia uma mensagem com a identificação de que aquela mensagem é pra ser reenviada para o sinc

# Vou receber uma lista de pinos e outra lista com os valores daquele pino.

# Como um dos primeiros passos: enviar os pinos que serão utilizados para leitura: lista com todos os pinos, mas com True para os 
# que serão utilizados (17 pinos)

from datetime import datetime

n_nodes = 4
tabela_pinos = np.zeros([n_nodes, 18])


node_list = [4208803281, 4208793911, 4208790561,4208779354]
# Mapa que relacione um index com o valor do id de um nó (id definido pela rede mesh)


# Recebe a lista de nós de entrada e atribui para cada um deles quais pinos serão e quais não deve ser ativados
def receber_ids(lista):
    '''
    Recebe a lista de ids dos nodes e os armazena, ao mesmo tempo que define a pinagem
    para cada nó. Definindo qual nó será ativado para cada node na rede mesh.
    '''
    def inicializar_pinos():
        '''
        Lista de pinos e respectivos sensores:
        1 - Temperatura
        3 - Humidade
        5 - Chama
        6 - UV
        14 - Pressão

        Os números representam o índice do pino na lista de pinos disponíveis
        '''

        # Definir como True todos os pinos que serão utilizados.
        tp = np.zeros([n_nodes, 18])
        tp = tp.tolist()
        # Node com temperatura, humidade, chama e uv
        tp[0][pin_temperatura] = 1
        tp[0][pin_humidade] = 1
        tp[0][pin_chama] = 1
        tp[0][pin_uv] = 1

        # Node com temperatura humidade e chama
        tp[1][pin_temperatura] = 1
        tp[1][pin_humidade] = 1
        tp[1][pin_chama] = 1

        # Node com temperatura humidade e uv
        tp[2][pin_temperatura] = 1
        tp[2][pin_humidade] = 1
        tp[2][pin_uv] = 1

        # Node com temperatura, humidade, uv e pressão
        tp[3][pin_temperatura] = 1
        tp[3][pin_humidade] = 1
        tp[3][pin_uv] = 1
        tp[3][pin_pressao] = 1

        return tp

    id_nodes = [l for l in lista]
    tp = inicializar_pinos()
    pin_table = {l:tp[ii][:] for ii, l in enumerate(id_nodes)}
    return id_nodes, pin_table


def receive_setup_signal(data_in, connection):
    '''
    Envia mensage para o node conectado a porta serial avisando que ele é o nó master, além
    de enviar informações importantes de setup como pinagem e timestamp
    '''

    timestamp = int(time.time())

    id_nodes, pin_table = receber_ids(data_in["node_list"])

    data = {}
    data["node_master"] = data_in["id_master"]
    data["send"] = True
    data["send_type"] = 3
    data["timestamp"] = timestamp
    data["pinDef"] = pin_table
    data=json.dumps(data)
    
    # connection.write(data.encode('ascii'))
    # connection.flush()
    print(data)
    return id_nodes

def changeFrequency(id, frequence, connection):

    period = 5 # período para que os sensores enviem os dados, em segundos
    data = {}
    data["node_master"] = "id"
    data["send"] = True
    data["send_type"] = 2
    data["nodeToSend"] = 0 #como saber o id do node que eu quero enviar
    data["t_send"] = period

    data=json.dumps(data)
    
    connection.write(data.encode('ascii'))
    connection.flush()

    return

def verifica_anomalia(pinData):

    temperatura = pinData[pin_temperatura]
    chama = pinData[pin_chama]
    uv = pinData[pin_uv]

    # Se ocorrer muito alta radiação devo considerar como alta radiação tambem ou serão excludentes?

    if uv is None or uv <= 5:
        database["alta_rad"].append(0)
        database["mto_alta_rad"].append(0)
    else: #nesse else o uv ja é maior que 5
        if uv > 7:
            database["mto_alta_rad"].append(1)
            database["alta_rad"].append(0)
        else:
            database["alta_rad"].append(1)
            database["mto_alta_rad"].append(0)

    if temperatura is None or temperatura <= 35:
        if chama:
            database["prin_incendio"].append(1)
        else: database["prin_incendio"].append(0)
        database["calor"].append(0)
        database["susp_incendio"].append(0)
        database["incendio"].append(0)
    else: # nesse else a temperatura ja é maior que 35
        if temperatura > 45: 
            if chama:
                print("Incêndio")
                database["calor"].append(0)
                database["susp_incendio"].append(0)
                database["incendio"].append(1)
                database["prin_incendio"].append(0)
            else:
                print("Suspeita de incêndio")
                database["calor"].append(0)
                database["susp_incendio"].append(1)
                database["incendio"].append(0)
                database["prin_incendio"].append(0)
        else:
            if chama:
                database["prin_incendio"].append(1)
            else: 
                database["prin_incendio"].append(0) 
            print("onda de calor")
            database["calor"].append(1)
            database["susp_incendio"].append(0)
            database["incendio"].append(0)
                       
    return

def leitura_dados(data):

    def sensor_data(data):
        database["temperatura"].append(data["pinData"][pin_temperatura])
        database["humidade"].append(data["pinData"][pin_humidade])
        if data["device"] == id_nodes[0]:
            # ideia: chamar o verifica anomalia dentro dos if, passando só os valores
            # que vão ser usados.
            database["chama"].append(data["pinData"][pin_chama])
            database["uv"].append(data["pinData"][pin_uv])
            database["pressao"].append(None)

        elif data["device"] == id_nodes[1]:
            database["chama"].append(data["pinData"][pin_chama])
            database["uv"].append(None)
            database["pressao"].append(None)

        elif data["device"] == id_nodes[2]:
            database["chama"].append(None)
            database["uv"].append(data["pinData"][pin_uv])
            database["pressao"].append(None)

        elif data["device"] == id_nodes[3]:
            database["chama"].append(None)
            database["uv"].append(data["pinData"][pin_uv])
            database["pressao"].append(data["pinData"][pin_pressao])

        return 
    database["id"].append(data["device"])
    database["timestamp"].append(data["timestamp"])
    database["latitude"].append(data["latitude"])
    database["longitude"].append(data["longitude"])
    sensor_data(data)
    verifica_anomalia(data["pinData"])
    print(database)
    return



# Dicionário de dados contendo todas as informações recebidas pelo Rasp, desde os valores dos sensores (None caso não estejam presentes no pacote),
# até a indnicação de ocorrência ou não de anomalias como incendios, alta radiação, onda de calor, entre outros
database = {"id":[], "timestamp": [], "latitude":[], "longitude":[], "uv": [], "temperatura": [], "chama": [], "humidade": [], "pressao": [],
        "datahora":[], "alta_rad":[], "mto_alta_rad":[], "calor":[], "susp_incendio":[], "prin_incendio":[], "incendio":[]}

# connection = serial.Serial(port="COM3", baudrate=9600)
# connection.reset_input_buffer()

# while(True):

#         msg = connection.readline().decode("utf-8")
#         print(f"data: {msg}")
        
#         data = json.loads(msg)
data = {}
# data["id_master"] = 456
# data["node_list"] = [123, 456, 789, 000]

data["device"] = 789
data["node_master"] = 456
data["nodeToSend"] = 456
data["nodeTime"] = 6666666 #o que é esse node time?
data["timestamp"] = 6666666
data["latitude"] = 13
data["longitude"] = 13.5
data["pinData"] = [0, 36, 0, 5, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
connection = 2
id_nodes = [123, 456, 789, 000]

if "id_master" in  data.keys():
    id_nodes = receive_setup_signal(data, connection)
else:
    leitura_dados(data)
    leitura_dados(data)
            
