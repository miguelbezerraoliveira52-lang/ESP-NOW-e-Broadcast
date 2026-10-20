# Comunicação ESP-NOW com ESP32 e LDR

Projeto desenvolvido para a **Prática 6 da disciplina de Sistemas Ciberfísicos e Colaborativos**, utilizando ESP32 e Arduino IDE.

## Objetivo

Implementar a comunicação entre placas ESP32 por meio do protocolo ESP-NOW, enviando uma mensagem curta e uma leitura de luminosidade no mesmo pacote.

O projeto utiliza dois modos de transmissão:

- **Unicast:** envia os dados para uma placa específica.
- **Broadcast:** envia os dados para os receptores ao alcance, configurados no mesmo canal.

## Funcionalidades

- Leitura do sensor LDR conectado ao GPIO34.
- Envio de mensagens pelo Monitor Serial.
- Identificação da equipe e do nó de origem.
- Contador de sequência por emissor.
- Filtro de recebimento pelo ID da equipe.
- Fila para processar as mensagens recebidas fora do callback do Wi-Fi.
- Validação do tamanho do pacote e do conteúdo recebido.

## Materiais necessários

- Duas ESP32 para comunicação entre A e B.
- Uma terceira ESP32 para testar broadcast com dois receptores.
- Um LDR.
- Um resistor de 10 kΩ.
- Protoboard e jumpers.
- Um cabo USB de dados por placa.
- Arduino IDE com o pacote **esp32 by Espressif Systems 3.x** instalado.

As bibliotecas utilizadas acompanham o pacote de suporte da ESP32.

## Montagem do circuito

Somente a placa A utiliza o sensor:

```text
3V3 → LDR → nó de leitura → resistor de 10 kΩ → GND
                  |
                GPIO34
```

As placas B e C precisam apenas de alimentação.

Nessa montagem, mais luz tende a aumentar a leitura do ADC. Os valores variam de **0 a 4095** e representam uma leitura bruta, não uma medida em lux.

## Configuração das placas

O mesmo código é utilizado nas três placas, alterando os parâmetros iniciais:

| Parâmetro | Placa A | Placa B | Placa C |
|---|---|---|---|
| `ID_NO` | `1` | `2` | `3` |
| `ID_EQUIPE` | Mesmo ID | Mesmo ID | Mesmo ID |
| `CANAL_WIFI` | `1` | `1` | `1` |
| Sensor | Habilitado | Desabilitado | Desabilitado |
| `MAC_DESTINO` | MAC de B | MAC de A | Pode ficar zerado |

O sensor é habilitado automaticamente quando `ID_NO` é igual a `1`.

### Identificação dos endereços MAC

1. Grave o código em cada placa com o `ID_NO` correspondente.
2. Inicialmente, mantenha `MAC_DESTINO` zerado.
3. Abra o Monitor Serial em **115200 baud**.
4. Pressione **EN/RESET**.
5. Anote o endereço apresentado como `MAC STA desta placa`.
6. Configure os destinos em A e B e grave novamente.

Exemplo de configuração para um MAC fictício `AA:BB:CC:11:22:33`:

```cpp
const uint8_t MAC_DESTINO[6] = {
  0xAA, 0xBB, 0xCC, 0x11, 0x22, 0x33
};
```

Substitua pelo endereço real da placa parceira.

## Como utilizar

No Monitor Serial, selecione:

- Velocidade: **115200 baud**.
- Final de linha: **Nova linha**.

Cada comando válido realiza um único envio. Aguarde pelo menos um segundo entre comandos.

### Envio por unicast

Na placa A, digite:

```text
u luz da bancada
```

A placa B apresenta a mensagem e a leitura do LDR.

Para responder, digite na placa B:

```text
u recebido em B
```

A resposta aparece na placa A com a indicação `sem sensor`.

### Envio por broadcast

Na placa A, digite:

```text
b leitura coletiva
```

As placas B e C podem receber o mesmo pacote, desde que estejam ao alcance e utilizem o mesmo canal e ID de equipe.

As mensagens devem conter **de 1 a 31 caracteres ASCII, sem acentos**.

## Estrutura dos dados

| Campo | Tipo | Descrição |
|---|---|---|
| `sequencia` | `uint32_t` | Contador incrementado a cada tentativa de envio |
| `equipe` | `uint8_t` | Identificador da equipe |
| `origem` | `uint8_t` | Identificador do nó emissor |
| `leitura` | `uint16_t` | ADC de 0 a 4095 ou 65535 para indicar ausência de sensor |
| `texto` | `char[32]` | Mensagem com até 31 caracteres e terminador |

Todas as placas devem utilizar a mesma estrutura de dados.

## Funcionamento

Ao receber um comando válido, a ESP32 prepara o pacote com identificação, sequência e texto. Na placa A, o LDR é lido naquele momento.

O callback de recepção valida o pacote e tenta colocá-lo em uma fila. O `loop()` retira as mensagens da fila e apresenta os dados no Monitor Serial.

Se a fila estiver cheia, novos recebimentos podem ser descartados.

## Testes sugeridos

| Teste | Resultado esperado |
|---|---|
| Enviar `u luz da bancada` em A | B recebe a mensagem |
| Enviar `u recebido em B` em B | A recebe uma resposta sem leitura de sensor |
| Enviar `b leitura coletiva` em A | B e C recebem o mesmo pacote |
| Cobrir e iluminar o LDR, enviando em cada condição | A leitura do ADC varia |
| Alterar o ID da equipe em C | C deixa de apresentar mensagens da equipe anterior |
| Alterar o canal de C | C deixa de receber os envios do canal original |

## Limitações

- Um envio aceito localmente não confirma o processamento no receptor.
- O broadcast não fornece confirmação individual de entrega.
- A comunicação depende do alcance, do canal e das condições do rádio.
- O ID da equipe filtra mensagens, mas não autentica os dispositivos.
- O projeto não utiliza criptografia.
- As placas não retransmitem mensagens; o projeto não forma uma rede em malha.
- O funcionamento precisa ser validado nas placas físicas.
