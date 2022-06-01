# -*- coding: utf-8 -*-

import socket
import pickle
import numpy as np
import json
import serial

import time
from datetime import datetime

pin_dht11 = 3
pin_bmp180 = [5, 6]
pin_bmp280 = [5, 6]
pin_uv = 0
pin_chama = 15

n_nodes = 4
node_list = [4208803281, 4208793911, 4208790561, 4208779354]


# Recebe a lista de nós de entrada e atribui para cada um deles quais pinos serão e quais não deve ser ativados
def define_pinTable():
    '''
    Recebe a lista de ids dos nodes e os armazena, ao mesmo tempo que define a pinagem
    para cada nó. Definindo qual nó será ativado para cada node na rede mesh.
    '''
    def inicializar_pinos():
        '''
        Define os pinos que serão ativados em cada um dos nodes
        '''

        # Definir como True todos os pinos que serão utilizados.
        tp = np.zeros([n_nodes, 18])
        tp = tp.tolist()

        # Node com temperatura, humidade, chama e uv
        tp[0][pin_dht11] = 1
        tp[0][pin_uv] = 1
        tp[0][pin_chama] = 1

        # Node com temperatura humidade e chama
        tp[1][pin_dht11] = 1
        tp[1][pin_chama] = 1

        # Node com temperatura humidade e uv
        tp[2][pin_bmp180[0]] = 1
        tp[2][pin_bmp180[1]] = 1
        tp[2][pin_uv] = 1

        # Node com temperatura, humidade, uv e pressão
        tp[3][pin_bmp280[0]] = 1
        tp[3][pin_bmp280[1]] = 1
        tp[3][pin_uv] = 1

        return tp

    id_nodes = [l for l in node_list]
    tp = inicializar_pinos()
    pin_table = {l:tp[ii][:] for ii, l in enumerate(id_nodes)}
    return pin_table


def send_setup_signal(data_in, connection):
    '''
    Envia mensage para o node conectado a porta serial avisando que ele é o nó master, além
    de enviar informações importantes de setup como pinagem e timestamp
    '''

    timestamp = int(time.time())

    pin_table = define_pinTable()

    data = {}
    data["node_master"] = data_in["id_node"]
    data["send"] = True
    data["send_type"] = 3
    data["timestamp"] = timestamp
    data = json.dumps(data)
    connection.write(data.encode('ascii'))
    connection.flush()
    time.sleep(0.5)
    #print(data)
    for n in node_list:
        data = {}
        data["nodeDestiny"] = n
        data["node_master"] = data_in["id_node"]
        data["t_send"] = 4
        data["send"] = True
        data["send_type"] = 3
        data["timestamp"] = timestamp
        data["pinDef"] = pin_table[n]
        data=json.dumps(data)
        print(data)
        connection.write(data.encode('ascii'))
        connection.flush()
        time.sleep(0.5)

    # print(data)
    return pin_table

def changeFrequency(id, frequence, connection):

    period = 5 # período para que os sensores enviem os dados, em segundos
    data = {}
    data["node_master"] = "id"
    data["send"] = True
    data["send_type"] = 2
    data["nodeDestiny"] = node_list[0] #como saber o id do node que eu quero enviar
    data["t_send"] = period

    data=json.dumps(data)
    
    connection.write(data.encode('ascii'))
    connection.flush()

    return

def verifica_anomalia(data):
    '''
    Verifica a existência de anomalias nos sinais recebidos pelos sensores
    '''
    temperatura = database["temperature"][-1]
    chama = database["flame"][-1]
    uv = database["uv"][-1]

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
    '''
    Realiza a leitura dos dados recebidos pela porta serial e armazena na estrutura de dados
    contendo todos os valores medidos, além das outras informações relevantes como latitude e longitude.
    '''
    database["id"].append(data["device"])
    database["timestamp"].append(data["timestamp"])
    database["latitude"].append(data["latitude"])
    database["longitude"].append(data["longitude"])
    
    sensors = ['temperature', 'humidity', 'flame', 'uv', 'pression']
    for s in sensors:
        if s in data.keys():
            database[s].append(data[s])
        else: 
            database[s].append(None)
    verifica_anomalia(data)
    print(database)
    return



# Dicionário de dados contendo todas as informações recebidas pelo Rasp, desde os valores dos sensores (None caso não estejam presentes no pacote),
# até a indnicação de ocorrência ou não de anomalias como incendios, alta radiação, onda de calor, entre outros
database = {"id":[], "timestamp": [], "latitude":[], "longitude":[], "uv": [], "temperature": [], "flame": [], "humidity": [], "pression": [],
        "datahora":[], "alta_rad":[], "mto_alta_rad":[], "calor":[], "susp_incendio":[], "prin_incendio":[], "incendio":[]}

connection = serial.Serial(port="/dev/ttyUSB0", baudrate=115200)
connection.reset_input_buffer()

cont = 1
n_bkup = 15
while(True):

        msg = connection.readline().decode("utf-8")
        connection.reset_input_buffer()
        data = json.loads(msg)
        print(data)
        if "id_node" in  data.keys():
            pin = send_setup_signal(data, connection)
        else:
            leitura_dados(data)
            cont = cont + 1

        # A nada n_bkup mensagens recebidas é realizado o backup dos dados
        if(cont % n_bkup == 0):
                print("Starting backup")
                pickle.dump(data, open("savingData.p", "wb"))
                print("Backup finished")
        

