import sys
import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns
from parse import *

read_time = 10 * 10**-9
write_time = 100 * 10**-9
adc_latency = 1 * 10**-9
read_energy = 40 * 10**-15
write_energy = 20 * 10**-12
adc_energy = 2 * 10**-12
sa_latency = 1 * 10**-9
sa_energy = 0.01 * 10**-12

experiments = ["WV", "AZ",
       "SD", "EP", "OK"]
#experiments = ["wiki-Vote", "web-Google", "soc-Slashdot0902", "amazon0302"]

experiment_results = {
        "graphr-total-time": [],
        "graphr-total-energy": [],
        "graphr-efficiency": [],
        "our-time": [],
        "our-energy": [],
        "our-efficiency": [],
        "our-offsets-time" : [],
        "our-offsets-energy": [],
        "our-total-time": [],
        "our-total-energy": [],
        "gpu-total-time": [],
        "gpu-total-energy": [],
        "normal-gpu-time": [],
        "normal-gpu-energy": [],
        "normal-graphsar-time": [],
        "normal-graphsar-energy": [],
        "normal-graphr-time": [],
        "normal-graphr-energy": [],
        "normal-graphr-efficiency": []}

graphsar_time_factor = 1.85
graphsar_energy_factor = 4.43

# for experiment in experiments:
#     [graphr_time, graphr_energy, graphr_efficiency, graphr_periphery_time, graphr_periphery_energy] = values_from_file(experiment + "-graphr.log", "graphr")
#     experiment_results["graphr-time"].append(graphr_time)
#     experiment_results["graphr-energy"].append(graphr_energy)
#     experiment_results["graphr-efficiency"].append(graphr_efficiency)
# 
#     [our_time, our_energy, our_efficiency, our_periphery_time, our_periphery_energy] = values_from_file(experiment + "-ours.log", "ours")
#     experiment_results["our-time"].append(our_time)
#     experiment_results["our-energy"].append(our_energy)
#     experiment_results["our-efficiency"].append(our_efficiency)
# 
#     [our_offset_time, our_offset_energy, _, _, _] = values_from_file(experiment + "-ours-offsets.log", "offsets")
#     experiment_results["our-offsets-time"].append(our_offset_time)
#     experiment_results["our-offsets-energy"].append(our_offset_energy)
# 
#     our_total_energy = our_offset_energy + our_energy + our_periphery_energy
#     our_total_time = our_offset_time + our_time + our_periphery_time
# 
#     graphr_total_energy = graphr_energy + graphr_periphery_energy
#     graphr_total_time = graphr_time + graphr_periphery_time
# 
#     experiment_results["our-total-time"].append(our_offset_time + our_time)
#     experiment_results["our-total-energy"].append(our_offset_energy + our_energy)
# 
#     experiment_results["normal-graphr-time"].append(graphr_total_time / our_total_time)
#     experiment_results["normal-graphr-energy"].append(graphr_total_energy / our_total_energy)
#     experiment_results["normal-graphr-efficiency"].append(graphr_efficiency / our_efficiency)

# Wiki-Vote

experiment_results["our-total-time"].append(0.009225)
experiment_results["our-total-energy"].append(0.000233)
experiment_results["our-efficiency"].append(0.001673)
experiment_results["graphr-total-time"].append(0.158245)
experiment_results["graphr-total-energy"].append(0.044459)
experiment_results["graphr-efficiency"].append(0.001673)
experiment_results["gpu-total-time"].append(19.924*10**-3)
experiment_results["gpu-total-energy"].append(57.33*19.924*10**-3)

#amazon

experiment_results["our-total-time"].append(0.115538)
experiment_results["our-total-energy"].append(0.000689)
experiment_results["our-efficiency"].append(0.000378)
experiment_results["graphr-total-time"].append(2.627140)
experiment_results["graphr-total-energy"].append(0.832344)
experiment_results["graphr-efficiency"].append(0.000378)
experiment_results["gpu-total-time"].append(92247.1*10**-3)
experiment_results["gpu-total-energy"].append(104*92247.1*10**-3)

# soc-Slashdot 

experiment_results["our-total-time"].append(0.208975)
experiment_results["our-total-energy"].append(0.001167)
experiment_results["our-efficiency"].append(0.000845)
experiment_results["graphr-total-time"].append(3.878714)
experiment_results["graphr-total-energy"].append(0.762019)
experiment_results["graphr-efficiency"].append(0.000845)
experiment_results["gpu-total-time"].append(1475.75*10**-3)
experiment_results["gpu-total-energy"].append(99.52*1475.75*10**-3)

# soc-Epinions 

experiment_results["our-total-time"].append(0.126085)
experiment_results["our-total-energy"].append(0.001007)
experiment_results["our-efficiency"].append(0.000838)
experiment_results["graphr-total-time"].append(2.524755)
experiment_results["graphr-total-energy"].append(0.611421)
experiment_results["graphr-efficiency"].append(0.000838)
experiment_results["gpu-total-time"].append(1307.6*10**-3)
experiment_results["gpu-total-energy"].append(98*1307.6*10**-3)

# com-Orkut

experiment_results["our-total-time"].append(1.400289)
experiment_results["our-total-energy"].append(0.005045)
experiment_results["our-efficiency"].append(0.000442)
experiment_results["graphr-total-time"].append(29.141508)
experiment_results["graphr-total-energy"].append(6.826764)
experiment_results["graphr-efficiency"].append(0.000442)
experiment_results["gpu-total-time"].append(1.50936*10**3)
experiment_results["gpu-total-energy"].append(111*1.50936*10**3)


for i in range(len(experiments)):
    graphr_normal_time = experiment_results["graphr-total-time"][i] / \
    experiment_results["our-total-time"][i]
    graphr_normal_energy = experiment_results["graphr-total-energy"][i] / \
    experiment_results["our-total-energy"][i]
    graphr_normal_efficiency = experiment_results["graphr-efficiency"][i] / \
    experiment_results["our-efficiency"][i]

    experiment_results["normal-graphr-time"].append(graphr_normal_time)
    experiment_results["normal-graphr-energy"].append(graphr_normal_energy)
    experiment_results["normal-graphr-efficiency"].append(graphr_normal_efficiency)

    gpu_normal_time = experiment_results["gpu-total-time"][i] / \
    experiment_results["our-total-time"][i]
    gpu_normal_energy = experiment_results["gpu-total-energy"][i] / \
    experiment_results["our-total-energy"][i]

    experiment_results["normal-graphsar-time"].append(graphr_normal_time / \
            graphsar_time_factor)
    experiment_results["normal-graphsar-energy"].append(graphr_normal_energy / \
            graphsar_energy_factor)

    experiment_results["normal-gpu-time"].append(gpu_normal_time)
    experiment_results["normal-gpu-energy"].append(gpu_normal_energy)

colors = sns.color_palette("gist_stern")
graph_colors = {
    'our': colors[0],
    'graphr': colors[1],
    'gpu': colors[2],
    'graphsar': colors[3]
}

width = 0.20
x = np.arange(len(experiments))
normals = [1] * len(experiments)

fig, ax = plt.subplots()
rects1 = ax.bar(x, experiment_results["normal-graphr-time"], width,
        label='GraphR', color=graph_colors['graphr'])
rects2 = ax.bar(x+width, normals, width,
        label='SparseMEM', color=graph_colors['our'])
rects3 = ax.bar(x+2*width, experiment_results["normal-gpu-time"], width,
        label='GPU', color=graph_colors['gpu'])
rects4 = ax.bar(x+3*width, experiment_results["normal-graphsar-time"], width,
        label='GraphSAR', color=graph_colors['graphsar'])
ax.legend()
ax.set_yscale('log')
ax.set_xticks(x + 2*width, experiments)
ax.set_xlabel("Dataset")
ax.set_ylabel("Speedup")
ax.set_title("Normalized time difference per dataset")

fig.tight_layout()
plt.savefig('time.pdf')

fig, ax = plt.subplots()
rects1 = ax.bar(x, experiment_results["normal-graphr-energy"], width,
        label='GraphR', color=graph_colors['graphr'])
rects2 = ax.bar(x+width, normals, width,
        label='Our', color=graph_colors['our'])
rects3 = ax.bar(x+2*width, experiment_results["normal-gpu-energy"], width,
        label='GPU', color=graph_colors['gpu'])
rects4 = ax.bar(x+3*width, experiment_results["normal-graphsar-energy"], width,
        label='GraphSAR', color=graph_colors['graphsar'])
ax.legend()
ax.set_yscale('log')
ax.set_xticks(x + 2*width, experiments)
ax.set_xlabel("Dataset")
ax.set_ylabel("Energy improvement")
ax.set_title("Normalized energy difference per dataset")

fig.tight_layout()
plt.savefig('energy.pdf')

fig, ax = plt.subplots()
rects1 = ax.bar(x-width/2, experiment_results["normal-graphr-efficiency"], width,
        label='GraphR', color=graph_colors['graphr'])
rects2 = ax.bar(x+width/2, normals, width,
        label='Our', color=graph_colors['our'])

ax.legend()
ax.set_xticks(x, experiments)
ax.set_xlabel("Dataset")
ax.set_ylabel("Efficiency")
ax.set_title("Normalized efficiency difference per dataset")

fig.tight_layout()
plt.savefig('efficiency.pdf')
