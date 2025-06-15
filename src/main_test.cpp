#include <iostream>
#include <vector>
#include <cassert>
#include <iomanip>

// Inclua todos os cabeçalhos das suas classes de protocolo
#include "SlowPacket.h"
#include "Utils.h"

// --- FUNÇÕES AUXILIARES DE TESTE ---

// Função para imprimir o resultado de um teste de forma clara
void print_test_result(const std::string& test_name, bool success) {
    std::cout << "Teste: " << std::left << std::setw(40) << test_name
              << " | Resultado: " << (success ? "[\033[32mPASS\033[0m]" : "[\033[31mFAIL\033[0m]")
              << std::endl;
}

// Função para comparar dois pacotes SLOW e ver se são idênticos
bool are_packets_equal(const SlowPacket& p1, const SlowPacket& p2) {
    if (p1.getSessionID() != p2.getSessionID()) return false;
    if (p1.getSttl() != p2.getSttl()) return false;
    if (p1.getFlags() != p2.getFlags()) return false;
    if (p1.getSequenceNumber() != p2.getSequenceNumber()) return false;
    if (p1.getAcknowledgementNumber() != p2.getAcknowledgementNumber()) return false;
    if (p1.getWindowSize() != p2.getWindowSize()) return false;
    if (p1.getFragmentID() != p2.getFragmentID()) return false;
    if (p1.getFragmentOffset() != p2.getFragmentOffset()) return false;
    if (p1.getData() != p2.getData()) return false;
    return true;
}

// --- SUÍTE DE TESTES ---

void test_endianness_conversion() {
    std::cout << "\n--- INICIANDO TESTES DE CONVERSÃO ENDIANNESS ---\n";
    uint32_t original32 = 0x12345678;
    uint32_t little_endian32 = Utils::hostToLittleEndian32(original32);
    uint32_t result32 = Utils::littleEndianToHost32(little_endian32);

    // Em uma máquina little-endian (x86), a conversão não deve fazer nada.
    // Em uma máquina big-endian, a conversão deve inverter os bytes.
    // O teste principal é que a ida-e-volta resulte no valor original.
    print_test_result("Conversão de 32-bit (ida-e-volta)", original32 == result32);

    uint16_t original16 = 0xABCD;
    uint16_t little_endian16 = Utils::hostToLittleEndian16(original16);
    uint16_t result16 = Utils::littleEndianToHost16(little_endian16);
    print_test_result("Conversão de 16-bit (ida-e-volta)", original16 == result16);
}

void test_packet_serialization_roundtrip() {
    std::cout << "\n--- INICIANDO TESTE DE SERIALIZAÇÃO (ROUND-TRIP) ---\n";

    // 1. Crie um pacote original com dados complexos
    SlowPacket original_packet;
    original_packet.setSessionID(Utils::generateUUIDv8());
    original_packet.setSttl(1234567); // Um valor de sttl
    original_packet.setFlags(FLAG_VAL_DATA);
    original_packet.setSequenceNumber(101);
    original_packet.setAcknowledgementNumber(55);
    original_packet.setWindowSize(4096);
    original_packet.setFragmentID(10);
    original_packet.setFragmentOffset(3);
    std::string message = "Ola mundo do teste!";
    original_packet.setData(std::vector<uint8_t>(message.begin(), message.end()));

    // 2. Serialize o pacote para um vetor de bytes
    std::vector<uint8_t> serialized_data = original_packet.serialize();
    std::cout << "Pacote serializado para " << serialized_data.size() << " bytes.\n";
    Utils::printHex(serialized_data, "HEX DUMP");

    // 3. Crie um novo pacote e deserialize os bytes nele
    SlowPacket deserialized_packet;
    bool success = deserialized_packet.deserialize(serialized_data);

    print_test_result("Deserialização bem-sucedida", success);
    if (!success) return;

    // 4. Compare o pacote original com o deserializado
    bool are_equal = are_packets_equal(original_packet, deserialized_packet);
    print_test_result("Pacote original == Pacote deserializado", are_equal);

    if (!are_equal) {
        std::cout << "\033[31mERRO: Os pacotes não são idênticos após serialização/deserialização!\033[0m\n";
    }
}

void test_specific_packet_generation() {
    std::cout << "\n--- INICIANDO TESTES DE GERAÇÃO DE PACOTES ESPECÍFICOS ---\n";

    // Teste 1: Pacote CONNECT
    std::cout << "\n1. Gerando pacote CONNECT:\n";
    SlowPacket connect_packet;
    connect_packet.setSessionID(Utils::generateNilUUID());
    connect_packet.setSttl(0);
    connect_packet.setFlags(FLAG_VAL_CONNECT);
    connect_packet.setSequenceNumber(0);
    connect_packet.setAcknowledgementNumber(0);
    connect_packet.setWindowSize(65535);
    Utils::printHex(connect_packet.serialize(), "CONNECT Packet Hex");

    // Teste 2: Pacote DATA
    std::cout << "\n2. Gerando pacote DATA:\n";
    SlowPacket data_packet;
    data_packet.setSessionID(Utils::generateUUIDv8());
    data_packet.setSttl(30000);
    data_packet.setFlags(FLAG_VAL_ACK); // Um pacote de dados simples é um ACK para o pacote anterior do servidor
    data_packet.setSequenceNumber(1);
    data_packet.setAcknowledgementNumber(1);
    data_packet.setWindowSize(60000);
    std::string data_msg = "DADOS";
    data_packet.setData(std::vector<uint8_t>(data_msg.begin(), data_msg.end()));
    Utils::printHex(data_packet.serialize(), "DATA Packet Hex");

    // Teste 3: Pacote DISCONNECT
    std::cout << "\n3. Gerando pacote DISCONNECT:\n";
    SlowPacket disconnect_packet;
    disconnect_packet.setSessionID(data_packet.getSessionID()); // Usa o mesmo SID da sessão
    disconnect_packet.setSttl(29000);
    disconnect_packet.setFlags(FLAG_VAL_DISCONNECT); // Valor combinado para disconnect
    disconnect_packet.setSequenceNumber(2);
    disconnect_packet.setAcknowledgementNumber(1);
    disconnect_packet.setWindowSize(60000);
    Utils::printHex(disconnect_packet.serialize(), "DISCONNECT Packet Hex");
}


int main() {
    std::cout << "=======================================\n";
    std::cout << "  INICIANDO SUÍTE DE TESTES DO PROTOCOLO SLOW\n";
    std::cout << "=======================================\n";

    // Executa todos os testes definidos
    test_endianness_conversion();
    test_packet_serialization_roundtrip();
    test_specific_packet_generation();

    std::cout << "\n=======================================\n";
    std::cout << "  TESTES CONCLUÍDOS\n";
    std::cout << "=======================================\n";

    return 0;
}