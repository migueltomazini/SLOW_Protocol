/**
 * @file SlowPacket.cpp
 * @brief Implementação da classe SlowPacket e dos métodos de manipulação do cabeçalho.
 *
 * Este arquivo contém a lógica para serializar um objeto SlowPacket em um
 * fluxo de bytes para transmissão e para deserializar um fluxo de bytes
 * recebido de volta para um objeto SlowPacket. Ele lida com a conversão
 * de endianness e a manipulação dos campos de bits do cabeçalho de forma segura.
 */

#include "SlowPacket.h"
#include "Utils.h"
#include <cstring> // Para memcpy
#include <iostream>

// Máscaras de bits para o campo sttl_and_flags
const uint32_t FLAGS_MASK = 0x0000001F; // Máscara para os 5 bits inferiores (flags)
const uint32_t STTL_MASK = ~FLAGS_MASK;  // Máscara para os 27 bits superiores (sttl)

//==============================================================================
// MÉTODOS DA STRUCT SlowHeader
//==============================================================================

void SlowHeader::setSttl(uint32_t sttl) {
    // Isola as flags atuais para não as corromper.
    uint32_t flags_only = sttl_and_flags & FLAGS_MASK;
    // Desloca o valor do STTL para a posição correta (bits 5 a 31) e aplica a máscara.
    uint32_t sttl_part = (sttl << 5) & STTL_MASK;
    // Combina as flags preservadas com o novo valor do STTL.
    sttl_and_flags = flags_only | sttl_part;
}

uint32_t SlowHeader::getSttl() const {
    // Extrai os 27 bits superiores e os desloca de volta para a direita.
    return (sttl_and_flags & STTL_MASK) >> 5;
}

void SlowHeader::setFlag(SlowFlags flag, bool value) {
    if (value) {
        sttl_and_flags |= flag; // Liga o bit da flag usando OR
    } else {
        sttl_and_flags &= ~flag; // Desliga o bit da flag usando AND com a máscara invertida
    }
}

bool SlowHeader::getFlag(SlowFlags flag) const {
    return (sttl_and_flags & flag) != 0;
}

//==============================================================================
// MÉTODOS DA CLASSE SlowPacket
//==============================================================================

SlowPacket::SlowPacket() {
    memset(&header, 0, sizeof(SlowHeader));
}

SlowPacket::SlowPacket(const std::vector<uint8_t>& raw_packet) {
    deserialize(raw_packet);
}

bool SlowPacket::deserialize(const std::vector<uint8_t>& raw_packet) {
    if (raw_packet.size() < sizeof(SlowHeader)) {
        std::cerr << "Erro: pacote recebido muito pequeno para ser um pacote SLOW." << std::endl;
        return false;
    }

    // Copia os bytes brutos diretamente para a estrutura do cabeçalho.
    memcpy(&header, raw_packet.data(), sizeof(SlowHeader));

    // Converte os campos multi-byte do formato little-endian (rede) para o formato do host.
    header.sttl_and_flags = Utils::littleEndianToHost32(header.sttl_and_flags);
    header.seqnum = Utils::littleEndianToHost32(header.seqnum);
    header.acknum = Utils::littleEndianToHost32(header.acknum);
    header.window = Utils::littleEndianToHost16(header.window);

    // Copia o payload de dados, se houver.
    if (raw_packet.size() > sizeof(SlowHeader)) {
        data.assign(raw_packet.begin() + sizeof(SlowHeader), raw_packet.end());
    } else {
        data.clear();
    }
    return true;
}

std::vector<uint8_t> SlowPacket::serialize() const {
    SlowHeader network_header = header;

    // Converte os campos multi-byte do formato do host para o formato little-endian (rede).
    network_header.sttl_and_flags = Utils::hostToLittleEndian32(network_header.sttl_and_flags);
    network_header.seqnum = Utils::hostToLittleEndian32(network_header.seqnum);
    network_header.acknum = Utils::hostToLittleEndian32(network_header.acknum);
    network_header.window = Utils::hostToLittleEndian16(network_header.window);

    // Cria o vetor de bytes com o tamanho total necessário.
    std::vector<uint8_t> raw_packet(sizeof(SlowHeader) + data.size());

    // Copia o cabeçalho no formato de rede para o início do vetor de bytes.
    memcpy(raw_packet.data(), &network_header, sizeof(SlowHeader));

    // Copia o payload de dados, se houver, logo após o cabeçalho.
    if (!data.empty()) {
        memcpy(raw_packet.data() + sizeof(SlowHeader), data.data(), data.size());
    }
    return raw_packet;
}


// --- Métodos de acesso (Setters e Getters) ---

void SlowPacket::setSessionID(const std::array<uint8_t, 16>& id) {
    header.sid = id;
}

std::array<uint8_t, 16> SlowPacket::getSessionID() const {
    return header.sid;
}

void SlowPacket::setSequenceNumber(uint32_t num) {
    header.seqnum = num;
}

uint32_t SlowPacket::getSequenceNumber() const {
    return header.seqnum;
}

void SlowPacket::setAcknowledgementNumber(uint32_t num) {
    header.acknum = num;
}

uint32_t SlowPacket::getAcknowledgementNumber() const {
    return header.acknum;
}

void SlowPacket::setWindowSize(uint16_t size) {
    header.window = size;
}

uint16_t SlowPacket::getWindowSize() const {
    return header.window;
}

void SlowPacket::setFragmentID(uint8_t id) {
    header.fid = id;
}

uint8_t SlowPacket::getFragmentID() const {
    return header.fid;
}

void SlowPacket::setFragmentOffset(uint8_t offset) {
    header.fo = offset;
}

uint8_t SlowPacket::getFragmentOffset() const {
    return header.fo;
}

void SlowPacket::setData(const std::vector<uint8_t>& data_bytes) {
    data = data_bytes;
}

const std::vector<uint8_t>& SlowPacket::getData() const {
    return data;
}

uint32_t SlowPacket::getSttl() const {
    return header.getSttl();
}
