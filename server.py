import socket
import time
import logging

# Konfigurasi logging
logging.basicConfig(level=logging.DEBUG, format='%(asctime)s - %(levelname)s - %(message)s')

# Konfigurasi server AIS
HOST = "0.0.0.0"  # Bind ke semua antarmuka
PORT = 4001        # Port untuk server AIS

def generate_ais_nmea():
    """Menghasilkan data dummy AIS dalam format NMEA."""
    timestamp = int(time.time())
    # return f"!AIVDO,1,1,,,15Mw0k0001q>Ac6@lk@Gio6005H`,0*37\r\n"  # Contoh NMEA
    # return f"$GPGGA,181908.00,3404.7041778,N,07044.3966270,W,4,13,1.00,495.144,M,29.200,M,0.10,0000,*40\r\n"  # Contoh NMEA
    return (
        f"$GPGGA,194546.127,5231.525,N,01323.391,E,1,12,1.0,0.0,M,0.0,M,,*6E\r\n"
        f"$GPGSA,A,3,01,02,03,04,05,06,07,08,09,10,11,12,1.0,1.0,1.0*30\r\n"
        f"$GPRMC,194546.127,A,5231.525,N,01323.391,E,2372.1,093.7,200220,000.0,W*40\r\n"
    )

def start_server():
    """Menjalankan server AIS."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.bind((HOST, PORT))
        server.listen(5)
        print(f"AIS Server berjalan di {HOST}:{PORT}")

        while True:
            client, addr = server.accept()
            print(f"Koneksi diterima dari {addr}")
            with client:
                try:
                    while True:
                        # ais_data = generate_ais_nmea() + "\r\n"
                        ais_data = generate_ais_nmea()
                        client.sendall(ais_data.encode("utf-8"))
                        logging.debug(f"Mengirim data AIS ke {addr}")
                        time.sleep(1)  # Simulasi interval pengiriman AIS
                except (BrokenPipeError, ConnectionResetError):
                    print(f"Koneksi dengan {addr} terputus")
                    continue

if __name__ == "__main__":
    start_server()
