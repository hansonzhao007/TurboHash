#/usr/bin/python3
import csv
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import mpl_toolkits.axisartist as axisartist
import numpy as np
from matplotlib.ticker import (MultipleLocator, FormatStrFormatter,
                               AutoMinorLocator)
# import matplotlib as mpl
plt.rcParams["font.family"] = "serif"
plt.rcParams['axes.linewidth'] = 1.2

hashtables = ['turbo', 'turbo30', 'clevel30', 'clht30']
legend_name = ('TURBO16', 'TURBO30', 'CLEVEL30', 'CLHT30')

markers= {
    'turbo'   : 'o',
    'turbo30' : '.', 
    'cceh'    : '|', 
    'cceh30'  : 'x', 
    'clevel30': 'd', 
    'clht30'  : ''}
colors= {
    'turbo'   : '#9B0522',
    'turbo30' : '#F37F82', 
    'cceh'    : '#83C047', 
    'cceh30'  : '#0E5932', 
    'clevel30': '#3182BD', 
    'clht30'  : '#BDBDBD'}
    
def Plot(filename, outfile, padding, title, ylabel, divide=1):
    df = pd.read_csv(filename)
    df.set_index('thread')
    df[hashtables] = df[hashtables] / divide
    print(df)
    fig, ax = plt.subplots(figsize=(4, 3.6))
    for i in hashtables:
        df.plot(
            ax=ax, 
            x='thread',
            y=i,
            linewidth=2,
            fontsize=14,
            marker=markers[i],
            markersize=11,
            fillstyle='none',
            color=colors[i])
    
    ax.legend(legend_name, fontsize=9, edgecolor='k',facecolor='w', framealpha=0, mode="expand", ncol=3, bbox_to_anchor=(-0.03, 0.92, 1.05, 0.1))
    # ax.legend(legend_name, fontsize=9, fancybox=True, framealpha=0.5, edgecolor='k')

    # set y ticks
    ymin, ymax = ax.get_ylim()
    ax.set_ylim([0.1, ymax*1.18])
    for label in ax.yaxis.get_ticklabels()[-2:]: # hidden last ticklabel
        label.set_visible(False)
    ax.tick_params(axis="y", direction="inout", pad=padding)
    for label in ax.yaxis.get_ticklabels():
        label.set_horizontalalignment('left')
    ax.set_ylabel(ylabel, fontsize=16)
    
    # set x ticks
    ax.set_xlim([-5, 42])
    ax.set_xlabel("Number of Threads", fontsize=16)

    # ax.set_title(title, fontsize=16)

    # draw v line
    # plt.axvline(x=20, color = 'grey', linestyle='--', linewidth=0.5)
    ax.yaxis.grid(linewidth=1, linestyle='--')
    
    fig.savefig(outfile, bbox_inches='tight', pad_inches=0.05)


def PlotScalability():
    # Plot throughput
    Plot("scalability_load.parse", "scalability_load.pdf", -8, "Write Throughput", "IOPS (Mops/s)")
    Plot("scalability_update.parse", "scalability_update.pdf", -10, "Update Throughput", "IOPS (Mops/s)")
    Plot("scalability_delete.parse", "scalability_delete.pdf", -10, "Delete Throughput", "IOPS (Mops/s)")
    
    # Plot IO
    Plot("scalability_load_io.parse", "scalability_load_io.pdf", -4, "", "Pmem I/O (GB)", 1024.0)
    Plot("scalability_update_io.parse", "scalability_update_io.pdf", -4, "", "Pmem I/O (GB)", 1024.0)
    Plot("scalability_delete_io.parse", "scalability_delete_io.pdf", -4, "", "Pmem I/O (GB)", 1024.0)
    
    # Plot bw
    Plot("scalability_load_bw.parse", "scalability_load_bw.pdf", -4, "", "Pmem Bandwidth (GB/s)", 1024.0)
    Plot("scalability_update_bw.parse", "scalability_update_bw.pdf", -4, "", "Pmem Bandwidth (GB/s)", 1024.0)
    Plot("scalability_delete_bw.parse", "scalability_delete_bw.pdf", -4, "", "Pmem Bandwidth (GB/s)", 1024.0)

PlotScalability()


