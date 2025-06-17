// Utils.h
// Este arquivo de cabeçalho define funções utilitárias que são usadas em todo o projeto.
// Inclui declarações para funções como a geração de UUIDv8 (para Session IDs)
// e funções para lidar com a conversão de ordem de bytes (endianness).

#ifndef UTILS_H
#define UTILS_H

#include <array>
#include <cstdint>
#include <vector>
#include <iostream>
#include <SlowPacket.h>

namespace Utils {
    // Gera um UUIDv8 conforme RFC9562.
    std::array<uint8_t, 16> generateUUIDv8();

    // Gera um Nil UUID
    std::array<uint8_t, 16> generateNilUUID();

    // Funções para conversão entre little-endian e host byte order, de 16 e 32 bits.
    // Isso é crucial para garantir que os dados de múltiplos bytes sejam interpretados corretamente
    // em diferentes arquiteturas de processador.
    uint16_t hostToLittleEndian16(uint16_t host_val);
    uint16_t littleEndianToHost16(uint16_t le_val);
    uint32_t hostToLittleEndian32(uint32_t host_val);
    uint32_t littleEndianToHost32(uint32_t le_val);

    uint8_t reverseBits(uint8_t b);

    // ... no final do namespace Utils
    void printHex(const std::vector<uint8_t>& data, const std::string& label);

    void printPacketDetails(const SlowPacket& packet, const std::string& label);
};

#endif 