/**
 * @file Utils.h
 * @brief Definição de funções utilitárias para o projeto.
 *
 * Este arquivo de cabeçalho declara funções auxiliares usadas em todo o projeto,
 * incluindo a geração de UUIDs para Session IDs e a conversão de ordem de bytes
 * (endianness). A conversão de endianness é crucial para garantir que os dados
 * de múltiplos bytes sejam interpretados corretamente entre diferentes
 * arquiteturas de processador.
 */
#ifndef UTILS_H
#define UTILS_H

#include <array>
#include <cstdint>
#include <vector>
#include <iostream>
#include <SlowPacket.h>

class SlowPacket;

namespace Utils {
    // Gera um UUIDv8 (RFC 9562) para ser usado como Session ID.
    std::array<uint8_t, 16> generateUUIDv8();

    // Gera um UUID nulo (todos os bytes zero).
    std::array<uint8_t, 16> generateNilUUID();

    // Funções para conversão entre little-endian e host byte order.
    uint16_t hostToLittleEndian16(uint16_t host_val);
    uint16_t littleEndianToHost16(uint16_t le_val);
    uint32_t hostToLittleEndian32(uint32_t host_val);
    uint32_t littleEndianToHost32(uint32_t le_val);

    // Imprime um vetor de bytes em formato hexadecimal, útil para depuração.
    void printHex(const std::vector<uint8_t>& data, const std::string& label);

    // Imprime os detalhes do cabeçalho de um pacote SLOW, útil para depuração.
    void printPacketDetails(const SlowPacket& packet, const std::string& label);
};

#endif