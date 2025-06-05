#include <iostream>
#include <vector>
#include <array>
#include <iomanip> // Para std::hex e std::setw
#include <bitset>  // Para std::bitset para imprimir em binário
#include "Utils.h"
#include "SlowPacket.h"

// Função auxiliar para imprimir um UUID em hexadecimal e binário, dividido por octetos
void print_uuid(const std::array<uint8_t, 16>& uuid) {
    std::cout << "Hex: ";
    for (size_t i = 0; i < uuid.size(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(uuid[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) {
            std::cout << "-";
        }
    }
    std::cout << std::dec << std::endl; // Volta para decimal

    std::cout << "Bin: ";
    for (size_t i = 0; i < uuid.size(); ++i) {
        // Converte cada byte para um bitset de 8 bits para impressão binária
        std::cout << std::bitset<8>(uuid[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) {
            std::cout << "-";
        }
        std::cout << " "; // Adiciona um espaço entre os octetos para melhor legibilidade
    }
    std::cout << std::endl;
}

// Função auxiliar para imprimir um vetor de bytes
void print_bytes(const std::vector<uint8_t>& bytes, const std::string& label = "") {
    if (!label.empty()) {
        std::cout << label << ": ";
    }
    for (uint8_t byte : bytes) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
    }
    std::cout << std::dec << std::endl;
}


int main() {
    std::cout << "--- Testando Funcoes do Utils ---" << std::endl;

    // Testando generateUUIDv8
    std::array<uint8_t, 16> uuid_v8 = Utils::generateUUIDv8();
    std::cout << "UUIDv8 Gerado:" << std::endl;
    print_uuid(uuid_v8);

    // ************* Verificação de VER e VAR *************
    // Extraindo e verificando o campo 'ver' (versão)
    // O campo 'ver' está nos 4 bits mais significativos do octeto 6 (uuid[6])
    uint8_t ver_field = (uuid_v8[6] >> 4) & 0x0F; // Desloca para a direita 4 bits e mascara os 4 bits de interesse
    bool ver_ok = (ver_field == 0x08); // UUIDv8 = 0b1000
    std::cout << "  Versao (ver): " << std::hex << static_cast<int>(ver_field) << " (esperado 8) " << (ver_ok ? "OK" : "FALHA") << std::dec << std::endl;

    // Extraindo e verificando o campo 'var' (variante)
    // O campo 'var' está nos 2 bits mais significativos do octeto 8 (uuid[8])
    uint8_t var_field = (uuid_v8[8] >> 6) & 0x03; // Desloca para a direita 6 bits e mascara os 2 bits de interesse
    bool var_ok = (var_field == 0x02); // RFC 4122 variant (0b10)
    std::cout << "  Variante (var): " << std::hex << static_cast<int>(var_field) << " (esperado 2) " << (var_ok ? "OK" : "FALHA") << std::dec << std::endl;
    // ************************************************************


    // Testando generateNilUUID
    std::array<uint8_t, 16> nil_uuid = Utils::generateNilUUID();
    std::cout << "\nNil UUID Gerado:" << std::endl;
    print_uuid(nil_uuid);

    std::cout << "\n--- Testando Funcoes de Endianness (Utils) ---" << std::endl;

    uint16_t test_val_16 = 0xABCD; // 43981
    uint32_t test_val_32 = 0x12345678; // 305419896

    std::cout << "Valor original 16-bit: 0x" << std::hex << test_val_16 << std::dec << std::endl;
    uint16_t le_16 = Utils::hostToLittleEndian16(test_val_16);
    std::cout << "Para Little Endian 16-bit: 0x" << std::hex << le_16 << std::dec << std::endl;
    uint16_t host_16 = Utils::littleEndianToHost16(le_16);
    std::cout << "De volta para Host 16-bit: 0x" << std::hex << host_16 << std::dec << std::endl;
    std::cout << "Verificacao 16-bit: " << (test_val_16 == host_16 ? "OK" : "FALHA") << std::endl;

    std::cout << "\nValor original 32-bit: 0x" << std::hex << test_val_32 << std::dec << std::endl;
    uint32_t le_32 = Utils::hostToLittleEndian32(test_val_32);
    std::cout << "Para Little Endian 32-bit: 0x" << std::hex << le_32 << std::dec << std::endl;
    uint32_t host_32 = Utils::littleEndianToHost32(le_32);
    std::cout << "De volta para Host 32-bit: 0x" << std::hex << host_32 << std::dec << std::endl;
    std::cout << "Verificacao 32-bit: " << (test_val_32 == host_32 ? "OK" : "FALHA") << std::endl;

    std::cout << "\n--- Testando Funcoes do SlowPacket ---" << std::endl;

    SlowPacket packet;

    // Testando set/get de STTL
    packet.header.setSttl(12345);
    std::cout << "STTL definido para: 12345. Obtido: " << packet.header.getSttl() << " " << (packet.header.getSttl() == 12345 ? "OK" : "FALHA") << std::endl;

    // Testando set/get de Flags
    packet.header.setFlag(FLAG_CONNECT, true);
    packet.header.setFlag(FLAG_ACK, true);
    packet.header.setFlag(FLAG_MORE_BITS, false); // Desligar explicitamente se estiver ligado aleatoriamente

    std::cout << "Flag CONNECT: " << (packet.header.getFlag(FLAG_CONNECT) ? "Ligada" : "Desligada") << " " << (packet.header.getFlag(FLAG_CONNECT) ? "OK" : "FALHA") << std::endl;
    std::cout << "Flag REVIVE: " << (packet.header.getFlag(FLAG_REVIVE) ? "Ligada" : "Desligada") << " " << (!packet.header.getFlag(FLAG_REVIVE) ? "OK" : "FALHA") << std::endl; // Esperado desligado
    std::cout << "Flag ACK: " << (packet.header.getFlag(FLAG_ACK) ? "Ligada" : "Desligada") << " " << (packet.header.getFlag(FLAG_ACK) ? "OK" : "FALHA") << std::endl;
    std::cout << "Flag MORE_BITS: " << (packet.header.getFlag(FLAG_MORE_BITS) ? "Ligada" : "Desligada") << " " << (!packet.header.getFlag(FLAG_MORE_BITS) ? "OK" : "FALHA") << std::endl; // Esperado desligado


    // Testando serialização e deserialização de um pacote básico
    std::cout << "\n--- Testando Serializacao/Deserializacao ---" << std::endl;
    SlowPacket original_packet;
    original_packet.setSessionID(Utils::generateUUIDv8());
    original_packet.header.setSttl(5000);
    original_packet.header.setFlag(FLAG_CONNECT, false); // Nao eh connect
    original_packet.header.setFlag(FLAG_ACK, true);
    original_packet.header.setFlag(FLAG_MORE_BITS, true);
    original_packet.setSequenceNumber(123456);
    original_packet.setAcknowledgementNumber(7890);
    original_packet.setWindowSize(1024);
    original_packet.setFragmentID(5);
    original_packet.setFragmentOffset(2);
    std::string test_data_str = "Isso eh um teste de dados para o pacote SLOW!";
    original_packet.setData(std::vector<uint8_t>(test_data_str.begin(), test_data_str.end()));

    std::cout << "Original Packet Details:" << std::endl;
    std::cout << "  SID:" << std::endl; print_uuid(original_packet.getSessionID());
    std::cout << "  STTL: " << original_packet.header.getSttl() << std::endl;
    std::cout << "  SeqNum: " << original_packet.getSequenceNumber() << std::endl;
    std::cout << "  AckNum: " << original_packet.getAcknowledgementNumber() << std::endl;
    std::cout << "  Window: " << original_packet.getWindowSize() << std::endl;
    std::cout << "  FID: " << static_cast<int>(original_packet.getFragmentID()) << std::endl;
    std::cout << "  FO: " << static_cast<int>(original_packet.getFragmentOffset()) << std::endl;
    std::cout << "  Data Size: " << original_packet.getData().size() << std::endl;
    std::cout << "  Flags: C=" << original_packet.header.getFlag(FLAG_CONNECT)
              << " R=" << original_packet.header.getFlag(FLAG_REVIVE)
              << " ACK=" << original_packet.header.getFlag(FLAG_ACK)
              << " A/R=" << original_packet.header.getFlag(FLAG_ACCEPT_REJECT)
              << " MB=" << original_packet.header.getFlag(FLAG_MORE_BITS) << std::endl;


    std::vector<uint8_t> serialized_packet = original_packet.serialize();
    std::cout << "\nSerialized Packet (" << serialized_packet.size() << " bytes):" << std::endl;
    print_bytes(serialized_packet);

    SlowPacket deserialized_packet;
    if (deserialized_packet.deserialize(serialized_packet)) {
        std::cout << "\nDeserialized Packet Details:" << std::endl;
        std::cout << "  SID:" << std::endl; print_uuid(deserialized_packet.getSessionID());
        std::cout << "  STTL: " << deserialized_packet.header.getSttl() << std::endl;
        std::cout << "  SeqNum: " << deserialized_packet.getSequenceNumber() << std::endl;
        std::cout << "  AckNum: " << deserialized_packet.getAcknowledgementNumber() << std::endl;
        std::cout << "  Window: " << deserialized_packet.getWindowSize() << std::endl;
        std::cout << "  FID: " << static_cast<int>(deserialized_packet.getFragmentID()) << std::endl;
        std::cout << "  FO: " << static_cast<int>(deserialized_packet.getFragmentOffset()) << std::endl;
        std::cout << "  Data Size: " << deserialized_packet.getData().size() << std::endl;
        std::cout << "  Data Content: " << std::string(deserialized_packet.getData().begin(), deserialized_packet.getData().end()) << std::endl;
        std::cout << "  Flags: C=" << deserialized_packet.header.getFlag(FLAG_CONNECT)
                  << " R=" << deserialized_packet.header.getFlag(FLAG_REVIVE)
                  << " ACK=" << deserialized_packet.header.getFlag(FLAG_ACK)
                  << " A/R=" << deserialized_packet.header.getFlag(FLAG_ACCEPT_REJECT)
                  << " MB=" << deserialized_packet.header.getFlag(FLAG_MORE_BITS) << std::endl;

        // Verificacao final de igualdade
        bool test_passed = true;
        if (original_packet.getSessionID() != deserialized_packet.getSessionID()) test_passed = false;
        if (original_packet.header.getSttl() != deserialized_packet.header.getSttl()) test_passed = false;
        if (original_packet.getSequenceNumber() != deserialized_packet.getSequenceNumber()) test_passed = false;
        if (original_packet.getAcknowledgementNumber() != deserialized_packet.getAcknowledgementNumber()) test_passed = false;
        if (original_packet.getWindowSize() != deserialized_packet.getWindowSize()) test_passed = false;
        if (original_packet.getFragmentID() != deserialized_packet.getFragmentID()) test_passed = false;
        if (original_packet.getFragmentOffset() != deserialized_packet.getFragmentOffset()) test_passed = false;
        if (original_packet.getData() != deserialized_packet.getData()) test_passed = false;
        // As flags precisam ser verificadas individualmente ou comparando o valor de sttl_and_flags
        if (original_packet.header.getFlag(FLAG_CONNECT) != deserialized_packet.header.getFlag(FLAG_CONNECT)) test_passed = false;
        if (original_packet.header.getFlag(FLAG_REVIVE) != deserialized_packet.header.getFlag(FLAG_REVIVE)) test_passed = false;
        if (original_packet.header.getFlag(FLAG_ACK) != deserialized_packet.header.getFlag(FLAG_ACK)) test_passed = false;
        if (original_packet.header.getFlag(FLAG_ACCEPT_REJECT) != deserialized_packet.header.getFlag(FLAG_ACCEPT_REJECT)) test_passed = false;
        if (original_packet.header.getFlag(FLAG_MORE_BITS) != deserialized_packet.header.getFlag(FLAG_MORE_BITS)) test_passed = false;


        std::cout << "\nDeserializacao e Reconstrucao: " << (test_passed ? "SUCESSO" : "FALHA") << std::endl;

    } else {
        std::cout << "\nFalha na deserializacao." << std::endl;
    }


    return 0;
}