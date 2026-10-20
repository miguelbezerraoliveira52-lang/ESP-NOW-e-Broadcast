#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <cstring>

constexpr uint8_t ID_NO = 1;

constexpr uint8_t ID_EQUIPE = 1;

constexpr uint8_t CANAL_WIFI = 1;

constexpr bool SENSOR_HABILITADO = (ID_NO == 1);

constexpr uint8_t PINO_LDR = 34;
constexpr uint16_t SEM_SENSOR = 65535;


const uint8_t MAC_DESTINO[6] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const uint8_t MAC_BROADCAST[6] = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};


struct __attribute__((packed)) Mensagem {
  uint32_t sequencia;
  uint8_t equipe;
  uint8_t origem;
  uint16_t leitura;
  char texto[32];
};

static_assert(sizeof(Mensagem) == 40,
              "Tamanho inesperado da mensagem.");

struct Recebimento {
  uint8_t macOrigem[6];
  Mensagem mensagem;
};

QueueHandle_t filaRecebimento = nullptr;

uint32_t sequencia = 0;
bool unicastDisponivel = false;

char linhaSerial[80];
size_t tamanhoLinha = 0;
bool linhaLonga = false;


void imprimirMAC(const uint8_t *mac) {
  Serial.printf(
    "%02X:%02X:%02X:%02X:%02X:%02X",
    mac[0], mac[1], mac[2],
    mac[3], mac[4], mac[5]
  );
}

void erroFatal(const char *mensagem) {
  Serial.println(mensagem);

  while (true) {
    delay(1000);
  }
}

bool macUnicastValido(const uint8_t *mac) {
  bool todosZero = true;

  for (int i = 0; i < 6; i++) {
    if (mac[i] != 0) {
      todosZero = false;
    }
  }

  return !todosZero && ((mac[0] & 0x01) == 0);
}

bool cadastrarDestino(const uint8_t *mac) {
  esp_now_peer_info_t destino = {};

  memcpy(destino.peer_addr, mac, 6);
  destino.channel = CANAL_WIFI;
  destino.ifidx = WIFI_IF_STA;
  destino.encrypt = false;

  esp_err_t resultado = esp_now_add_peer(&destino);

  if (resultado == ESP_OK ||
      resultado == ESP_ERR_ESPNOW_EXIST) {
    return true;
  }

  Serial.printf(
    "Erro ao cadastrar destino: %s\n",
    esp_err_to_name(resultado)
  );

  return false;
}


void aoReceber(const esp_now_recv_info_t *info,const uint8_t *dados,int tamanho) {
  if (info == nullptr || dados == nullptr) {
    return;
  }

  if (tamanho != static_cast<int>(sizeof(Mensagem))) {
    return;
  }

  Recebimento recebido = {};

  memcpy(&recebido.mensagem, dados, sizeof(Mensagem));

  if (recebido.mensagem.equipe != ID_EQUIPE) {
    return;
  }

  if (recebido.mensagem.leitura != SEM_SENSOR &&
      recebido.mensagem.leitura > 4095) {
    return;
  }

  const char *fim = static_cast<const char *>(
    memchr(recebido.mensagem.texto, '\0',
           sizeof(recebido.mensagem.texto))
  );

  if (fim == nullptr) {
    return;
  }

  for (const char *p = recebido.mensagem.texto;
       p < fim; p++) {
    uint8_t caractere = static_cast<uint8_t>(*p);

    if (caractere < 32 || caractere > 126) {
      return;
    }
  }

  memcpy(recebido.macOrigem, info->src_addr, 6);

  xQueueSend(filaRecebimento, &recebido, 0);
}


void processarRecebimentos() {
  Recebimento recebido;

  while (xQueueReceive(filaRecebimento,&recebido, 0) == pdTRUE) {
    const Mensagem &mensagem = recebido.mensagem;

    Serial.println("\n--- MENSAGEM RECEBIDA ---");

    Serial.print("MAC de origem: ");
    imprimirMAC(recebido.macOrigem);
    Serial.println();

    Serial.printf("Equipe: %u\n", mensagem.equipe);
    Serial.printf("No de origem: %u\n", mensagem.origem);
    Serial.printf(
      "Sequencia: %lu\n",
      static_cast<unsigned long>(mensagem.sequencia)
    );

    if (mensagem.leitura == SEM_SENSOR) {
      Serial.println("ADC: sem sensor");
    } else {
      Serial.printf("ADC: %u\n", mensagem.leitura);
    }

    Serial.printf("Texto: %s\n", mensagem.texto);
    Serial.println("------------------------");
  }
}


void enviarMensagem(bool broadcast, const char *texto) {
  if (!broadcast && !unicastDisponivel) {
    Serial.println(
      "Configure o MAC_DESTINO com o MAC STA do parceiro "
      "e grave novamente."
    );
    return;
  }

  Mensagem mensagem = {};

  mensagem.sequencia = ++sequencia;
  mensagem.equipe = ID_EQUIPE;
  mensagem.origem = ID_NO;

  mensagem.leitura = SENSOR_HABILITADO
    ? static_cast<uint16_t>(analogRead(PINO_LDR))
    : SEM_SENSOR;

  memcpy(mensagem.texto, texto, strlen(texto) + 1);

  const uint8_t *destino = broadcast
    ? MAC_BROADCAST
    : MAC_DESTINO;

  esp_err_t resultado = esp_now_send(
    destino,
    reinterpret_cast<const uint8_t *>(&mensagem),
    sizeof(mensagem)
  );

  Serial.println("\n--- TENTATIVA DE ENVIO ---");
  Serial.printf(
    "Modo: %s\n",
    broadcast ? "BROADCAST" : "UNICAST"
  );

  Serial.print("Destino: ");
  imprimirMAC(destino);
  Serial.println();

  Serial.printf(
    "Sequencia: %lu\n",
    static_cast<unsigned long>(mensagem.sequencia)
  );

  if (mensagem.leitura == SEM_SENSOR) {
    Serial.println("ADC: sem sensor");
  } else {
    Serial.printf("ADC: %u\n", mensagem.leitura);
  }

  Serial.printf("Texto: %s\n", mensagem.texto);

  if (resultado == ESP_OK) {
    Serial.println(
      "Envio aceito localmente. Confira no receptor."
    );
  } else {
    Serial.printf(
      "Falha local: %s\n",
      esp_err_to_name(resultado)
    );
  }
}


void processarComando(const char *linha) {
  size_t tamanho = strlen(linha);

  if (tamanho < 3 ||
      (linha[0] != 'u' && linha[0] != 'b') ||
      linha[1] != ' ') {
    Serial.println(
      "Comando invalido. Use: u mensagem ou b mensagem"
    );
    return;
  }

  const char *texto = linha + 2;
  size_t tamanhoTexto = strlen(texto);

  if (tamanhoTexto == 0 || tamanhoTexto > 31) {
    Serial.println("Use de 1 a 31 caracteres no texto.");
    return;
  }

  for (size_t i = 0; i < tamanhoTexto; i++) {
    uint8_t caractere = static_cast<uint8_t>(texto[i]);

    if (caractere < 32 || caractere > 126) {
      Serial.println("Use texto ASCII, sem acentos.");
      return;
    }
  }

  enviarMensagem(linha[0] == 'b', texto);
}

void lerSerial() {
  while (Serial.available() > 0) {
    char caractere = static_cast<char>(Serial.read());

    if (caractere == '\r') {
      continue;
    }

    if (caractere == '\n') {
      if (linhaLonga) {
        Serial.println("Comando muito longo.");
      } else if (tamanhoLinha > 0) {
        linhaSerial[tamanhoLinha] = '\0';
        processarComando(linhaSerial);
      }

      tamanhoLinha = 0;
      linhaLonga = false;
      continue;
    }

    if (!linhaLonga) {
      if (tamanhoLinha < sizeof(linhaSerial) - 1) {
        linhaSerial[tamanhoLinha++] = caractere;
      } else {
        linhaLonga = true;
      }
    }
  }
}


void setup() {
  Serial.begin(115200);
  delay(1000);

  if (SENSOR_HABILITADO) {
    pinMode(PINO_LDR, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(PINO_LDR, ADC_11db);
  }

  filaRecebimento = xQueueCreate(10, sizeof(Recebimento));

  if (filaRecebimento == nullptr) {
    erroFatal("Erro ao criar fila de recebimento.");
  }

  if (!WiFi.mode(WIFI_STA)) {
    erroFatal("Erro ao ativar Wi-Fi STA.");
  }

  WiFi.disconnect();
  WiFi.setSleep(false);

  if (esp_wifi_set_channel(
        CANAL_WIFI, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    erroFatal("Erro ao configurar canal Wi-Fi.");
  }

  Serial.println("\n=== PRATICA 6: ESP-NOW ===");
  Serial.printf("No: %u\n", ID_NO);
  Serial.printf("Equipe: %u\n", ID_EQUIPE);
  Serial.printf("Canal: %u\n", CANAL_WIFI);

  Serial.print("MAC STA desta placa: ");
  Serial.println(WiFi.macAddress());

  Serial.printf(
    "Sensor: %s\n",
    SENSOR_HABILITADO ? "LDR no GPIO34" : "desabilitado"
  );

  if (esp_now_init() != ESP_OK) {
    erroFatal("Erro ao iniciar ESP-NOW.");
  }

  if (esp_now_register_recv_cb(aoReceber) != ESP_OK) {
    erroFatal("Erro ao registrar callback.");
  }

  if (!cadastrarDestino(MAC_BROADCAST)) {
    erroFatal("Erro ao cadastrar broadcast.");
  }

  if (macUnicastValido(MAC_DESTINO)) {
    unicastDisponivel = cadastrarDestino(MAC_DESTINO);
  }

  if (!unicastDisponivel) {
    Serial.println(
      "Unicast indisponivel: configure MAC_DESTINO."
    );
  }

  Serial.println("\nComandos:");
  Serial.println("u luz da bancada");
  Serial.println("b leitura coletiva");
  Serial.println(
    "Monitor Serial: 115200 baud e Nova linha."
  );
  Serial.println("Aguarde ao menos 1 segundo entre comandos.");
}

void loop() {
  processarRecebimentos();
  lerSerial();
  delay(1);
}