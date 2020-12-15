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
data = pd.read_csv('motivation1.csv', skipinitialspace=True)
print(data.dtypes)

r_lines = ['rnd_r_l1_miss', 'rnd_r_tlb_miss', 'seq_r_l1_miss', 'seq_r_tlb_miss']
r_latency = ['rnd_read_latency', 'seq_read_latency']
r_miss_ratio = ['rnd_r_l1_miss_ratio', 'rnd_r_tlb_miss_ratio', 'seq_r_l1_miss_ratio', 'seq_r_tlb_miss_ratio']

w_lines = ['rnd_w_l1_miss', 'rnd_w_tlb_miss', 'seq_w_l1_miss', 'seq_w_tlb_miss']
w_latency = ['rnd_write_latency', 'seq_write_latency']
w_miss_ratio = ['rnd_w_l1_miss_ratio', 'rnd_w_tlb_miss_ratio', 'seq_w_l1_miss_ratio', 'seq_w_tlb_miss_ratio']


data['rnd_r_l1_miss_ratio'] = data.rnd_r_l1_miss / data.rnd_r_l1_load
data['rnd_r_tlb_miss_ratio'] = data.rnd_r_tlb_miss / data.rnd_r_tlb_load
data['seq_r_l1_miss_ratio'] = data.seq_r_l1_miss / data.seq_r_l1_load
data['seq_r_tlb_miss_ratio'] = data.seq_r_tlb_miss / data.seq_r_tlb_load
# print(r_miss_ratio.dtypes)

data['rnd_w_l1_miss_ratio'] = data.rnd_w_l1_miss / data.rnd_w_l1_load
data['rnd_w_tlb_miss_ratio'] = data.rnd_w_tlb_miss / data.rnd_w_tlb_load
data['seq_w_l1_miss_ratio'] = data.seq_w_l1_miss / data.seq_w_l1_load
data['seq_w_tlb_miss_ratio'] = data.seq_w_tlb_miss / data.seq_w_tlb_load


def PlotMiss(data, lines, latency, filename, islog):
    markers = ['x', '+', '.', 'o']
    colors = ('#2077B4', '#FF7F0E', '#2CA02C', '#D62728')
    styles = ['bs-','rs--','bd-','rd--']
    fig, ax = plt.subplots(figsize=(4, 3.6))
    ax2 = ax.twinx()

    # plot bar
    data[latency].plot.bar(ax=ax2, alpha=0.8, color=("white", "white"), edgecolor='k', fontsize=12)
    bars = ax2.patches
    hatches = ''.join(h*len(data) for h in 'x .')
    for bar, hatch in zip(bars, hatches):
        bar.set_hatch(hatch)
    ax2.text(0.96, 0.97, "ns", 
        horizontalalignment='center',
        verticalalignment='center',
        transform = ax.transAxes,
        fontsize=12)
    start, end = ax2.get_ylim()
    ticks_loc = ax2.get_yticks().tolist()
    ax2.yaxis.set_major_locator(mticker.FixedLocator(ticks_loc))
    ax2.yaxis.set_major_formatter(FormatStrFormatter('%d'))
    # For the minor ticks, use no labels; default NullFormatter.
    # ax2.yaxis.set_minor_locator(MultipleLocator(100))
    ax2.tick_params(axis="y", direction="inout", pad=-24)
    ax2.set_yticklabels(["", "", "", "600", "", "", ""])
    ax2.set_xlim([-0.5, 5.8])
    ax2.set_axisbelow(True)
    
    # plot line
    data[lines].plot(ax=ax, style=styles, fillstyle='none', markersize=10, fontsize=12)
    if islog:
        ax.set_yscale('log')
    ax.set_xticklabels(data.access_size.values, fontsize=16)
    ax.text(-0.07, 0.97, "miss", 
        horizontalalignment='center',
        verticalalignment='center',
        transform = ax.transAxes,
        fontsize=12)
    ax.set_zorder(ax2.get_zorder()+1)
    ax.patch.set_visible(False)
    ax.set_xlabel("Number of Accessed Blocks", fontsize=16)

    # combine legend
    legend_position = (-0.02, 1.03)
    if not islog:
        legend_position = (-0.02, 0.84)
    lines, labels = ax.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    labels = ["R_L1", "R_TLB", "S_L1", "S_TLB", "R_Latency", "S_Latency"]
    ax2.legend(lines + lines2, labels, bbox_to_anchor=legend_position, loc="upper left", fontsize=10, edgecolor='none')
    ax.get_legend().remove()

    ax2.grid(which='major', linestyle='--', zorder=0)
    ax2.grid(which='minor', linestyle='--', zorder=0, linewidth=0.3)
    
    fig.savefig(filename, bbox_inches='tight', pad_inches=0 )


PlotMiss(data, r_lines, r_latency, "motivation_read.pdf", True)
PlotMiss(data, w_lines, w_latency, "motivation_write.pdf", True)

PlotMiss(data, r_miss_ratio, r_latency, "motivation_read_ratio.pdf", False)
PlotMiss(data, w_miss_ratio, w_latency, "motivation_write_ratio.pdf", False)