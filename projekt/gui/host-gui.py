import matplotlib
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import tkinter as tk
from tkinter import messagebox
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

from custom_socket import CustomSocket


host_ip = "10.42.0.227"
port = 12345 # tu damy jakis port
socket = CustomSocket(host_ip, port)


matplotlib.use('TkAgg')

root = tk.Tk()
root.title("Aplikacja SKPS")

fig, ax = plt.subplots()
plt.style.use('fivethirtyeight')


def activate_brake():
    socket.send_brake_signal()

def deactivate_brake():
    socket.send_deactivate_brake_signal()

def draw_plot(i):
    ax.cla()
    ax.plot(socket.index_list, socket.speed_list)
    ax.legend(['Speed over time'], loc='upper left')
    plt.tight_layout()


def update_and_schedule():
    socket.update_records()
    root.after(100, update_and_schedule)

root.after(100, update_and_schedule)
animation = FuncAnimation(fig, draw_plot, interval=1000)

canvas = FigureCanvasTkAgg(fig, master=root)
canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=1)

brake_button = tk.Button(root, text="Break", command=activate_brake)
brake_button.pack(side=tk.BOTTOM)
deactivate_brake_button = tk.Button(root, text="Deactivate Break", command=deactivate_brake)
deactivate_brake_button.pack(side=tk.BOTTOM)

root.mainloop()
