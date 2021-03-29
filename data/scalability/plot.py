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
plt.rcParams['axes.linewidth'] = 2

hashtables = ['turbo', 'cceh', 'dash', 'turbo30', 'cceh30', 'clevel30', 'clht30']
legend_name = ('TURBO16', 'CCEH16', 'DASH16', 'TURBO30', 'CCEH30', 'CLEVEL30', 'CLHT30')

markers= {
    'turbo'   : 'o',     
    'cceh'    : '|',
    'dash'    : '^',
    'turbo30' : '.',
    'cceh30'  : 'x', 
    'clevel30': 'd', 
    'clht30'  : ''}

dashes= {
    'turbo'   : [2, 0],
    'cceh'    : [2, 0],
    'dash'    : [2, 0],
    'turbo30' : [3, 2],
    'cceh30'  : [3, 2],
    'clevel30': [3, 2],
    'clht30'  : [3, 2]}

colors= {
    'turbo'   : '#9B0522',     
    'cceh'    : '#83C047',
    'dash'    : '#f7cd6b',
    'turbo30' : '#F37F82',
    'cceh30'  : '#7e72b5', 
    'clevel30': '#3182BD', 
    'clht30'  : '#808084'}
    
def Plot(filename, outfile, padding, title, ylabel, divide=1):
    df = pd.read_csv(filename)
    df.set_index('thread')
    df[hashtables] = df[hashtables] / divide
    print(df)
    fig, ax = plt.subplots(figsize=(4, 3.6), sharex=True, squeeze=True)
    for i in hashtables:
        df.plot(
            ax=ax, 
            x='thread',
            y=i,
            linewidth=2,
            fontsize=14,
            marker=markers[i],
            dashes=dashes[i],
            markersize=8,
            fillstyle='none',
            color=colors[i])
    
    ax.legend(legend_name, fontsize=9, edgecolor='k',facecolor='w', framealpha=0, mode="expand", ncol=3, bbox_to_anchor=(-0.01, 0.765, 1.03, 0.1))
    # ax.legend(legend_name, fontsize=9, fancybox=True, framealpha=0.5, edgecolor='k')

    # set y ticks
    ymin, ymax = ax.get_ylim()
    ax.set_ylim([0.1, ymax*1.18])
    for label in ax.yaxis.get_ticklabels()[-3:]: # hidden last ticklabel
        label.set_visible(False)
    ax.tick_params(axis="y", direction="inout", pad=padding)
    for label in ax.yaxis.get_ticklabels():
        label.set_horizontalalignment('left')
    ax.set_ylabel(ylabel, fontsize=16)
    
    # set x ticks
    ax.set_xlim([-5, 42])
    ax.set_xlabel("Number of Threads", fontsize=16)

    # hide top edge
    # ax.spines['top'].set_visible(False)
        
    # ax.yaxis.grid(linewidth=1, linestyle='--')
    fig.savefig(outfile, bbox_inches='tight', pad_inches=0.05)


def PlotScalability():
    # Plot throughput
    Plot("scalability_load.parse", "scalability_load.pdf", -8, "Write Throughput", "IOPS (Mops/s)")
    Plot("scalability_read.parse", "scalability_read.pdf", -10, "Positive Read Throughput", "IOPS (Mops/s)")
    Plot("scalability_readnon.parse", "scalability_readnon.pdf", -10, "Negative Read Throughput", "IOPS (Mops/s)")
    
    # Plot IO
    Plot("scalability_load_io.parse", "scalability_load_io.pdf", -4, "", "Pmem I/O (GB)", 1024.0)
    Plot("scalability_read_io.parse", "scalability_read_io.pdf", -4, "", "Pmem I/O (GB)", 1024.0)
    Plot("scalability_readnon_io.parse", "scalability_readnon_io.pdf", -4, "", "Pmem I/O (GB)", 1024.0)

    # Plot bw
    Plot("scalability_load_bw.parse", "scalability_load_bw.pdf", -4, "", "Pmem Bandwidth (GB/s)", 1024.0)
    Plot("scalability_read_bw.parse", "scalability_read_bw.pdf", -4, "", "Pmem Bandwidth (GB/s)", 1024.0)
    Plot("scalability_readnon_bw.parse", "scalability_readnon_bw.pdf", -4, "", "Pmem Bandwidth (GB/s)", 1024.0)



PlotScalability()


