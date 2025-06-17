/**
 * @file Utils.cpp
 * @brief Implementação de funções utilitárias para o protocolo SLOW.
 *
 * Este arquivo contém a implementação de funções auxiliares usadas na
 * manipulação de UUIDs, conversão entre formatos de endianness, inversão de bits
 * e depuração via impressão de dados em hexadecimal e informações de pacotes.
 * 
 * Tais funções são essenciais para garantir a interoperabilidade e
 * consistência na comunicação entre diferentes arquiteturas no protocolo SLOW.
 */

#include "Utils.h"
#include <random>
#include <chrono>
#include <iomanip>

/**
 * @brief Gera um UUID versão 8 (UUIDv8) conforme a RFC 9562.
 * 
 * A versão 8 permite o uso de dados arbitrários definidos pela aplicação.
 * Este UUID é preenchido com valores aleatórios.
 * 
 * @return std::array<uint8_t, 16> UUID v8 aleatório.
 */
std::array<uint8_t, 16> Utils::generateUUIDv8() {
    std::array<uint8_t, 16> uuid;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);

    for (int i = 0; i < 16; i++) {
        uuid[i] = dist(gen);
    }

    // Define versão (ver=8) e variante (var=RFC padrão)
    uuid[6] = (uuid[6] & 0x0F) | 0x80;
    uuid[8] = (uuid[8] & 0x3F) | 0x80;

    return uuid;
}

/**
 * @brief Gera um UUID nulo (todos os bytes iguais a zero).
 * 
 * Frequentemente usado para representar a ausência de UUID.
 * 
 * @return std::array<uint8_t, 16> UUID nulo.
 */
std::array<uint8_t, 16> Utils::generateNilUUID() {
    return {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
}

/**
 * @brief Converte um valor de 16 bits da ordem do host para little-endian.
 * 
 * @param host_val Valor na ordem do host.
 * @return uint16_t Valor convertido para little-endian.
 */
uint16_t Utils::hostToLittleEndian16(uint16_t host_val) {
    if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) {
        return (host_val << 8 | host_val >> 8);
    } else {
        return host_val;
    }
}

/**
 * @brief Converte um valor de 16 bits de little-endian para a ordem do host.
 * 
 * @param le_val Valor em little-endian.
 * @return uint16_t Valor convertido para a ordem do host.
 */
uint16_t Utils::littleEndianToHost16(uint16_t le_val) {
    if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) {
        return (le_val << 8 | le_val >> 8);
    } else {
        return le_val;
    }
}

/**
 * @brief Converte um valor de 32 bits da ordem do host para little-endian.
 * 
 * @param host_val Valor na ordem do host.
 * @return uint32_t Valor convertido para little-endian.
 */
uint32_t Utils::hostToLittleEndian32(uint32_t host_val) {
    if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) {
        return ((host_val >> 24) & 0x000000FF) |
               ((host_val >> 8)  & 0x0000FF00) |
               ((host_val << 8)  & 0x00FF0000) |
               ((host_val << 24) & 0xFF000000);
    } else {
        return host_val;
    }
}

/**
 * @brief Converte um valor de 32 bits de little-endian para a ordem do host.
 * 
 * @param le_val Valor em little-endian.
 * @return uint32_t Valor convertido para a ordem do host.
 */
uint32_t Utils::littleEndianToHost32(uint32_t le_val) {
    if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) {
        return ((le_val >> 24) & 0x000000FF) |
               ((le_val >> 8)  & 0x0000FF00) |
               ((le_val << 8)  & 0x00FF0000) |
               ((le_val << 24) & 0xFF000000);
    } else {
        return le_val;
    }
}

/**
 * @brief Inverte os bits de um byte.
 * 
 * @param b Byte de entrada.
 * @return uint8_t Byte com bits invertidos.
 */
uint8_t Utils::reverseBits(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

/**
 * @brief Imprime um vetor de bytes em formato hexadecimal.
 * 
 * @param data Vetor de bytes.
 * @param label Rótulo para identificar o dado impresso.
 */
void Utils::printHex(const std::vector<uint8_t>& data, const std::string& label) {
    std::cout << label << " (" << data.size() << " bytes):" << std::endl;
    for (size_t i = 0; i < data.size(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
        if ((i + 1) % 16 == 0) {
            std::cout << std::endl;
        }
    }
    std::cout << std::dec << std::endl;
}

/**
 * @brief Imprime os detalhes de um pacote SLOW de forma estruturada.
 * 
 * Exibe session ID, campos de cabeçalho, flags ativas e uma prévia do payload.
 * 
 * @param packet Pacote SLOW a ser analisado.
 * @param label Rótulo identificador da impressão.
 */
void Utils::printPacketDetails(const SlowPacket& packet, const std::string& label) {
    std::cout << "---[ Packet Details: " << label << " ]---" << std::endl;

    std::cout << "  - SID:      ";
    for (const auto& byte : packet.getSessionID()) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    }
    std::cout << std::dec << std::endl;

    std::cout << "  - STTL:     " << packet.getSttl() << std::endl;
    std::cout << "  - SeqNum:   " << packet.getSequenceNumber() << std::endl;
    std::cout << "  - AckNum:   " << packet.getAcknowledgementNumber() << std::endl;
    std::cout << "  - Window:   " << packet.getWindowSize() << std::endl;
    std::cout << "  - Frag ID:  " << (int)packet.getFragmentID() << std::endl;
    std::cout << "  - Frag Off: " << (int)packet.getFragmentOffset() << std::endl;

    std::cout << "  - Flags:    [";
    if (packet.header.getFlag(FLAG_CONNECT))        std::cout << "C";
    if (packet.header.getFlag(FLAG_REVIVE))         std::cout << "R";
    if (packet.header.getFlag(FLAG_ACK))            std::cout << "A";
    if (packet.header.getFlag(FLAG_ACCEPT_REJECT))  std::cout << "A/R";
    if (packet.header.getFlag(FLAG_MORE_BITS))      std::cout << "M";
    std::cout << "]" << std::endl;

    const auto& data = packet.getData();
    std::cout << "  - Data Len: " << data.size() << " bytes" << std::endl;
    if (!data.empty()) {
        std::cout << "  - Data Str: \"";
        for (size_t i = 0; i < std::min((size_t)32, data.size()); ++i) {
            char c = data[i];
            std::cout << (isprint(c) ? c : '.');
        }
        if (data.size() > 32) std::cout << "...";
        std::cout << "\"" << std::endl;
    }

    std::cout << "------------------------------------------------" << std::endl;
}
