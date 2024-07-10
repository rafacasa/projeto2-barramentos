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
#define ENDERECO_PIN_DEZ 33
#define ENDERECO_PIN_UN 32

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
uint16_t qtd_bytes_dados_recebidos; // Salva o tamanho do quadro Modbus recebido
char resposta[100]; // Salva o quadro Modbus a ser enviado como resposta
uint16_t qtd_bytes_resposta; // Salva o tamanho do quadro Modbus a ser enviado como resposta
// bool broadcast;        // Informa se o último quadro recebido foi um broadcast ou não
uint16_t registradores[8]; // Os valores a serem alterados pelas solicitações Modbus
uint8_t botoes; // Cada bit representa o estado de um botão do módulo TM1638
uint8_t reg_exibido_1, reg_exibido_2; // Guarda os registradores sendo exibidos no momento
uint8_t endereco_escravo; // Guarda o endereco do escravo, conforme lido pelos botoes


// put function declarations here:
bool lerBotoes();
void atualizaDisplay();
void acendeLeds(uint8_t indice1, uint8_t indice2);
void atualizaRegistradoresExbidos();
bool checaValidadeModbus();
bool avaliaComandoModbus();
uint8_t executaSolicitacao();
uint8_t executaWriteMultipleRegisters();
void escreveRegistrador(uint16_t endereco, uint16_t valor);

static void handleData(void *arg, AsyncClient *client, void *data, size_t len) {
  uint16_t qtd_bytes_para_cabecalho;
  bool envia_resposta = false;
  receivedData = (byte *)data;
  qtd_bytes_dados_recebidos = len;
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

  if (checaValidadeModbus()) {
      //Interpretar comando modbus
        // Verificar Funcao
        // Executar
    envia_resposta = avaliaComandoModbus();
  }
  
	// Envio da resposta
  if (envia_resposta) {
    qtd_bytes_para_cabecalho = qtd_bytes_resposta - 6;

    resposta[5] = qtd_bytes_para_cabecalho & 0xff;  // Adicionando o tamanho da resposta no cabecalho
    resposta[4] = qtd_bytes_para_cabecalho >> 8;

    if (client->space() > qtd_bytes_resposta && client->canSend())
    {
      client->add(resposta, qtd_bytes_resposta);
      client->send();
    }
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

  //Pinos dos botoes seletores do endereco
  pinMode(ENDERECO_PIN_DEZ, INPUT_PULLUP);
  pinMode(ENDERECO_PIN_UN, INPUT_PULLUP);

  // Definição do endereço do escravo
  //Obtém os estados dos botões
  int unidade_endereco = digitalRead(ENDERECO_PIN_UN);
  int dezena_endereco = digitalRead(ENDERECO_PIN_DEZ);

  // Verifica o estado dos botões e escreve o valor do endereço
  if (dezena_endereco == LOW) {
    if (unidade_endereco == LOW) {
      endereco_escravo = 4;
    } else {
      endereco_escravo = 3;
    }
  } else {
    if (unidade_endereco == LOW) {
      endereco_escravo = 2;
    } else {
      endereco_escravo = 1;
    }
  }
  Serial.printf("UnitID configurado para %d\n", endereco_escravo);

  // Inicializa com 0 os registradores
  for(uint8_t i = 0; i < 8; i++) {
    registradores[i] = 0;
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
  Serial.println("Conectado a rede com o endereço IP:");
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

// Testar ProtocolID, UnitID e tamanho informado
bool checaValidadeModbus() {
  // Testando protocolID
  if (receivedData[2] == 0x00 && receivedData[3] == 0x00) {

    Serial.println("ProtocolID ok");
    if (receivedData[6] == endereco_escravo) {

      Serial.println("UnitID ok");
      uint16_t tamanho_informado = receivedData[4];
      tamanho_informado <<= 8;
      tamanho_informado += receivedData[5] & 0xff;
      if (tamanho_informado == (qtd_bytes_dados_recebidos - 6)) {

        Serial.println("Tamanho informado ok");
        return true;
      } else {
        Serial.println("Erro Tamanho Informado");
      }
    } else {
      Serial.println("Erro UnitID");
    }
  } else {
    Serial.println("Erro ProtocolID");
  }
  return false;
}

bool avaliaComandoModbus() {
  uint8_t codigoExcessao;
  // Monta o inicio do cabecalho da resposta
  resposta[0] = receivedData[0];
  resposta[1] = receivedData[1];
  resposta[2] = 0x00;
  resposta[3] = 0x00;
  resposta[6] = endereco_escravo;

  qtd_bytes_resposta = 7;

  // Tenta executar a solicitação e obtem o código de excessão caso não foi possível
  codigoExcessao = executaSolicitacao();

  switch (codigoExcessao) { // Verifica o código retornado pela função executaSolicitacao()
    case 1:
      // Enviando exceção funcao não suportada
      resposta[7] = receivedData[7] | 0x80;
      resposta[8] = 0x01;
      qtd_bytes_resposta+=2;
      return true;
    case 2:
      // Enviando exceção endereco invalido
      resposta[7] = receivedData[7] | 0x80;
      resposta[8] = 0x02;
      qtd_bytes_resposta+=2;
      return true;
    case 3:
      // Enviando exceção dados do registrador invalidos
      resposta[7] = receivedData[7] | 0x80;
      resposta[8] = 0x03;
      qtd_bytes_resposta+=2;
      return true;
    case 4:
      // Enviando exceção de valor invalido para o registrador
      resposta[7] = receivedData[7] | 0x80;
      resposta[8] = 0x04;
      qtd_bytes_resposta+=2;
      return true;
    case 0:
      // Executou com sucesso - A resposta foi montada na função
      return true;
    default:
      return false;
  }
}

// Executa a solicitação enviada, caso seja possível.
// Retorna 0 caso a solicitação foi executada com sucesso
// Retorna o código de excessão caso houve algum erro:
//  1 - Funcao nao suportada
//  2 - Endereço inválido
//  3 - Dados do registrador inválidos
uint8_t executaSolicitacao() {
  uint8_t funcao_solicitada = receivedData[7];

  switch (funcao_solicitada) { // Verifica a função solicitada
    case 0x10: // Write Multiple Registers
      return executaWriteMultipleRegisters();
      break;
    
    default: // Função não suportada
      return 1;
      break;
  }
}

// Funcao que executa a funcao modbus 0x10
uint8_t executaWriteMultipleRegisters() {
  uint16_t quantidade_registradores, endereco_inicial, valor_informado[8];
  uint8_t contagem_bytes;

  // Obtêm o campo Quantidade de resgistradores do quadro Modbus recebido
  quantidade_registradores = receivedData[10]; // MSB
  quantidade_registradores <<= 8;
  quantidade_registradores += receivedData[11] & 0xff; // LSB

  // Verifica se a quantidade de resgistradores sendo alterados pela solicitação. Gera a excessão 3.
  if (quantidade_registradores < 1 || quantidade_registradores > 8) {
    return 3;
  }

  // Obtêm o campo Contagem de bytes do quadro Modbus recebido
  contagem_bytes = receivedData[12];

  // Verifica se a quantidade de bytes informada no quadro Modbus é compativel com a solicitacao
  // 2 * quantidade de registradores a serem alterados
  // Gera a excessão 3
  if (contagem_bytes != quantidade_registradores * 2) {
    return 3;
  }

  // Obtêm o campo Endereço do primeiro registrador do quadro Modbus recebido
  endereco_inicial = receivedData[8]; // MSB
  endereco_inicial <<= 8;
  endereco_inicial += receivedData[9] & 0xff; // LSB

  // Verifica se o endereço inicial está entre os endereços disponíveis
  // Gera a excessão 2
  if (endereco_inicial < 0x0010 || endereco_inicial > 0x0017) {
    return 2;
  }

  // Verifica se todos os endereços de registradoes são disponíveis
  // Gera a excessão 2
  if (endereco_inicial + quantidade_registradores > 0x0018) {
    return 2;
  }

  // Checa se os valores a serem escritos estão entre 0 e 1023 - Excessao 4
  for (uint8_t i = 0; i < quantidade_registradores; i++) {
    // Obtem o i-ésimo valor a ser escrito em registradores
    valor_informado[i] = receivedData[(2*i) + 13]; // MSB
    valor_informado[i] <<= 8;
    valor_informado[i] += receivedData[(2*i) + 14] & 0xff; // LSB

    // Caso o valor a ser alterado seja maior que o permitido, levanta a excessão
    if (valor_informado[i] > 1023) {
      return 4;
    }
  }

  // Escreve os valores nos registradores
  for (uint8_t i = 0; i < quantidade_registradores; i++) {
    escreveRegistrador(endereco_inicial + i, valor_informado[i]);
  }

  // Criando e enviando resposta de confirmação
  resposta[7] = 0x10;
  resposta[8] = receivedData[8];
  resposta[9] = receivedData[9];
  resposta[10] = receivedData[10];
  resposta[11] = receivedData[11];
  qtd_bytes_resposta=12;
  return 0;
}

// Função que escreve o valor informado em "valor" no registrador identificado por "endereco"
void escreveRegistrador(uint16_t endereco, uint16_t valor) {
  uint16_t indice = endereco - 16;
  registradores[indice] = valor;
}
