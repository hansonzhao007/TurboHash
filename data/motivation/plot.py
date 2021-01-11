#/usr/bin/python3
import csv
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import numpy as np
from matplotlib.ticker import (MultipleLocator, FormatStrFormatter,
                               AutoMinorLocator)
# import matplotlib as mpl
plt.rcParams["font.family"] = "serif"
plt.rcParams['axes.linewidth'] = 1.2

def PlotMiss(data, bw, r_w, latency, filename, islog):
    markers = ['x', '+', '.', 'o']
    colors = ('#2077B4', '#FF7F0E', '#2CA02C', '#D62728')
    styles = ['b-','r--','bo-','ro--']

    fig, axes = plt.subplots(ncols=2, sharey=True)
    ax2 = axes[1]
    # Plot bar
    data[latency].plot.barh(ax=ax2, alpha=0.8, color=("white", "grey"), edgecolor='k', fontsize=14)
    bars = ax2.patches
    hatches = ''.join(h*len(data) for h in " x")
    for bar, hatch in zip(bars, hatches):
        bar.set_hatch(hatch)
    ax2.grid(axis='x', linestyle='-.', linewidth=0.5)
    ax2.set_axisbelow(True)
    ax2.set_xlabel("Latency (ns)", fontsize=16)
    ax2.legend(loc="upper right", fontsize=11, framealpha=1, edgecolor='white')

    # Plot bw
    rnd_r = "rnd_" + r_w + "_r"
    rnd_w = "rnd_" + r_w + "_w"
    seq_r = "seq_" + r_w + "_r"
    seq_w = "seq_" + r_w + "_w"
    ax = axes[0]
    bw['rnd_bandwidth'] = bw[rnd_r] + bw[rnd_w]
    bw['seq_bandwidth'] = bw[seq_r] + bw[seq_w]
    bw[['rnd_bandwidth', 'seq_bandwidth']].plot.barh(ax=ax, alpha=0.8, color=("white", "grey"), edgecolor='k', fontsize=14)
    bars = ax.patches
    hatches = ''.join(h*len(data) for h in " x")
    for bar, hatch in zip(bars, hatches):
        bar.set_hatch(hatch)
    ax.grid(axis='x', linestyle='-.', linewidth=0.5)
    ax.set_axisbelow(True)
    ax.set_xlabel("Bandwidth (GB/s)", fontsize=16)
    # ax.legend(["Read Bandwidth", "Write Bandwidth"], loc="upper left", fontsize=10, framealpha=0)
    ax.set(yticklabels=data.access_size.values)
    ax.tick_params(axis='y', labelsize=16)

    axes[0].invert_xaxis()
    axes[0].invert_yaxis()
    axes[0].yaxis.tick_right()
    fig.subplots_adjust(wspace=0.17)

    fig.savefig(filename, bbox_inches='tight', pad_inches=0.05)


# Plot Dram
data = pd.read_csv('motivation_dram.csv', skipinitialspace=True)
bw = pd.read_csv('motivation_dram_bw.csv', skipinitialspace=True)
bw = bw / 1000.0
print(bw)
r_latency = ['rnd_read_latency', 'seq_read_latency']

w_latency = ['rnd_write_latency', 'seq_write_latency']
PlotMiss(data, bw, "read",  r_latency, "motivation_read_dram.pdf",  False)
PlotMiss(data, bw, "write", w_latency, "motivation_write_dram.pdf", False)

# Plot pmem
data = pd.read_csv('motivation_pmem.csv', skipinitialspace=True)
bw = pd.read_csv('motivation_pmem_bw.csv', skipinitialspace=True)
bw = bw / 1000.0
print(bw)
r_latency = ['rnd_read_latency', 'seq_read_latency']
w_latency = ['rnd_write_latency', 'seq_write_latency']
PlotMiss(data, bw, "read",  r_latency, "motivation_read_pmem.pdf",  False)
PlotMiss(data, bw, "write", w_latency, "motivation_write_pmem.pdf", False)

