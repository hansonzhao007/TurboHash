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

def PlotMiss(data, lines, latency, filename, islog):
    markers = ['x', '+', '.', 'o']
    colors = ('#2077B4', '#FF7F0E', '#2CA02C', '#D62728')
    styles = ['b-','r--','bo-','ro--']
    fig, ax2 = plt.subplots(figsize=(4, 3.6))
    # ax2 = ax.twinx()

    # plot bar
    data[latency].plot.bar(ax=ax2, alpha=0.8, color=("white", "white"), edgecolor='k', fontsize=9)
    bars = ax2.patches
    hatches = ''.join(h*len(data) for h in 'x .')
    for bar, hatch in zip(bars, hatches):
        bar.set_hatch(hatch)
    ax2.text(0.05, 0.97, "ns", 
        horizontalalignment='center',
        verticalalignment='center',
        transform = ax2.transAxes,
        fontsize=12)
    ticks_loc = ax2.get_yticks().tolist()
    ax2.yaxis.set_major_locator(mticker.FixedLocator(ticks_loc))
    ax2.yaxis.set_major_formatter(FormatStrFormatter('%d'))
    # For the minor ticks, use no labels; default NullFormatter.
    # ax2.yaxis.set_minor_locator(MultipleLocator(100))
    ax2.set_xticklabels(data.access_size.values, fontsize=16, rotation=0)
    ax2.tick_params(axis="y", direction="inout", pad=-28)
    ax2.legend(loc="upper center", fontsize=10, edgecolor='none')
    y_min, y_max = ax2.get_ylim()
    ax2.grid(axis='y', linestyle='-.', linewidth=0.5)
    ax2.set_ylim([1, y_max])
    ax2.set_axisbelow(True)
    
    # # plot line
    # data[lines].plot(ax=ax, style=styles, fillstyle='none', markersize=10, fontsize=10)
    # if islog:
    #     ax.set_yscale('log')
    # ax.set_xticklabels(data.access_size.values, fontsize=16)
    # ax.text(0.04, 0.95, "cache miss", 
    #     horizontalalignment='left',
    #     verticalalignment='center',
    #     transform = ax.transAxes,
    #     fontsize=12)
    # ax.set_zorder(ax2.get_zorder()+1)
    # ax.patch.set_visible(False)
    # ax.set_xlabel("Number of Accessed Blocks", fontsize=16)

    # # combine legend
    # legend_position = (-0.02, 1.03)
    # if not islog:
    #     legend_position = (-0.02, 0.84)
    # lines, labels = ax.get_legend_handles_labels()
    # lines2, labels2 = ax2.get_legend_handles_labels()
    # labels = ["Rnd_L1_miss", "Seq_L1_miss", "Rnd_Latency", "Seq_Latency"]
    # ax2.legend(lines + lines2, labels, bbox_to_anchor=legend_position, loc="upper left", fontsize=10, edgecolor='none')
    # ax.get_legend().remove()

    # ax2.grid(which='major', linestyle='--', zorder=0)
    # ax2.grid(which='minor', linestyle='--', zorder=0, linewidth=0.3)
    
    fig.savefig(filename, bbox_inches='tight', pad_inches=0 )


# Plot Dram
data = pd.read_csv('motivation_dram.csv', skipinitialspace=True)
print(data.dtypes)
r_lines = ['rnd_r_l1_miss', 'seq_r_l1_miss']
r_latency = ['rnd_read_latency', 'seq_read_latency']
r_miss_ratio = ['rnd_r_l1_miss_ratio', 'seq_r_l1_miss_ratio']
w_lines = ['rnd_w_l1_miss', 'seq_w_l1_miss']
w_latency = ['rnd_write_latency', 'seq_write_latency']
w_miss_ratio = ['rnd_w_l1_miss_ratio', 'seq_w_l1_miss_ratio']
PlotMiss(data, r_lines, r_latency, "motivation_read_dram.pdf", False)
PlotMiss(data, w_lines, w_latency, "motivation_write_dram.pdf", False)

# Plot pmem
data = pd.read_csv('motivation_pmem.csv', skipinitialspace=True)
print(data.dtypes)
r_lines = ['rnd_r_l1_miss', 'seq_r_l1_miss']
r_latency = ['rnd_read_latency', 'seq_read_latency']
r_miss_ratio = ['rnd_r_l1_miss_ratio', 'seq_r_l1_miss_ratio']
w_lines = ['rnd_w_l1_miss', 'seq_w_l1_miss']
w_latency = ['rnd_write_latency', 'seq_write_latency']
w_miss_ratio = ['rnd_w_l1_miss_ratio', 'seq_w_l1_miss_ratio']
PlotMiss(data, r_lines, r_latency, "motivation_read_pmem.pdf", False)
PlotMiss(data, w_lines, w_latency, "motivation_write_pmem.pdf", False)