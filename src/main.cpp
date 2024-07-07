#include <Arduino.h>

#include <TM1638plus.h> // Biblioteca de manipulação do módulo de display

// Pinos utilizados pelo 74HC595 - Leds
#define LATCH_595_PIN 18
#define CLOCK_595_PIN 19
#define DATA_595_PIN 21

// Pinos utilizados pelo módulo TM1638
#define STROBE_TM_PIN 15
#define CLOCK_TM_PIN 2
#define DIO_TM_PIN 4

// Pinos utilizados pelos botões seletores de endereço
// #define ENDERECO_PIN_DEZ 33
// #define ENDERECO_PIN_UN 32

// Constantes utilizadas pelo módulo TM1638
const bool HIGH_FREQ_TM = true; // Configuração de alta frequência - verdadeiro para Esp

const long INTERVALO_LEITURA_BOTAO = 100; // Tempo em ms entre leituras dos botões
const long INTERVALO_ATT_DISPLAY = 1000; // Tempo em ms entre atualizações do display

TM1638plus tm(STROBE_TM_PIN, CLOCK_TM_PIN , DIO_TM_PIN, HIGH_FREQ_TM); // Objeto usado para controlar o módulo TM1638

// Variáveis Globais
// byte receivedData[20]; // Salva o quadro Modbus recebido
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


void setup() {
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


  // // Verifica se há um quadro modbus disponivel
  // if (quadroModbusDisponivel()) {
  //   // Quando há um quadro disponível - ler, testar erros e executar
  //   lerQuadroModbus();
  // }
  
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