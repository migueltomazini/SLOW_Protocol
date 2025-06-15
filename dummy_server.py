import socket
import uuid
import struct

HOST = '127.0.0.1'
PORT = 7033
FLAG_CONNECT_MASK = 0x08000000

def print_hex(data, label=""):
    hex_str = ' '.join(f'{b:02x}' for b in data)
    print(f"[{label}] ({len(data)} bytes): {hex_str}")

def main():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.bind((HOST, PORT))
        print(f"Dummy Server v3 escutando em {HOST}:{PORT}...")

        while True:
            data, addr = s.recvfrom(1500)
            print(f"\n--- Pacote recebido de {addr} ---")
            print_hex(data, "Recebido")

            # Sempre extrai o SID e as flags para decidir o que fazer
            sid_from_client = data[0:16]
            sttl_flags, = struct.unpack('<I', data[16:20])

            # CENÁRIO 1: É um pacote de conexão?
            if sid_from_client == b'\x00' * 16 and (sttl_flags & FLAG_CONNECT_MASK):
                print("[Análise] Pacote de CONEXÃO detectado. Enviando resposta SETUP...")
                
                new_session_id = uuid.uuid4().bytes
                resp_flags = (1 << 30) | (1 << 29) # ACCEPT + ACK
                resp_seqnum = 0
                resp_acknum = 1
                resp_window = 4096
                
                header_format = "<16sIIIHBB"
                response_packet = struct.pack(header_format,
                    new_session_id, resp_flags, resp_seqnum,
                    resp_acknum, resp_window, 0, 0)

                print_hex(response_packet, "Enviando SETUP")
                s.sendto(response_packet, addr)
            
            # CENÁRIO 2: É um pacote de dados/desconexão de uma sessão existente
            else:
                client_seqnum, = struct.unpack('<I', data[20:24])
                print(f"[Análise] Pacote de DADOS/DISCONNECT (seq={client_seqnum}) detectado. Enviando ACK...")

                resp_flags = (1 << 29) # Apenas FLAG_ACK
                resp_seqnum = 1 # Seqnum estático do servidor
                resp_acknum = client_seqnum + 1 # Acusa recebimento e espera o próximo
                resp_window = 4096
                
                header_format = "<16sIIIHBB"
                
                ack_packet = struct.pack(header_format,
                    sid_from_client, # Usa o SID recebido no pacote
                    resp_flags, resp_seqnum, resp_acknum,
                    resp_window, 0, 0)
                
                print_hex(ack_packet, f"Enviando ACK para seq={client_seqnum}")
                s.sendto(ack_packet, addr)

if __name__ == '__main__':
    main()