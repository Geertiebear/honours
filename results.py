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

sssp_results = {
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

bfs_results = {
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

pagerank_results = {
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
#     sssp_results["graphr-time"].append(graphr_time)
#     sssp_results["graphr-energy"].append(graphr_energy)
#     sssp_results["graphr-efficiency"].append(graphr_efficiency)
# 
#     [our_time, our_energy, our_efficiency, our_periphery_time, our_periphery_energy] = values_from_file(experiment + "-ours.log", "ours")
#     sssp_results["our-time"].append(our_time)
#     sssp_results["our-energy"].append(our_energy)
#     sssp_results["our-efficiency"].append(our_efficiency)
# 
#     [our_offset_time, our_offset_energy, _, _, _] = values_from_file(experiment + "-ours-offsets.log", "offsets")
#     sssp_results["our-offsets-time"].append(our_offset_time)
#     sssp_results["our-offsets-energy"].append(our_offset_energy)
# 
#     our_total_energy = our_offset_energy + our_energy + our_periphery_energy
#     our_total_time = our_offset_time + our_time + our_periphery_time
# 
#     graphr_total_energy = graphr_energy + graphr_periphery_energy
#     graphr_total_time = graphr_time + graphr_periphery_time
# 
#     sssp_results["our-total-time"].append(our_offset_time + our_time)
#     sssp_results["our-total-energy"].append(our_offset_energy + our_energy)
# 
#     sssp_results["normal-graphr-time"].append(graphr_total_time / our_total_time)
#     sssp_results["normal-graphr-energy"].append(graphr_total_energy / our_total_energy)
#     sssp_results["normal-graphr-efficiency"].append(graphr_efficiency / our_efficiency)

# Wiki-Vote

sssp_results["our-total-time"].append(0.009225)
sssp_results["our-total-energy"].append(0.000233)
sssp_results["our-efficiency"].append(0.001673)
sssp_results["graphr-total-time"].append(0.158245)
sssp_results["graphr-total-energy"].append(0.044459)
sssp_results["graphr-efficiency"].append(0.001673)
sssp_results["gpu-total-time"].append(19.924*10**-3)
sssp_results["gpu-total-energy"].append(57.33*19.924*10**-3)

#amazon

sssp_results["our-total-time"].append(0.115538)
sssp_results["our-total-energy"].append(0.000689)
sssp_results["our-efficiency"].append(0.000378)
sssp_results["graphr-total-time"].append(2.627140)
sssp_results["graphr-total-energy"].append(0.832344)
sssp_results["graphr-efficiency"].append(0.000378)
sssp_results["gpu-total-time"].append(92247.1*10**-3)
sssp_results["gpu-total-energy"].append(104*92247.1*10**-3)

# soc-Slashdot 

sssp_results["our-total-time"].append(0.208975)
sssp_results["our-total-energy"].append(0.001167)
sssp_results["our-efficiency"].append(0.000845)
sssp_results["graphr-total-time"].append(3.878714)
sssp_results["graphr-total-energy"].append(0.762019)
sssp_results["graphr-efficiency"].append(0.000845)
sssp_results["gpu-total-time"].append(1475.75*10**-3)
sssp_results["gpu-total-energy"].append(99.52*1475.75*10**-3)

# soc-Epinions 

sssp_results["our-total-time"].append(0.126085)
sssp_results["our-total-energy"].append(0.001007)
sssp_results["our-efficiency"].append(0.000838)
sssp_results["graphr-total-time"].append(2.524755)
sssp_results["graphr-total-energy"].append(0.611421)
sssp_results["graphr-efficiency"].append(0.000838)
sssp_results["gpu-total-time"].append(1307.6*10**-3)
sssp_results["gpu-total-energy"].append(98*1307.6*10**-3)

# com-Orkut

sssp_results["our-total-time"].append(1.400289)
sssp_results["our-total-energy"].append(0.005045)
sssp_results["our-efficiency"].append(0.000442)
sssp_results["graphr-total-time"].append(29.141508)
sssp_results["graphr-total-energy"].append(6.826764)
sssp_results["graphr-efficiency"].append(0.000442)
sssp_results["gpu-total-time"].append(1.50936*10**3)
sssp_results["gpu-total-energy"].append(111*1.50936*10**3)

# Wiki-Vote

bfs_results["our-total-time"].append(0.008744)
bfs_results["our-total-energy"].append(0.000131)
bfs_results["our-efficiency"].append(0.001673)
bfs_results["graphr-total-time"].append(0.090107)
bfs_results["graphr-total-energy"].append(0.002497)
bfs_results["graphr-efficiency"].append(0.001673)
bfs_results["gpu-total-time"].append(19.924*10**-3)
bfs_results["gpu-total-energy"].append(57.33*19.924*10**-3)

#amazon

bfs_results["our-total-time"].append(0.109778)
bfs_results["our-total-energy"].append(0.000656)
bfs_results["our-efficiency"].append(0.000378)
bfs_results["graphr-total-time"].append(1.811297)
bfs_results["graphr-total-energy"].append(0.048653)
bfs_results["graphr-efficiency"].append(0.000378)
bfs_results["gpu-total-time"].append(92247.1*10**-3)
bfs_results["gpu-total-energy"].append(104*92247.1*10**-3)

# soc-Slashdot 

bfs_results["our-total-time"].append(0.189453)
bfs_results["our-total-energy"].append(0.000656)
bfs_results["our-efficiency"].append(0.000845)
bfs_results["graphr-total-time"].append(1.111653)
bfs_results["graphr-total-energy"].append(0.036200)
bfs_results["graphr-efficiency"].append(0.000845)
bfs_results["gpu-total-time"].append(1475.75*10**-3)
bfs_results["gpu-total-energy"].append(99.52*1475.75*10**-3)

# soc-Epinions 

bfs_results["our-total-time"].append(0.116103)
bfs_results["our-total-energy"].append(0.000566)
bfs_results["our-efficiency"].append(0.000838)
bfs_results["graphr-total-time"].append(1.109890)
bfs_results["graphr-total-energy"].append(0.032372)
bfs_results["graphr-efficiency"].append(0.000838)
bfs_results["gpu-total-time"].append(1307.6*10**-3)
bfs_results["gpu-total-energy"].append(98*1307.6*10**-3)

# com-Orkut

bfs_results["our-total-time"].append(1.279167)
bfs_results["our-total-energy"].append(0.002835)
bfs_results["our-efficiency"].append(0.000442)
bfs_results["graphr-total-time"].append(12.042871)
bfs_results["graphr-total-energy"].append(0.356048)
bfs_results["graphr-efficiency"].append(0.000442)
bfs_results["gpu-total-time"].append(1.50936*10**3)
bfs_results["gpu-total-energy"].append(111*1.50936*10**3)

# Wiki-Vote

pagerank_results["our-total-time"].append(0.012632)
pagerank_results["our-total-energy"].append(0.000019)
pagerank_results["our-efficiency"].append(0.001673)
pagerank_results["graphr-total-time"].append(0.013633)
pagerank_results["graphr-total-energy"].append(0.002614)
pagerank_results["graphr-efficiency"].append(1.000000)
pagerank_results["gpu-total-time"].append(249.319/18 * 10**-3)
pagerank_results["gpu-total-energy"].append(60 * 239.319/18 * 10**-3)

#amazon

pagerank_results["our-total-time"].append(0.042884)
pagerank_results["our-total-energy"].append(0.000015)
pagerank_results["our-efficiency"].append(0.000378)
pagerank_results["graphr-total-time"].append(0.069811)
pagerank_results["graphr-total-energy"].append(0.013691)
pagerank_results["graphr-efficiency"].append(1.000000)
pagerank_results["gpu-total-time"].append(34359.5 / 25 * 10**-3)
pagerank_results["gpu-total-energy"].append(61 * 34359.5 / 25 * 10**-3)

# soc-Slashdot 

pagerank_results["our-total-time"].append(0.145478)
pagerank_results["our-total-energy"].append(0.000112)
pagerank_results["our-efficiency"].append(0.000845)
pagerank_results["graphr-total-time"].append(0.175734)
pagerank_results["graphr-total-energy"].append(0.033934)
pagerank_results["graphr-efficiency"].append(1.000000)
pagerank_results["gpu-total-time"].append(7590.39 / 22 * 10**-3)
pagerank_results["gpu-total-energy"].append(60.8 * 7590.39 / 22 * 10**-3)

# soc-Epinions 

pagerank_results["our-total-time"].append(0.095046)
pagerank_results["our-total-energy"].append(0.000072)
pagerank_results["our-efficiency"].append(0.000838)
pagerank_results["graphr-total-time"].append(0.139022)
pagerank_results["graphr-total-energy"].append(0.027129)
pagerank_results["graphr-efficiency"].append(1.000000)
pagerank_results["gpu-total-time"].append(7751.71 / 34 * 10**-3)
pagerank_results["gpu-total-energy"].append(61 * 7751.71 / 34 * 10**-3)

# com-Orkut

pagerank_results["our-total-time"].append(1)
pagerank_results["our-total-energy"].append(1)
pagerank_results["our-efficiency"].append(1)
pagerank_results["graphr-total-time"].append(1)
pagerank_results["graphr-total-energy"].append(1)
pagerank_results["graphr-efficiency"].append(1)
pagerank_results["gpu-total-time"].append(1)
pagerank_results["gpu-total-energy"].append(1)

for i in range(len(experiments)):
    graphr_normal_time = sssp_results["gpu-total-time"][i] / \
    sssp_results["graphr-total-time"][i]
    graphr_normal_energy = sssp_results["gpu-total-energy"][i] / \
    sssp_results["graphr-total-energy"][i]
    graphr_normal_efficiency = sssp_results["graphr-efficiency"][i] / \
    sssp_results["our-efficiency"][i]

    sssp_results["normal-graphr-time"].append(graphr_normal_time)
    sssp_results["normal-graphr-energy"].append(graphr_normal_energy)
    sssp_results["normal-graphr-efficiency"].append(graphr_normal_efficiency)

    gpu_normal_time = sssp_results["gpu-total-time"][i] / \
    sssp_results["our-total-time"][i]
    gpu_normal_energy = sssp_results["gpu-total-energy"][i] / \
    sssp_results["our-total-energy"][i]

    sssp_results["normal-graphsar-time"].append(graphr_normal_time * \
            graphsar_time_factor)
    sssp_results["normal-graphsar-energy"].append(graphr_normal_energy * \
            graphsar_energy_factor)

    sssp_results["normal-gpu-time"].append(gpu_normal_time)
    sssp_results["normal-gpu-energy"].append(gpu_normal_energy)

    graphr_normal_time = bfs_results["gpu-total-time"][i] / \
    bfs_results["graphr-total-time"][i]
    graphr_normal_energy = bfs_results["gpu-total-energy"][i] / \
    bfs_results["graphr-total-energy"][i]
    graphr_normal_efficiency = bfs_results["graphr-efficiency"][i] / \
    bfs_results["our-efficiency"][i]

    bfs_results["normal-graphr-time"].append(graphr_normal_time)
    bfs_results["normal-graphr-energy"].append(graphr_normal_energy)
    bfs_results["normal-graphr-efficiency"].append(graphr_normal_efficiency)

    gpu_normal_time = bfs_results["gpu-total-time"][i] / \
    bfs_results["our-total-time"][i]
    gpu_normal_energy = bfs_results["gpu-total-energy"][i] / \
    bfs_results["our-total-energy"][i]

    bfs_results["normal-graphsar-time"].append(graphr_normal_time * \
            graphsar_time_factor)
    bfs_results["normal-graphsar-energy"].append(graphr_normal_energy * \
            graphsar_energy_factor)

    bfs_results["normal-gpu-time"].append(gpu_normal_time)
    bfs_results["normal-gpu-energy"].append(gpu_normal_energy)

    graphr_normal_time = pagerank_results["gpu-total-time"][i] / \
    pagerank_results["graphr-total-time"][i]
    graphr_normal_energy = pagerank_results["gpu-total-energy"][i] / \
    pagerank_results["graphr-total-energy"][i]
    graphr_normal_efficiency = pagerank_results["graphr-efficiency"][i] / \
    pagerank_results["our-efficiency"][i]

    pagerank_results["normal-graphr-time"].append(graphr_normal_time)
    pagerank_results["normal-graphr-energy"].append(graphr_normal_energy)
    pagerank_results["normal-graphr-efficiency"].append(graphr_normal_efficiency)

    gpu_normal_time = pagerank_results["gpu-total-time"][i] / \
    pagerank_results["our-total-time"][i]
    gpu_normal_energy = pagerank_results["gpu-total-energy"][i] / \
    pagerank_results["our-total-energy"][i]

    pagerank_results["normal-graphsar-time"].append(graphr_normal_time * \
            graphsar_time_factor)
    pagerank_results["normal-graphsar-energy"].append(graphr_normal_energy * \
            graphsar_energy_factor)

    pagerank_results["normal-gpu-time"].append(gpu_normal_time)
    pagerank_results["normal-gpu-energy"].append(gpu_normal_energy)

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
rects1 = ax.bar(x, sssp_results["normal-graphr-time"], width,
        label='GraphR', color=graph_colors['graphr'])
rects2 = ax.bar(x+width, normals, width,
        label='GPU', color=graph_colors['gpu'])
rects3 = ax.bar(x+2*width, sssp_results["normal-gpu-time"], width,
        label='SparseMEM', color=graph_colors['our'])
rects4 = ax.bar(x+3*width, sssp_results["normal-graphsar-time"], width,
        label='GraphSAR', color=graph_colors['graphsar'])
ax.legend()
ax.set_yscale('log')
ax.set_xticks(x + 2*width, experiments)
ax.set_xlabel("Dataset")
ax.set_ylabel("Speedup")

fig.tight_layout()
plt.savefig('sssp_time.pdf')

fig, ax = plt.subplots()
rects1 = ax.bar(x, sssp_results["normal-graphr-energy"], width,
        label='GraphR', color=graph_colors['graphr'])
rects2 = ax.bar(x+width, normals, width,
        label='GPU', color=graph_colors['gpu'])
rects3 = ax.bar(x+2*width, sssp_results["normal-gpu-energy"], width,
        label='SparseMEM', color=graph_colors['our'])
rects4 = ax.bar(x+3*width, sssp_results["normal-graphsar-energy"], width,
        label='GraphSAR', color=graph_colors['graphsar'])
ax.legend()
ax.set_yscale('log')
ax.set_xticks(x + 2*width, experiments)
ax.set_xlabel("Dataset")
ax.set_ylabel("Energy improvement")
ax.set_title("Normalized energy difference per dataset - SSSP")

fig.tight_layout()
plt.savefig('sssp_energy.pdf')

fig, ax = plt.subplots()
rects1 = ax.bar(x-width/2, sssp_results["normal-graphr-efficiency"], width,
        label='GraphR', color=graph_colors['graphr'])
rects2 = ax.bar(x+width/2, normals, width,
        label='Our', color=graph_colors['our'])

ax.legend()
ax.set_xticks(x, experiments)
ax.set_xlabel("Dataset")
ax.set_ylabel("Efficiency")

fig.tight_layout()
plt.savefig('sssp_efficiency.pdf')

fig, ax = plt.subplots()
rects1 = ax.bar(x, bfs_results["normal-graphr-time"], width,
        label='GraphR', color=graph_colors['graphr'])
rects2 = ax.bar(x+width, normals, width,
        label='GPU', color=graph_colors['gpu'])
rects3 = ax.bar(x+2*width, bfs_results["normal-gpu-time"], width,
        label='SparseMEM', color=graph_colors['our'])
rects4 = ax.bar(x+3*width, bfs_results["normal-graphsar-time"], width,
        label='GraphSAR', color=graph_colors['graphsar'])
ax.legend()
ax.set_yscale('log')
ax.set_xticks(x + 2*width, experiments)
ax.set_xlabel("Dataset")
ax.set_ylabel("Speedup")
ax.set_title("Normalized time difference per dataset - BFS")

fig.tight_layout()
plt.savefig('bfs_time.pdf')

fig, ax = plt.subplots()
rects1 = ax.bar(x, bfs_results["normal-graphr-energy"], width,
        label='GraphR', color=graph_colors['graphr'])
rects2 = ax.bar(x+width, normals, width,
        label='GPU', color=graph_colors['gpu'])
rects3 = ax.bar(x+2*width, bfs_results["normal-gpu-energy"], width,
        label='SparseMEM', color=graph_colors['our'])
rects4 = ax.bar(x+3*width, bfs_results["normal-graphsar-energy"], width,
        label='GraphSAR', color=graph_colors['graphsar'])
ax.legend()
ax.set_yscale('log')
ax.set_xticks(x + 2*width, experiments)
ax.set_xlabel("Dataset")
ax.set_ylabel("Energy improvement")

fig.tight_layout()
plt.savefig('bfs_energy.pdf')

fig, ax = plt.subplots()
rects1 = ax.bar(x-width/2, sssp_results["normal-graphr-efficiency"], width,
        label='GraphR', color=graph_colors['graphr'])
rects2 = ax.bar(x+width/2, normals, width,
        label='Our', color=graph_colors['our'])

fig, ax = plt.subplots()
rects1 = ax.bar(x-width/2, bfs_results["normal-graphr-efficiency"], width,
        label='GraphR', color=graph_colors['graphr'])
rects2 = ax.bar(x+width/2, normals, width,
        label='Our', color=graph_colors['our'])

ax.legend()
ax.set_xticks(x, experiments)
ax.set_xlabel("Dataset")
ax.set_ylabel("Efficiency")

fig.tight_layout()
plt.savefig('bfs_efficiency.pdf')

fig, ax = plt.subplots()
rects1 = ax.bar(x, pagerank_results["normal-graphr-time"], width,
        label='GraphR', color=graph_colors['graphr'])
rects2 = ax.bar(x+width, normals, width,
        label='GPU', color=graph_colors['gpu'])
rects3 = ax.bar(x+2*width, pagerank_results["normal-gpu-time"], width,
        label='SparseMEM', color=graph_colors['our'])
rects4 = ax.bar(x+3*width, pagerank_results["normal-graphsar-time"], width,
        label='GraphSAR', color=graph_colors['graphsar'])
ax.legend()
ax.set_yscale('log')
ax.set_xticks(x + 2*width, experiments)
ax.set_xlabel("Dataset")
ax.set_ylabel("Speedup")

fig.tight_layout()
plt.savefig('pagerank_time.pdf')

fig, ax = plt.subplots()
rects1 = ax.bar(x, pagerank_results["normal-graphr-energy"], width,
        label='GraphR', color=graph_colors['graphr'])
rects2 = ax.bar(x+width, normals, width,
        label='GPU', color=graph_colors['gpu'])
rects3 = ax.bar(x+2*width, pagerank_results["normal-gpu-energy"], width,
        label='SparseMEM', color=graph_colors['our'])
rects4 = ax.bar(x+3*width, pagerank_results["normal-graphsar-energy"], width,
        label='GraphSAR', color=graph_colors['graphsar'])
ax.legend()
ax.set_yscale('log')
ax.set_xticks(x + 2*width, experiments)
ax.set_xlabel("Dataset")
ax.set_ylabel("Energy improvement")

fig.tight_layout()
plt.savefig('pagerank_energy.pdf')

fig, ax = plt.subplots()
rects1 = ax.bar(x-width/2, pagerank_results["normal-graphr-efficiency"], width,
        label='GraphR', color=graph_colors['graphr'])
rects2 = ax.bar(x+width/2, normals, width,
        label='Our', color=graph_colors['our'])

ax.legend()
ax.set_xticks(x, experiments)
ax.set_xlabel("Dataset")
ax.set_ylabel("Efficiency")

fig.tight_layout()
plt.savefig('pagerank_efficiency.pdf')
