# 🌱 Estação Inteligente de Monitoramento Ambiental (Franzininho WiFi)

Simulação de sistema embarcado desenvolvido com **Franzininho WiFi (ESP32)** para monitoramento ambiental em tempo real, com integração IoT via **MQTT (Adafruit IO)**, interface local com **OLED**, controle por botões e armazenamento de logs.

---

## 📌 Visão Geral

Este projeto simula uma estação inteligente capaz de:

* Monitorar **temperatura**, **umidade** e **luminosidade**
* Exibir dados em um display OLED
* Gerenciar limites configuráveis pelo usuário
* Detectar condições de alarme
* Enviar dados para a nuvem (Adafruit IO)
* Registrar eventos em memória (LittleFS)
* Permitir interação local via botões

---

## ⚙️ Tecnologias Utilizadas

* ESP32 (Franzininho WiFi)
* Arduino Framework
* MQTT (Adafruit IO)
* Wokwi Simulator
* FreeRTOS (conceitos aplicados)
* LittleFS (armazenamento local)
* Preferences (persistência de configuração)

---

## 🧰 Componentes

### 🔌 Hardware (simulado no Wokwi)

![Image](https://docs.franzininho.com.br/assets/images/franzininho-wifi-new-726cd0edc1ae88b9f789475b6d5797de.jpg)

![Image](https://diyables.io/images/products/dht22-module-1.jpg)

![Image](https://cdn.awsli.com.br/600x700/468/468162/produto/61767182/15c70d9108-mmz9x81eyv.jpg)

![Image](https://hobbycomponents.com/1559-large_default/ssd1306-128x64-pixel-oled-display-module-white.jpg)

![Image](https://nettigo.eu/system/images/3580/original.JPG?1578141142=)

![Image](https://www.winstar.com.tw/uploads/photos/graphic-oled-display/WEA012864DB-01.jpg)

![Image](https://electropeak.com/media/catalog/product/cache/a99a51fafac039a73087ecfaa8ccceba/l/c/lcd-01-125-1-0-96-inch-i2c-oled-display.jpg)

* Franzininho WiFi (ESP32)
* Sensor DHT22 (temperatura e umidade)
* Sensor LDR (luminosidade)
* Display OLED SSD1306 (I2C)
* LED RGB
* Buzzer
* 3 Botões (interface do usuário)

---

## 🔗 Estrutura do Projeto

```
📁 projeto-final/
├── sketch.ino          # Código principal
├── diagram.json        # Circuito Wokwi
├── libraries.txt       # Bibliotecas utilizadas
├── README.md           # Documentação
```

---

## 📚 Bibliotecas Utilizadas

Conforme definido em `libraries.txt` :

* PubSubClient
* Adafruit GFX
* Adafruit SSD1306
* DHT sensor library for ESPx

---

## 🚀 Funcionalidades

### 📊 Monitoramento

* Temperatura (°C)
* Umidade (%)
* Luminosidade (%)

### 📟 Interface OLED

* Exibição dos sensores
* Status de WiFi e MQTT
* Limites configurados
* Indicação de alarmes

### 🔔 Sistema de Alarmes

* Disparado quando valores ultrapassam limites
* Ativa buzzer e mensagem visual
* Registro em log interno
* Envio para MQTT

### 🌐 Integração IoT

* Publicação de dados no Adafruit IO:

  * temperatura
  * umidade
  * luminosidade
* Recebimento de comandos para LED RGB

### 💾 Armazenamento

* Logs salvos em `LittleFS`
* Configurações persistidas com `Preferences`

### 🎛️ Interação com Botões

| Botão | Função                              |
| ----- | ----------------------------------- |
| BT1   | Menu / Confirmar / Silenciar buzzer |
| BT2   | Incrementar                         |
| BT3   | Decrementar                         |

---

## ☁️ Configuração do MQTT (Adafruit IO)

Antes de executar, configure no código:

```cpp
const char* AIO_USERNAME = "SEU_USUARIO";
const char* AIO_KEY      = "SUA_AIO_KEY";
```

### 📡 Broker MQTT

* Host: `io.adafruit.com`
* Porta: `1883`

### 📌 Feeds utilizados

* `temperatura`
* `umidade`
* `luminosidade`
* `alarmes`
* `led-r`
* `led-g`
* `led-b`

---
