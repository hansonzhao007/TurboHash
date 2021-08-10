#/usr/bin/python3
import csv
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import FuncFormatter
from matplotlib.ticker import (MultipleLocator, FormatStrFormatter,
                               AutoMinorLocator)

plt.rcParams['axes.linewidth'] = 2

# read data
hashtables = ('turbo', 'cceh', 'dash', 'turbo30', 'cceh30', 'clevel30', 'clht30')
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

def Plot(ylable, is_legend):
    # Plot Average Log Distance
    fig, ax = plt.subplots(figsize=(6, 2.2))
    for i in hashtables:
        filename = "all_loadfactor." + str(i) + ".parse"
        df = pd.read_csv(filename ,header=None)
        df.index = np.arange(1, len(df) + 1)
        df.columns = ["loadfactor"]
        
        df.plot(
            ax=ax, 
            linewidth=1.8,
            fontsize=16,
            marker=markers[i],
            dashes=dashes[i],
            color=colors[i],
            markevery=4,
            markersize=7
            # fillstyle='none'
            )

    if (is_legend):
        ax.legend(legend_name, fontsize=13, loc='upper left', edgecolor='k',facecolor='w', framealpha=0, mode="expand", ncol=1, bbox_to_anchor=(1, 1.05, 0.4, 0)
    )

    ax.set_ylabel(ylable, fontsize=16, color='k')
    ax.set_xlabel("Number of records (million)", fontsize=16)
    ax.yaxis.grid(linewidth=0.5, dashes=[8,8], color='gray', alpha=0.5)
    plt.savefig("all_loadfactor.pdf", bbox_inches='tight', pad_inches=0.05)

Plot("Load Factor", True)