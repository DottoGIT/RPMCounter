import csv
import random
import time

time_value = 0
speed_value = 0

fieldnames = ['time', 'speed']

with open('data.csv', 'w', newline='') as file:
    writer = csv.DictWriter(file, fieldnames=fieldnames)
    writer.writeheader()

while True:
    with open('data.csv', 'a', newline='') as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        time_value = round(time_value + 0.1, 1)
        speed_value = max(0, speed_value + random.randint(-4, 5))
        writer.writerow({'time': time_value, 'speed': speed_value})
        print(f'Wrote {time_value} and {speed_value} to file')
    
    time.sleep(0.1)