import socket
import uuid
import struct

# Endereço e porta para escutar
HOST = '127.0.0.1'  # localhost
PORT = 7033

# Flags do protocolo (posição de bit a partir da direita, 0-indexed)
# FLAG_CONNECT = 1 << 27 etc.
# Em um único int de 32 bits, isso é mais fácil
FLAG_CONNECT_MASK = 0x08000000

def print_hex(data, label=""):
    hex_str = ' '.join(f'{b:02x}' for b in data)
    print(f"[{label}] ({len(data)} bytes): {hex_str}")

def main():
    # Cria um socket UDP
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.bind((HOST, PORT))
        print(f"Dummy Server escutando em {HOST}:{PORT}...")

        while True:
            # Espera por um pacote
            data, addr = s.recvfrom(1500)
            print(f"\n--- Pacote recebido de {addr} ---")
            print_hex(data, "Recebido")

            # Deserializa o cabeçalho básico para ver o que é
            # SID (16 bytes) + sttl_flags (4)
            sid = data[0:16]
            sttl_flags_le = data[16:20]
            
            # Converte flags para host order (Python é geralmente little-endian, mas é bom ser explícito)
            sttl_flags, = struct.unpack('<I', sttl_flags_le)

            # Verifica se é um pacote de conexão (SID nulo e flag CONNECT ligada)
            if sid == b'\x00' * 16 and (sttl_flags & FLAG_CONNECT_MASK):
                print("[Análise] Pacote de CONEXÃO detectado. Enviando resposta SETUP...")

                # Monta uma resposta de SETUP (Accept)
                new_session_id = uuid.uuid4().bytes # Gera um SID aleatório
                
                # sttl_flags: FLAG_ACCEPT_REJECT (1<<30) + FLAG_ACK (1<<29)
                resp_flags = (1 << 30) | (1 << 29)
                resp_seqnum = 0
                resp_acknum = 1 # Acusa recebimento do seqnum 0 do cliente
                resp_window = 4096

                # Empacota em little-endian
                # Formato: < (little-endian) 16s (16 bytes) I (uint32) I I H (uint16) B B (uint8)
                header_format = "<16sIIHHBB"
                
                # Note que o sttl_and_flags é apenas as flags
                # sid, sttl_flags, seq, ack, window, fid, fo
                response_packet = struct.pack(header_format,
                    new_session_id,
                    resp_flags,
                    resp_seqnum,
                    resp_acknum,
                    resp_window,
                    0, # fid
                    0  # fo
                )

                print_hex(response_packet, "Enviando SETUP")
                s.sendto(response_packet, addr)
            else:
                print("[Análise] Pacote desconhecido. Ignorando.")

if __name__ == '__main__':
    main()