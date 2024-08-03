import socket

def main():
    host = '10.42.0.227'  
    port = 12345        

    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    client_socket.connect((host, port))

    try:
        while True:
            data = client_socket.recv(1024)
            if not data:
                break
            print("Received:", data.decode())
    except KeyboardInterrupt:
        print("Client closed.")
    finally:
        client_socket.close()

if __name__ == "__main__":
    main()