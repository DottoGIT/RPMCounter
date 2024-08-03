import socket


class CustomSocket:
    def __init__(self, host_ip, port):

        # inicjalizujemy data pustym ciagiem bajtow
        self.data = b""
        self.start_signal = b"S;"
        self.end_signal = b"E;"
        self.activate_brake_signal = b"B;"
        self.deactivate_brake_signal = b"N;"
        self.host_ip = host_ip
        self.port = port
        self.speed_list = []
        self.index_list = []
        self.brake_triggered = False

        # gniazdo tcp
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.connect()

    def connect(self):
        self.socket.connect((self.host_ip, self.port))
        print("connected")
        self.socket.send(self.start_signal)
    
    def send_brake_signal(self):
        self.socket.send(self.activate_brake_signal)
        self.brake_triggered = True
    
    def send_deactivate_brake_signal(self):
        self.socket.send(self.deactivate_brake_signal)
        self.brake_triggered = False

    def receive_data(self):
        self.data += self.socket.recv(64)

    def read(self):
        print("trying to read data")
        while b";" not in self.data:
            self.receive_data()
            print("A")
        
        message, _ , self.data = self.data.partition(b";")

        return message.decode("utf-8")

    def update_records(self):
        message = self.read()
        print(message)
        
        if message[0] == "M":
            index, speed = map(int, message[1:].split(" "))
            self.index_list.append(index)
            self.speed_list.append(round(speed / 10, 2))
        else:
            print(message)
