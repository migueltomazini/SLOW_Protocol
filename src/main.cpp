// main.cpp - VERSÃO DE TESTE E DEBUG
//
// Esta main foi projetada para testar as funcionalidades principais da aplicação.
// Ela é dividida em seções que podem ser executadas para verificar:
// 1. Funções utilitárias (Utils.h/.cpp)
// 2. Serialização/Deserialização de pacotes (SlowPacket.h/.cpp)
// 3. O fluxo completo de comunicação do Peripheral (requer um servidor Central)
//
// Para usar:
// 1. Compile seu projeto normalmente.
// 2. Execute o programa passando o IP e a Porta do servidor Central:
//    ./seu_executavel <central_ip> <central_port>
//

#include "Peripheral.h"
#include "SlowPacket.h"
#include "Utils.h"

#include <iostream>
#include <vector>
#include <thread>
#include <string>
#include <cassert> // Para asserts simples nos testes

// --- Protótipos das Funções de Teste ---

void test_utils();
void test_slow_packet();
void test_peripheral_full_flow(int argc, char* argv[]);

// --- Função Principal ---

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "         INICIANDO SUITE DE TESTES        " << std::endl;
    std::cout << "========================================" << std::endl;

    // Teste 1: Funções Utilitárias
    test_utils();

    // Teste 2: Lógica de Pacotes
    test_slow_packet();

    // Teste 3: Fluxo Completo do Peripheral
    // Este teste requer argumentos da linha de comando (IP e Porta)
    test_peripheral_full_flow(argc, argv);

    std::cout << "\n========================================" << std::endl;
    std::cout << "         SUITE DE TESTES CONCLUÍDA        " << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}

// --- Implementação das Funções de Teste ---

/**
 * @brief Testa as funções auxiliares em Utils.cpp
 */
void test_utils() {
    std::cout << "\n--- [TESTE 1/3] EXECUTANDO TESTES DE UTILS ---\n" << std::endl;

    // Teste de geração de UUID
    std::cout << "[Utils] Testando geração de UUIDs..." << std::endl;
    auto nil_uuid = Utils::generateNilUUID();
    auto v8_uuid = Utils::generateUUIDv8();
    Utils::printHex(std::vector<uint8_t>(nil_uuid.begin(), nil_uuid.end()), "Nil UUID (esperado: tudo 00)");
    Utils::printHex(std::vector<uint8_t>(v8_uuid.begin(), v8_uuid.end()), "UUIDv8 (esperado: aleatório com bits de versão/variante corretos)");
    std::cout << "------------------------------------------" << std::endl;

    // Teste de conversão de Endianness
    std::cout << "[Utils] Testando conversão de Endianness..." << std::endl;
    uint32_t original32 = 0x12345678;
    uint32_t little_endian32 = Utils::hostToLittleEndian32(original32);
    uint32_t host32_again = Utils::littleEndianToHost32(little_endian32);

    uint16_t original16 = 0xABCD;
    uint16_t little_endian16 = Utils::hostToLittleEndian16(original16);
    uint16_t host16_again = Utils::littleEndianToHost16(little_endian16);
    
    std::cout << std::hex; // Mudar para base hexadecimal para facilitar visualização
    std::cout << "Original 32-bit: 0x" << original32 << " -> LE: 0x" << little_endian32 << " -> Host: 0x" << host32_again << std::endl;
    std::cout << "Original 16-bit: 0x" << original16 << " -> LE: 0x" << little_endian16 << " -> Host: 0x" << host16_again << std::endl;
    std::cout << std::dec; // Voltar para decimal

    // Verificação
    if (original32 == host32_again && original16 == host16_again) {
        std::cout << "[PASSOU] Conversão de endianness é reversível." << std::endl;
    } else {
        std::cerr << "[FALHOU] Conversão de endianness NÃO é reversível." << std::endl;
    }
    
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    std::cout << "(Sistema é Big-Endian, valores foram trocados)" << std::endl;
#else
    std::cout << "(Sistema é Little-Endian, valores permaneceram iguais)" << std::endl;
#endif
    std::cout << "------------------------------------------" << std::endl;
}

/**
 * @brief Testa a criação, serialização e deserialização de um SlowPacket.
 */
void test_slow_packet() {
    std::cout << "\n--- [TESTE 2/3] EXECUTANDO TESTES DE SLOWPACKET ---\n" << std::endl;

    SlowPacket original;
    std::string test_payload_str = "Teste de Payload 123!";
    std::vector<uint8_t> test_payload(test_payload_str.begin(), test_payload_str.end());

    // 1. Preencher o pacote original com dados de teste
    std::cout << "[SlowPacket] Montando pacote original com dados de teste..." << std::endl;
    original.setSessionID(Utils::generateUUIDv8());
    original.header.setSttl(12345);
    original.header.setFlag(FLAG_CONNECT, true);
    original.header.setFlag(FLAG_ACK, true);
    original.header.setFlag(FLAG_MORE_BITS, false); // Explicitamente false
    original.setSequenceNumber(98765);
    original.setAcknowledgementNumber(54321);
    original.setWindowSize(1024);
    original.setFragmentID(42);
    original.setFragmentOffset(5);
    original.setData(test_payload);

    // 2. Serializar o pacote
    std::cout << "[SlowPacket] Serializando o pacote..." << std::endl;
    std::vector<uint8_t> raw_bytes = original.serialize();
    Utils::printHex(raw_bytes, "Bytes serializados");

    // 3. Deserializar para um novo objeto
    std::cout << "[SlowPacket] Deserializando os bytes para um novo pacote..." << std::endl;
    SlowPacket deserialized;
    if (!deserialized.deserialize(raw_bytes)) {
        std::cerr << "[FALHOU] Falha crítica na deserialização do pacote." << std::endl;
        return;
    }

    // 4. Verificar cada campo
    std::cout << "[SlowPacket] Verificando a integridade dos dados..." << std::endl;
    bool success = true;

    auto check = [&](const std::string& field, bool condition) {
        std::cout << "Verificando " << field << "... " << (condition ? "[PASSOU]" : "[FALHOU]") << std::endl;
        if (!condition) success = false;
    };

    check("Session ID", deserialized.getSessionID() == original.getSessionID());
    check("STTL", deserialized.header.getSttl() == 12345);
    check("Flag CONNECT", deserialized.header.getFlag(FLAG_CONNECT) == true);
    check("Flag ACK", deserialized.header.getFlag(FLAG_ACK) == true);
    check("Flag REVIVE", deserialized.header.getFlag(FLAG_REVIVE) == false);
    check("Flag MORE_BITS", deserialized.header.getFlag(FLAG_MORE_BITS) == false);
    check("Sequence Number", deserialized.getSequenceNumber() == 98765);
    check("Acknowledgement Number", deserialized.getAcknowledgementNumber() == 54321);
    check("Window Size", deserialized.getWindowSize() == 1024);
    check("Fragment ID", deserialized.getFragmentID() == 42);
    check("Fragment Offset", deserialized.getFragmentOffset() == 5);
    check("Payload Data", deserialized.getData() == test_payload);

    std::cout << "------------------------------------------" << std::endl;
    if (success) {
        std::cout << "Resultado final: [SUCESSO TOTAL] O pacote foi serializado e deserializado corretamente." << std::endl;
    } else {
        std::cerr << "Resultado final: [FALHA] Alguns campos não corresponderam após a deserialização." << std::endl;
    }
}

/**
 * @brief Testa o ciclo de vida completo do Peripheral.
 */
void test_peripheral_full_flow(int argc, char* argv[]) {
    std::cout << "\n--- [TESTE 3/3] EXECUTANDO TESTE DE FLUXO COMPLETO DO PERIPHERAL ---\n" << std::endl;

    // 1. Validar os argumentos da linha de comando
    if (argc != 3) {
        std::cerr << "[ERRO] Uso: " << argv[0] << " <central_ip> <central_port>" << std::endl;
        std::cerr << "Este teste foi ignorado por falta de argumentos." << std::endl;
        return;
    }

    const std::string central_ip = argv[1];
    int central_port;
    try {
        central_port = std::stoi(argv[2]);
    } catch (const std::exception& e) {
        std::cerr << "Erro: Porta inválida '" << argv[2] << "'" << std::endl;
        return;
    }
    
    std::cout << ">>> ATENÇÃO: Este teste requer um servidor Central rodando em "
              << central_ip << ":" << central_port << " <<<" << std::endl;

    try {
        Peripheral peripheral(central_ip, central_port);
        
        // 2. Iniciar a conexão (chama sendConnect() internamente)
        std::cout << "\n[PASSO 1] Tentando iniciar conexão..." << std::endl;
        if (!peripheral.start()) {
            std::cerr << "[FALHOU] Não foi possível iniciar o Peripheral. Verifique se o servidor está rodando ou se há um problema de rede." << std::endl;
            return;
        }

        // 3. Criar a thread de rede para escutar as respostas
        std::cout << "[PASSO 2] Iniciando thread de rede para escutar respostas do Central..." << std::endl;
        std::thread network_thread([&peripheral]() {
            peripheral.run();
        });

        // Um pequeno delay para dar tempo ao handshake de acontecer.
        std::cout << "[PASSO 3] Aguardando handshake (2 segundos)..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        std::cout << "\n>>> DICA: Se a conexão falhar, procure por 'Timeout! Retransmitindo pacote com seq=0' no console." << std::endl;
        std::cout << ">>> Isso indica que o teste de retransmissão do pacote CONNECT está funcionando." << std::endl;


        // 4. Enviar dados de teste (pacote único)
        std::cout << "\n[PASSO 4] Enviando um pacote de dados simples..." << std::endl;
        std::string message = "Ola mundo, este eh um teste do protocolo SLOW!";
        std::vector<uint8_t> data_to_send(message.begin(), message.end());
        peripheral.sendData(data_to_send);
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // 5. Enviar dados que precisam de fragmentação
        std::cout << "\n[PASSO 5] Enviando dados grandes para testar a fragmentação (2000 bytes)..." << std::endl;
        std::vector<uint8_t> large_data(2000, 'A');
        peripheral.sendData(large_data);
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // 6. Desconectar
        std::cout << "\n[PASSO 6] Enviando pedido de desconexão..." << std::endl;
        peripheral.sendDisconnect();

        // 7. Esperar a thread de rede terminar seu trabalho
        std::cout << "[PASSO 7] Aguardando a thread de rede finalizar..." << std::endl;
        network_thread.join();

        std::cout << "\n[SUCESSO] Fluxo completo do Peripheral executado com sucesso." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[FALHOU] Erro inesperado durante o teste de fluxo: " << e.what() << std::endl;
    }
}