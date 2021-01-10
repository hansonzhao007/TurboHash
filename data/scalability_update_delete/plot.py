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
    'cceh'    : '^', 
    'cceh30'  : 'v', 
    'clevel30': 's', 
    'clht30'  : ''}
colors= {
    'turbo'   : '#E31A1C',
    'turbo30' : '#FB9A99', 
    'cceh'    : '#E7BA52', 
    'cceh30'  : '#FF7F00', 
    'clevel30': '#3182BD', 
    'clht30'  : '#BDBDBD'}
    
def Plot(df, filename, padding, title):
    fig, ax = plt.subplots(figsize=(4, 3.6))
    for i in hashtables:
        df.plot(
            ax=ax, 
            x='thread',
            y=i,
            linewidth=2,
            fontsize=14,
            marker=markers[i],
            markersize=9,
            fillstyle='none',
            color=colors[i])
    
    ax.legend(legend_name, fontsize=9, fancybox=True, framealpha=0.5, edgecolor='k', loc='center right')

    # set y ticks
    ymin, ymax = ax.get_ylim()
    ax.set_ylim([0.1, ymax])
    ax.tick_params(axis="y", direction="inout", pad=padding)
    for label in ax.yaxis.get_ticklabels():
        label.set_horizontalalignment('left')
    ax.set_ylabel("Mops/s", fontsize=16)
    
    # set x ticks
    ax.set_xlim([-4, 42])
    ax.set_xlabel("Number of Threads", fontsize=16)

    ax.set_title(title, fontsize=16)

    # draw v line
    plt.axvline(x=20, color = 'grey', linestyle='--', linewidth=0.5)

    fig.savefig(filename, bbox_inches='tight', pad_inches=0.05)


def PlotScalability():
    data_load = pd.read_csv("scalability_load.parse")
    data_update = pd.read_csv("scalability_update.parse")
    data_delete = pd.read_csv("scalability_delete.parse")
    
    
    data_load.set_index('thread')
    data_update.set_index('thread')
    data_delete.set_index('thread')
    
    print(data_load)

    Plot(data_load, "scalability_load.pdf", -8, "Write")
    Plot(data_update, "scalability_update.pdf", -10, "Update")
    Plot(data_delete, "scalability_delete.pdf", -10, "Delete")
    
    

PlotScalability()


