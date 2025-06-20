# Implementação do Periférico - Protocolo SLOW

Este repositório contém a implementação do lado **Periférico** (cliente) para o protocolo de transporte SLOW. SLOW é um protocolo customizado sobre UDP, projetado para oferecer confiabilidade através de mecanismos como handshake de 3 vias, controle de fluxo por janela deslizante, confirmações (ACKs) e fragmentação de pacotes.

## Visão Geral do Projeto

O objetivo deste projeto é desenvolver um cliente funcional para o protocolo SLOW. O executável `peripheral` aqui presente implementa um ciclo de vida de comunicação completo e automatizado com um servidor Central. Ele demonstra as seguintes capacidades:

1.  **Conexão:** Inicia uma conexão usando um handshake de 3 vias.
2.  **Transmissão Confiável:** Envia dados e aguarda por confirmações (ACKs), gerenciando uma fila de retransmissão para pacotes não confirmados.
3.  **Fragmentação:** Lida com o envio de dados maiores que o MTU, fragmentando-os em pacotes menores e garantindo que todos sejam confirmados.
4.  **Desconexão:** Encerra a sessão de forma limpa.

O código é estruturado de forma modular e orientada a objetos para facilitar a compreensão e manutenção.

### Estrutura do Código

-   **`main.cpp`**: Ponto de entrada da aplicação. Orquestra o fluxo de teste automatizado.
-   **`Peripheral` (.h/.cpp)**: Classe principal que encapsula a lógica do cliente SLOW, incluindo a máquina de estados da sessão e o gerenciamento da comunicação.
-   **`SlowPacket` (.h/.cpp)**: Representa um pacote do protocolo SLOW. Contém a lógica para serializar e deserializar os pacotes, além de manipular o cabeçalho.
-   **`UdpSocket` (.h/.cpp)**: Uma classe wrapper que abstrai as chamadas de socket UDP da API POSIX, simplificando a comunicação em rede.
-   **`Utils` (.h/.cpp)**: Contém funções utilitárias para tarefas como geração de UUIDs, conversão de endianness e impressão de pacotes para depuração.

## Como Compilar e Executar

Este projeto utiliza CMake para gerenciar a compilação.

**Pré-requisitos:** `g++` (ou outro compilador C++17), `cmake` e `make`.

1.  **Clone o repositório:**
    ```bash
    git clone https://github.com/seu-usuario/seu-repositorio.git
    cd seu-repositorio
    ```

2.  **Crie um diretório de build:**
    ```bash
    mkdir build
    cd build
    ```

3.  **Gere os arquivos de compilação com CMake:**
    ```bash
    cmake ..
    ```

4.  **Compile o projeto:**
    ```bash
    make
    ```
    O executável `peripheral` será criado dentro do diretório `build`.

5.  **Execute a aplicação:**
    O programa requer o IP e a porta do Central como argumentos. Para se conectar ao servidor de teste oficial:
    ```bash
    ./peripheral 142.93.184.175 7033
    ```

## Exemplo de Execução

Abaixo está um exemplo da saída do programa ao se conectar ao Central de teste, demonstrando todas as etapas do ciclo de vida da comunicação.

```
>>> INICIANDO TESTE COMPLETO DO PERIFÉRICO SLOW <<<
Conectando ao Central em 142.93.184.175:7033

--- ETAPA 1: Iniciando conexão ---
Pacote de conexão enviado. Aguardando resposta...
Aguardando conclusão do handshake...
STTL recebido do central: 599ms
---[ Packet Details: Pacote Setup Recebido ]---
  - SID:      2c7be1e4eac286fe8d1d2c8e05a38aee
  - STTL:     599
  - SeqNum:   4417
  - AckNum:   0
  - Window:   1024
  - Frag ID:  0
  - Frag Off: 0
  - Flags:    [A/R]
  - Data Len: 0 bytes
------------------------------------------------
Sessão estabelecida. SID recebido. Janela do Central: 1024

>>> Conexão estabelecida com sucesso! Estado PÓS-HANDSHAKE:
---[ STATUS REPORT ]---
  - Estado:         CONNECTED
  - Session STTL:   599
  - Próximo SeqNum: 4418
  - Pacotes sem ACK:0
-----------------------

(Pausa de 2 segundos para visualização...)

--- ETAPA 2: Enviando um pacote de dados simples ---
---[ Packet Details: Pacote DATA Saindo ]---
  - SID:      2c7be1e4eac286fe8d1d2c8e05a38aee
  - STTL:     599
  - SeqNum:   4418
  - AckNum:   4417
  - Window:   1024
  - Frag ID:  0
  - Frag Off: 0
  - Flags:    []
  - Data Len: 25 bytes
  - Data Str: "Teste com STTL da sessao."
------------------------------------------------
Pacote de dados (seq=4418) enviado.
Pacote enviado. Aguardando confirmação (ACK)...
ACK recebido (acknum=4419). Pacotes em trânsito: 0
>>> Confirmação (ACK) para pacote simples recebida!
---[ STATUS REPORT ]---
  - Estado:         CONNECTED
  - Session STTL:   599
  - Próximo SeqNum: 4419
  - Pacotes sem ACK:0
-----------------------

(Pausa de 2 segundos para visualização...)

--- ETAPA 3: Enviando dados para fragmentação ---
---[ Packet Details: Pacote DATA Fragmento 0 Saindo ]---
  - SID:      2c7be1e4eac286fe8d1d2c8e05a38aee
  - STTL:     599
  - SeqNum:   4419
  - AckNum:   4418
  - Window:   1024
  - Frag ID:  218
  - Frag Off: 0
  - Flags:    [M]
  - Data Len: 1440 bytes
  - Data Str: "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB..."
------------------------------------------------
---[ Packet Details: Pacote DATA Fragmento 1 Saindo ]---
  - SID:      2c7be1e4eac286fe8d1d2c8e05a38aee
  - STTL:     599
  - SeqNum:   4420
  - AckNum:   4418
  - Window:   1024
  - Frag ID:  218
  - Frag Off: 1
  - Flags:    []
  - Data Len: 60 bytes
  - Data Str: "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB..."
------------------------------------------------
Dados fragmentados enviados em 2 pacotes.
Pacotes fragmentados enviados. Aguardando confirmações...
ACK recebido (acknum=4421). Pacotes em trânsito: 0
>>> Todas as confirmações (ACKs) para os fragmentos foram recebidas!
---[ STATUS REPORT ]---
  - Estado:         CONNECTED
  - Session STTL:   599
  - Próximo SeqNum: 4421
  - Pacotes sem ACK:0
-----------------------

(Pausa de 2 segundos para visualização...)

--- ETAPA 4: Enviando pedido de desconexão ---
---[ Packet Details: Pacote DISCONNECT Saindo ]---
  - SID:      00000000000000000000000000000000
  - STTL:     0
  - SeqNum:   0
  - AckNum:   0
  - Window:   1024
  - Frag ID:  0
  - Frag Off: 0
  - Flags:    [C]
  - Data Len: 0 bytes
------------------------------------------------
Pacote de desconexão enviado. A sessão será encerrada.
Aguardando finalização da sessão (timeout de 10s)...

ERRO CRÍTICO: Pacote com SID inválido recebido durante a desconexão. Este é um comportamento esperado do Central em teste. Encerrando.
---[ Packet Details: Pacote Incorreto Recebido ]---
  - SID:      adcf2c98eae08e85901929c20054fe0f
  - STTL:     599
  - SeqNum:   8174
  - AckNum:   0
  - Window:   1024
  - Frag ID:  0
  - Frag Off: 0
  - Flags:    [A/R]
  - Data Len: 0 bytes
------------------------------------------------
>>> Sessão finalizada com sucesso (estado = DISCONNECTED).
Loop da thread de rede encerrado.

>>> TESTE CONCLUÍDO <<<
```

## 👥 Autores

-   Antonio Carlos de Almeida Micheli Neto
-   Júlia Cavalio Orlando
-   Miguel Rodrigues Tomazini