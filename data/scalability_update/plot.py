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
    file1 = filename + ".parse"
    df = pd.read_csv(file1)
    print(df)
    
    df['thread'] = [1,2,4,8,16,20,24,28,32,36,40]
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
            dashes=dashes[i],
            markersize=8,
            fillstyle='none',
            color=colors[i])
    
    ax.legend(legend_name, fontsize=9, edgecolor='k',facecolor='w', framealpha=0, mode="expand", ncol=3, bbox_to_anchor=(-0.01, 0.765, 1.03, 0.1))

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

    # ax.set_title(title, fontsize=16)

    # draw v line
    # plt.axvline(x=20, color = 'grey', linestyle='--', linewidth=0.5)
    # ax.yaxis.grid(linewidth=1, linestyle='--')

    fig.savefig(outfile, bbox_inches='tight', pad_inches=0.05)


def PlotScalability():
    # Plot throughput
    Plot("scalability_update", "scalability_update.pdf", -10, "Update Throughput", "Throughput (Mops/s)")
    
    # Plot IO
    Plot("scalability_update_io", "scalability_update_io.pdf", -4, "", "Pmem I/O (GB)", 1024.0)
    
    # Plot bw
    Plot("scalability_update_bw", "scalability_update_bw.pdf", -4, "", "Pmem Bandwidth (GB/s)", 1024.0)

PlotScalability()


