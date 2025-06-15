// SlowPacket.cpp
// Este arquivo implementa a classe SlowPacket. Ele contém a lógica para
// serializar um objeto SlowPacket em um stream de bytes para transmissão
// e para deserializar um stream de bytes recebido de volta para um objeto
// SlowPacket. Cuida da conversão de endianness e da manipulação dos campos
// de bits do cabeçalho.

#include "SlowPacket.h"
#include "Utils.h"
#include <cstring> // Para memcpy
#include <iostream> // Para debug

// --- Implementação dos métodos da struct SlowHeader ---

// sttl ocupa os 27 bits inferiores (0-26)
const uint32_t STTL_MASK = 0x07FFFFFF;
// As flags ocupam os 5 bits superiores (27-31)
const uint32_t FLAGS_MASK = 0xF8000000;

void SlowHeader::setSttl(uint32_t sttl) {
    // Preserva as flags atuais (nos bits superiores)
    uint32_t current_flags = sttl_and_flags & FLAGS_MASK;
    // Define o novo sttl, garantindo que ele não ultrapasse 27 bits, e combina com as flags.
    sttl_and_flags = (sttl & STTL_MASK) | current_flags;
}

uint32_t SlowHeader::getSttl() const {
    // Extrai os 27 bits inferiores
    return sttl_and_flags & STTL_MASK;
}

void SlowHeader::setFlag(SlowFlags flag, bool value) {
    // Liga ou desliga o bit específico da flag
    if (value) {
        sttl_and_flags |= flag;
    } else {
        sttl_and_flags &= ~flag;
    }
}

bool SlowHeader::getFlag(SlowFlags flag) const {
    // Verifica se o bit da flag está ativo
    return (sttl_and_flags & flag) != 0;
}

// --- Implementação dos métodos da classe SlowPacket ---

SlowPacket::SlowPacket() {
    // Inicializa o cabeçalho com zeros. A diretiva #pragma pack(1) garante
    // que não há padding, então sizeof(SlowHeader) é o tamanho real do cabeçalho.
    memset(&header, 0, sizeof(SlowHeader));
    // O vetor de dados 'data' já é inicializado vazio por padrão.
}

// Construtor que deserializa a partir de um vetor de bytes
SlowPacket::SlowPacket(const std::vector<uint8_t>& raw_packet) {
    deserialize(raw_packet);
}

bool SlowPacket::deserialize(const std::vector<uint8_t>& raw_packet) {
    // Um pacote válido deve ter pelo menos o tamanho do cabeçalho
    if (raw_packet.size() < sizeof(SlowHeader)) {
        std::cerr << "Erro: pacote recebido muito pequeno para ser um pacote SLOW." << std::endl;
        return false;
    }

    // Copia os bytes do cabeçalho diretamente para a struct.
    // Isso funciona por causa do #pragma pack(1).
    memcpy(&header, raw_packet.data(), sizeof(SlowHeader));

    // Converte os campos de múltiplos bytes de little-endian (rede) para host-endian.
    header.sttl_and_flags = Utils::littleEndianToHost32(header.sttl_and_flags);
    header.seqnum = Utils::littleEndianToHost32(header.seqnum);
    header.acknum = Utils::littleEndianToHost32(header.acknum);
    header.window = Utils::littleEndianToHost16(header.window);
    // Campos de 1 byte (sid, fid, fo) não precisam de conversão.

    // Se houver dados além do cabeçalho, copia-os para o vetor 'data'.
    if (raw_packet.size() > sizeof(SlowHeader)) {
        data.assign(raw_packet.begin() + sizeof(SlowHeader), raw_packet.end());
    } else {
        data.clear();
    }

    return true;
}

std::vector<uint8_t> SlowPacket::serialize() const {
    // Cria uma cópia do cabeçalho para poder modificar os campos para o formato de rede
    // sem alterar o estado do objeto original (o método é const).
    SlowHeader network_header = header;

    // Converte os campos de múltiplos bytes de host-endian para little-endian (rede).
    network_header.sttl_and_flags = Utils::hostToLittleEndian32(network_header.sttl_and_flags);
    network_header.seqnum = Utils::hostToLittleEndian32(network_header.seqnum);
    network_header.acknum = Utils::hostToLittleEndian32(network_header.acknum);
    network_header.window = Utils::hostToLittleEndian16(network_header.window);

    // Cria o vetor de bytes com o tamanho total necessário (cabeçalho + dados).
    std::vector<uint8_t> raw_packet(sizeof(SlowHeader) + data.size());

    // Copia o cabeçalho já convertido para o início do vetor de bytes.
    memcpy(raw_packet.data(), &network_header, sizeof(SlowHeader));

    // Se houver dados, copia-os para o vetor de bytes logo após o cabeçalho.
    if (!data.empty()) {
        memcpy(raw_packet.data() + sizeof(SlowHeader), data.data(), data.size());
    }

    return raw_packet;
}

// --- Métodos auxiliares (Setters e Getters) ---

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