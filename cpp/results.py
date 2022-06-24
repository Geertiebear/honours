import sys
import matplotlib.pyplot as plt
import numpy as np
from parse import *

read_time = 10 * 10**-9
write_time = 100 * 10**-9
adc_latency = 1 * 10**-9
read_energy = 40 * 10**-15
write_energy = 20 * 10**-12
adc_energy = 2 * 10**-12

experiments = ["wiki-Vote"]

def values_from_file(filename):
    with open(filename, "r") as results:
        data = results.readlines()
        total_time = 0
        total_energy = 0
        efficiency = 0.0
        [row, col] = parse("init: {:d},{:d}\n", data[0]) 
        for i in range(1, len(data)):
            parsed = parse("{:l}{}", data[i])
            command = parsed[0].strip('\n')
            if command == "clear":
                total_time += row*write_time
                total_energy += row*write_energy
            elif command == "read":
                [adc_acts, row_reads, row_writes, inputs] = parse(" {:d},{:d},{:d},{:d}\n", parsed[1])
                total_time += (row_reads * read_time + adc_acts * adc_latency) * inputs
                total_energy += (row_reads * read_energy + adc_acts * adc_energy) * inputs
            elif command == "write":
                [adc_acts, row_reads, row_writes, inputs] = parse(" {:d},{:d},{:d},{:d}\n", parsed[1])
                total_time += (row_writes * write_time + adc_acts * adc_latency)
                total_energy += (row_writes * write_energy + adc_acts * adc_energy) * inputs
            elif command == "efficiency":
                efficiency = parse(" {:f}\n", parsed[1])[0]
            else:
                print("Unknown command " + command)
        return (total_time, total_energy, efficiency)


experiment_results = {
        "graphr-time": [],
        "graphr-energy": [],
        "graphr-efficiency": [],
        "our-time": [],
        "our-energy": [],
        "our-efficiency": [],
        "our-offsets-time" : [],
        "our-offsets-energy": [],
        "our-total-time": [],
        "our-total-energy": []}

for experiment in experiments:
    [graphr_time, graphr_energy, graphr_efficiency] = values_from_file(experiment + "-graphr.log")
    experiment_results["graphr-time"].append(graphr_time)
    experiment_results["graphr-energy"].append(graphr_energy)
    experiment_results["graphr-efficiency"].append(graphr_efficiency)

    [our_time, our_energy, our_efficiency] = values_from_file(experiment + "-ours.log")
    experiment_results["our-time"].append(our_time)
    experiment_results["our-energy"].append(our_energy)
    experiment_results["our-efficiency"].append(our_efficiency)

    [our_offset_time, our_offset_energy, _] = values_from_file(experiment + "-ours-offsets.log")
    experiment_results["our-offsets-time"].append(our_offset_time)
    experiment_results["our-offsets-energy"].append(our_offset_energy)

    experiment_results["our-total-time"].append(our_offset_time + our_time)
    experiment_results["our-total-energy"].append(our_offset_energy + our_energy)


x = np.arange(len(experiments))
width = 0.35

fig, ax = plt.subplots()
rects1 = ax.bar(x-width/2, experiment_results["graphr-time"], width,
        label='GraphR')
rects2 = ax.bar(x+width/2, experiment_results["our-total-time"], width,
        label='Our')
ax.legend()
ax.set_yscale('log')
ax.set_xticks(x, experiments)
ax.bar_label(rects1, padding=3)
ax.bar_label(rects2, padding=3)

fig.tight_layout()
plt.savefig('time.png')

fig, ax = plt.subplots()
rects1 = ax.bar(x-width/2, experiment_results["graphr-energy"], width,
        label='GraphR')
rects2 = ax.bar(x+width/2, experiment_results["our-total-energy"], width,
        label='Our')

ax.legend()
ax.set_yscale('log')
ax.set_xticks(x, experiments)
ax.bar_label(rects1, padding=3)
ax.bar_label(rects2, padding=3)

fig.tight_layout()
plt.savefig('energy.png')

fig, ax = plt.subplots()
rects1 = ax.bar(x-width/2, experiment_results["graphr-efficiency"], width,
        label='GraphR')
rects2 = ax.bar(x+width/2, experiment_results["our-efficiency"], width,
        label='Our')

ax.legend()
ax.set_xticks(x, experiments)
ax.bar_label(rects1, padding=3)
ax.bar_label(rects2, padding=3)

fig.tight_layout()
plt.savefig('efficiency.png')
