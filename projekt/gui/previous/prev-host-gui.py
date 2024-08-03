import pandas as pd
import matplotlib
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import tkinter as tk
from tkinter import messagebox
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

matplotlib.use('TkAgg')

root = tk.Tk()
root.title("Speed over Time")

fig, ax = plt.subplots()
plt.style.use('fivethirtyeight')


def activate_brake():
    messagebox.showinfo("brake", "wysylamy sygnal zeby hamowac")

def deactivate_brake():
    messagebox.showinfo("deactivate brake", "wysylamy sygnal zeby przestac hamowac")

def draw_plot(i):
    data = pd.read_csv('data.csv')
    times = data['time']
    speeds = data['speed']

    ax.cla()
    ax.plot(times, speeds)
    ax.legend(['Speed over time'], loc='upper left')
    plt.tight_layout()


animation = FuncAnimation(fig, draw_plot, interval=100)

canvas = FigureCanvasTkAgg(fig, master=root)
canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=1)

deactivate_brake_button = tk.Button(root, text="Deactivate Brake", command=deactivate_brake)
deactivate_brake_button.pack(side=tk.BOTTOM)

button = tk.Button(root, text="Brake", command=activate_brake)
button.pack(side=tk.BOTTOM)

root.mainloop()
