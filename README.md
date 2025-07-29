# 📈 **Datalogger de Movimento MPU6050 com SD Card**

Projeto desenvolvido para o programa **CEPEDI TIC37 - Embarcatech**, que implementa um datalogger de movimento completo e portátil. O sistema utiliza um Raspberry Pi Pico para capturar dados de 6 eixos de um sensor MPU6050, armazenando-os em um cartão SD. A interface do usuário é composta por botões, um display OLED e um LED RGB para controle e feedback em tempo real.

---

## 🔎 **Objetivos**

O objetivo central é criar um dispositivo autônomo capaz de registrar dados de movimento para análises posteriores. As metas específicas do projeto incluem:
* Realizar a interface com um sensor MPU6050 via I2C para obter dados de aceleração e giroscópio.
* Integrar um módulo de cartão SD utilizando a biblioteca FatFs para criar e escrever arquivos de dados em formato CSV.
* Desenvolver uma interface de usuário simples e eficaz, baseada em botões para controlar as operações de montagem do SD, gravação e leitura de dados.
* Fornecer feedback claro ao usuário sobre o status do sistema através de um display OLED e um LED RGB multicor.
* Implementar o uso de interrupções para uma resposta rápida aos comandos do usuário.

---

## 📊 **Video do Projeto:**
[https://drive.google.com/file/d/1VasZNuEhWiXib4sgYcmhZDbZ_ZuNSg1S/view?usp=drive_li
nk]

---

## 🛠️ **Tecnologias Utilizadas**

-   **Linguagem de Programação:** C / CMake
-   **Placas Microcontroladoras:**
    -   Raspberry Pi Pico
-   **Sistema Operacional / Bibliotecas:**
    -   Raspberry Pi Pico SDK
    -   FatFs (para sistema de arquivos do SD card)
-   **Sensores e Módulos:**
    -   MPU6050 (I2C)
    -   Módulo de Cartão SD (SPI)
    -   SSD1306 OLED Display (I2C)
-   **Atuadores:**
    -   LED RGB
    -   Buzzer (PWM)

---

## 📖 **Como Utilizar**

1.  **Montagem:** Certifique-se de que um cartão SD formatado em FAT32 esteja inserido no módulo.
2.  **Ligar:** Energize o Raspberry Pi Pico.
3.  **Montar o SD Card:** Pressione o **Botão B** (GPIO 6). O LED piscará amarelo e ficará **verde** se a montagem for bem-sucedida.
4.  **Iniciar Gravação:** Pressione o **Botão A** (GPIO 5). O LED ficará **vermelho**, o buzzer soará intermitentemente, e o sistema gravará 100 amostras de dados no arquivo `mpu_data.csv`. Ao final, o LED voltará a ficar verde.
5.  **Verificar Dados:** Pressione o **Botão do Joystick** (GPIO 22). O LED ficará **azul** e o conteúdo do arquivo `mpu_data.csv` será impresso no monitor serial.

---

## 📊 **Funcionalidades Demonstradas**

-   **Interface com Sensor I2C:** Leitura e processamento de dados de um sensor de movimento de 6 eixos (MPU6050).
-   **Armazenamento em Cartão SD:** Utilização da biblioteca FatFs para montar um sistema de arquivos, criar, escrever e ler arquivos em formato CSV.
-   **Interface de Usuário com Botões:** Controle das principais funções do sistema (montar, gravar, ler) através de botões físicos.
-   **Feedback Visual e Sonoro:** Uso de um display OLED para dados em tempo real, um LED RGB para indicar o estado da máquina (pronto, gravando, etc.) e um buzzer para alerta de gravação.
-   **Manipulação de Interrupções:** Resposta a eventos de hardware (pressionar de botões) de forma eficiente utilizando interrupções de GPIO.
-   **Uso de Múltiplos Barramentos I2C:** Segregação do sensor e do display em barramentos I2C distintos para evitar conflitos de endereço e otimizar a comunicação.
