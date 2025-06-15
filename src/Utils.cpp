// Utils.cpp
// Este arquivo implementa as funções utilitárias declaradas em Utils.h.
// Contém o código para gerar UUIDs de forma padronizada e para realizar
// conversões entre a ordem de bytes do host e a ordem little endian.
// Essas implementações são cruciais para garantir que os dados de múltiplos bytes
// no protocolo SLOW sejam corretamente formatados e interpretados em diferentes sistemas.

#include "Utils.h"
#include <random>
#include <chrono>
#include <iomanip>

std::array<uint8_t, 16> Utils::generateUUIDv8() {
    std::array<uint8_t, 16> uuid;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);

    // Gera um valor aleatório entre 0 e 255 para cada 8 bits do UUID
    for (int i = 0; i < 16; i++) {
        uuid[i] = dist(gen);
    }

    // Define ver com base nas normas da RFC9562
    // Localizado nos 4 bits mais significativos do octeto 6
    uuid[6] = (uuid[6] & 0x0F) | 0x80; // 0b1000

    // Define var com base nas normas da RFC9562
    // Localizado nos 2 bits mais significativos do octeto 8
    uuid[8] = (uuid[8] & 0x3F) | 0x80; // 0b10

    return uuid;
}

std::array<uint8_t, 16> Utils::generateNilUUID() {
    return {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
}

// Implementações para conversão little-endian e big-endian
// Em sistemas big-endian, ocorre a inversão dos bytes tanto na ida quanto na volta
// Em sistemas little-endian, mantém a ordem, uma vez que o SLOW é little-endian

uint16_t Utils::hostToLittleEndian16(uint16_t host_val) {
    if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) {
        return (host_val << 8 | host_val >> 8);
    } else {
        return host_val;
    }
}

uint16_t Utils::littleEndianToHost16(uint16_t le_val) {
    if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) {
        return (le_val << 8 | le_val >> 8);
    } else {
        return le_val;
    }
}

uint32_t Utils::hostToLittleEndian32(uint32_t host_val) {
    if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) {
    return ((host_val >> 24) & 0x000000FF) |
            ((host_val >>  8) & 0x0000FF00) |
            ((host_val <<  8) & 0x00FF0000) |
            ((host_val << 24) & 0xFF000000);
    } else {
        return host_val;
    }
}

uint32_t Utils::littleEndianToHost32(uint32_t le_val) {
     if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) {
        return ((le_val >> 24) & 0x000000FF) |
               ((le_val >>  8) & 0x0000FF00) |
               ((le_val <<  8) & 0x00FF0000) |
               ((le_val << 24) & 0xFF000000);
    } else {
        return le_val;
    }
}

uint8_t Utils::reverseBits(uint8_t b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

void Utils::printHex(const std::vector<uint8_t>& data, const std::string& label) {
    std::cout << label << " (" << data.size() << " bytes):" << std::endl;
    for (size_t i = 0; i < data.size(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
        if ((i + 1) % 16 == 0) {
            std::cout << std::endl;
        }
    }
    std::cout << std::dec << std::endl; // Volta para decimal
}