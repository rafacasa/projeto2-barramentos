#include <Arduino.h>
#include <WiFi.h>
#include <TM1638plus.h> // Biblioteca de manipulação do módulo de display
#include <AsyncTCP.h>

// Pinos utilizados pelo 74HC595 - Leds
#define LATCH_595_PIN 18
#define CLOCK_595_PIN 19
#define DATA_595_PIN 21

// Pinos utilizados pelo módulo TM1638
#define STROBE_TM_PIN 15
#define CLOCK_TM_PIN 2
#define DIO_TM_PIN 4

//Porta TCP do Modbus
#define MODBUS_PORT 502

// Pinos utilizados pelos botões seletores de endereço
// #define ENDERECO_PIN_DEZ 33
// #define ENDERECO_PIN_UN 32

// Constantes com as informações de conexao wifi
const char* SSID = "FamiliaCasa";
const char* WIFI_PASSWORD = "";

// Constantes utilizadas pelo módulo TM1638
const bool HIGH_FREQ_TM = true; // Configuração de alta frequência - verdadeiro para Esp

const long INTERVALO_LEITURA_BOTAO = 100; // Tempo em ms entre leituras dos botões
const long INTERVALO_ATT_DISPLAY = 1000; // Tempo em ms entre atualizações do display

TM1638plus tm(STROBE_TM_PIN, CLOCK_TM_PIN , DIO_TM_PIN, HIGH_FREQ_TM); // Objeto usado para controlar o módulo TM1638

// Variáveis Globais
byte* receivedData; // Salva o quadro Modbus recebido
// byte resposta[20]; // Salva o quadro Modbus a ser enviado como resposta
// bool broadcast;        // Informa se o último quadro recebido foi um broadcast ou não
uint16_t registradores[8]; // Os valores a serem alterados pelas solicitações Modbus
uint8_t botoes; // Cada bit representa o estado de um botão do módulo TM1638
uint8_t reg_exibido_1, reg_exibido_2; // Guarda os registradores sendo exibidos no momento
// uint8_t endereco_escravo; // Guarda o endereco do escravo, conforme lido pelos botoes


// put function declarations here:
bool lerBotoes();
void atualizaDisplay();
void acendeLeds(uint8_t indice1, uint8_t indice2);
void atualizaRegistradoresExbidos();

static void handleData(void *arg, AsyncClient *client, void *data, size_t len) {
  receivedData = (byte *)data;
	Serial.printf("\n DADOS RECEBIDOS %s \n", client->remoteIP().toString().c_str());
  for (uint16_t i = 0; i < len; i++){
    Serial.printf("%02x - ", receivedData[i]);
  }
  Serial.println("");
  
  // COMANDO MODBUS RECEBIDO EM dados

  //Verificar MBAP -
    // Protocol ID
    // Unit ID
    // Length
  
  //Interpretar comando modbus
    // Verificar Funcao
    // Executar

	//our big json string test
	String jsonString = "{\"data_from_module_type\":5,\"hub_unique_id\":\"hub-bfd\",\"slave_unique_id\":\"\",\"water_sensor\":{\"unique_id\":\"water-sensor-ba9\",\"firmware\":\"0.0.1\",\"hub_unique_id\":\"hub-bfd\",\"ip_address\":\"192.168.4.2\",\"mdns\":\"water-sensor-ba9.local\",\"pair_status\":127,\"ec\":{\"value\":0,\"calib_launch\":0,\"sensor_k_origin\":1,\"sensor_k_calibration\":1,\"calibration_solution\":1,\"regulation_state\":1,\"max_pumps_durations\":5000,\"set_point\":200},\"ph\":{\"value\":0,\"calib_launch\":0,\"regulation_state\":1,\"max_pumps_durations\":5000,\"set_point\":700},\"water\":{\"temperature\":0,\"pump_enable\":false}}}";
	// reply to client
	if (client->space() > strlen(jsonString.c_str()) && client->canSend())
	{
		client->add(jsonString.c_str(), strlen(jsonString.c_str()));
		client->send();
	}
}

static void handleError(void *arg, AsyncClient *client, int8_t error) {
	Serial.printf("\n Erro %s na conexao do cliente %s \n", client->errorToString(error), client->remoteIP().toString().c_str());
}

static void handleDisconnect(void *arg, AsyncClient *client) {
	Serial.printf("\n Cliente %s se desconectou \n", client->remoteIP().toString().c_str());
}

static void handleTimeOut(void *arg, AsyncClient *client, uint32_t time) {
	Serial.printf("\n Cliente sofreu Timeout: %s \n", client->remoteIP().toString().c_str());
}

static void handleNewClient(void *arg, AsyncClient *client) {
	Serial.printf("\n Cliente conectou ao servidor, ip: %s", client->remoteIP().toString().c_str());
	// register events
	client->onData(&handleData, NULL);
	client->onError(&handleError, NULL);
	client->onDisconnect(&handleDisconnect, NULL);
	client->onTimeout(&handleTimeOut, NULL);
}


void setup() {
  //Inicializa a serial para debug
  Serial.begin(115200);

  // Inicializa o módulo TM 1638
  tm.displayBegin();
  tm.displayText("88888888");

  // Inicializa os pinos:
  //Pinos do 74HC595
  pinMode(LATCH_595_PIN, OUTPUT);
  pinMode(CLOCK_595_PIN, OUTPUT);
  pinMode(DATA_595_PIN, OUTPUT);

  // Pinos dos botoes seletores do endereco
  // pinMode(ENDERECO_PIN_DEZ, INPUT_PULLUP);
  // pinMode(ENDERECO_PIN_UN, INPUT_PULLUP);

  // Definição do endereço do escravo
  //Obtém os estados dos botões
  // int unidade_endereco = digitalRead(ENDERECO_PIN_UN);
  // int dezena_endereco = digitalRead(ENDERECO_PIN_DEZ);

  // Verifica o estado dos botões e escreve o valor do endereço
  // if (dezena_endereco == LOW) {
  //   if (unidade_endereco == LOW) {
  //     endereco_escravo = 4;
  //   } else {
  //     endereco_escravo = 3;
  //   }
  // } else {
  //   if (unidade_endereco == LOW) {
  //     endereco_escravo = 2;
  //   } else {
  //     endereco_escravo = 1;
  //   }
  // }

  // Inicializa com 0 os registradores
  for(uint8_t i = 0; i < 8; i++) {
    registradores[i] = i; // TODO ALTERAR PARA 0
  }

  // Inicializa os registradores exibidos
  reg_exibido_1 = 0;
  reg_exibido_2 = 1;

  WiFi.begin(SSID, WIFI_PASSWORD);
  Serial.println("Conectando a rede...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println(".");
  }
  Serial.println("");
  Serial.println("Concetado a rede com o endereço IP:");
  Serial.println(WiFi.localIP());

  AsyncServer *server = new AsyncServer(MODBUS_PORT);
  server->onClient(&handleNewClient, server);
	server->begin();
}

void loop() {
  // Faz a leitura dos botões
  if (lerBotoes()) { // Caso houve modifição da situação dos botões
    if (botoes > 0) { // Verifica se na situação atual existe algum botão pressionado
      atualizaRegistradoresExbidos(); // Atualiza os índices dos registradores sendo exibidos
    }
  }

   // Atualiza os dados do display
  atualizaDisplay();  
}


// Funcao que le e faz debounce dos botões
bool lerBotoes() {
  uint8_t botoes_atual;
  static bool pressionado = false;
  static unsigned long tempo_anterior = 0; // Salva o tempo da execução anterior
  unsigned long tempo_atual = millis();

  if (tempo_atual - tempo_anterior >= INTERVALO_LEITURA_BOTAO) {
    tempo_anterior = tempo_atual;
    botoes_atual = tm.readButtons();
    if (botoes_atual == botoes) {
      if (pressionado) {
        pressionado = false;
        return true;
      }
    } else {
      botoes = botoes_atual;
      pressionado = true;
    }
  }
  return false;
}

// Funcao que atualiza os dados exibidos no display a cada intervalo configurado
void atualizaDisplay() {
  unsigned long currentMillis = millis();
  static unsigned long previousMillisDisplay = 0;  // executed once 
  if (currentMillis - previousMillisDisplay >= INTERVALO_ATT_DISPLAY) {
    // Atualizando leds acesos
    acendeLeds(reg_exibido_1, reg_exibido_2);    
    // Atualizando valor do display
    if (reg_exibido_1 < reg_exibido_2) {
      tm.DisplayDecNumNibble(registradores[reg_exibido_1], registradores[reg_exibido_2],  false, TMAlignTextRight);
    } else {
      tm.DisplayDecNumNibble(registradores[reg_exibido_2], registradores[reg_exibido_1],  false, TMAlignTextRight);
    }
  }
}

// Funcao que acende os leds dos dois indices informados
void acendeLeds(uint8_t indice1, uint8_t indice2) {
  uint8_t data_to_send = 0;
  data_to_send += pow(2, indice1);
  data_to_send += pow(2, indice2); // Calcula o byte a ser enviado

  digitalWrite(LATCH_595_PIN, LOW); // Aciona o recebimento de dados
  shiftOut(DATA_595_PIN, CLOCK_595_PIN, MSBFIRST, data_to_send); // Envia os bytes
  digitalWrite(LATCH_595_PIN, HIGH); // Encerra o recebimento de dados
}

// Funcao que atualiza os indices dos registradores que estão sendo exibidos, confome os botões pressionados
void atualizaRegistradoresExbidos() {
  uint8_t indice_botao;
  // Checa se apenas um botão esta apertado
  if ((botoes > 0) && ((botoes & (botoes-1)) == 0)) {
    // Obtem o indice do bit 1 (LSB = 1, MSB = 8)
    indice_botao = ffs(botoes) - 1;

    if (indice_botao == reg_exibido_1) {
      return;
    }

    reg_exibido_2 = reg_exibido_1;
    reg_exibido_1 = indice_botao;
  }
}