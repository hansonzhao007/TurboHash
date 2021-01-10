#/usr/bin/python3
import csv
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import FuncFormatter
from matplotlib.ticker import (MultipleLocator, FormatStrFormatter,
                               AutoMinorLocator)

plt.rcParams["font.family"] = "serif"
plt.rcParams['axes.linewidth'] = 1.2

# read data
hashtables = ('turbo', 'turbo30', 'cceh', 'cceh30', 'clevel30', 'clht30')
legend_name = ('TURBO16', 'TURBO30', 'CCEH16', 'CCEH30', 'CLEVEL30', 'CLHT30')
markers= {
    'turbo'   : 'o',
    'turbo30' : '.', 
    'cceh'    : '^', 
    'cceh30'  : 'v', 
    'clevel30': 's', 
    'clht30'  : ''}
colors= {
    'turbo'   : '#B82126',
    'turbo30' : '#F37F82', 
    'cceh'    : '#0E5932', 
    'cceh30'  : '#AFD34E', 
    'clevel30': '#3182BD', 
    'clht30'  : '#BDBDBD'}

def Plot(ylable, is_legend):
    # Plot Average Log Distance
    fig, ax = plt.subplots(figsize=(6, 3.6))
    for i in hashtables:
        filename = "all_loadfactor." + str(i) + ".parse"
        df = pd.read_csv(filename ,header=None)
        df.index = np.arange(1, len(df) + 1)
        df.columns = ["loadfactor"]
        
        df.plot(
            ax=ax, 
            linewidth=2,
            fontsize=12,
            marker=markers[i],
            markevery=4,
            markersize=8,
            fillstyle='none',
            color=colors[i])

    if (is_legend):
        ax.legend(
        legend_name,
        loc="upper right", 
        fontsize=12, 
        edgecolor='k',
        facecolor='white', 
        mode="expand", 
        ncol=3,
        bbox_to_anchor=(0.10, .15, 0.90, .102)
    )

    ax.set_ylabel(ylable, fontsize=16, color='k')
    ax.set_xlabel("Insertion (million)", fontsize=16)
    ax.yaxis.grid(linewidth=1, linestyle='--')
    plt.savefig("all_loadfactor.pdf", bbox_inches='tight', pad_inches=0.05)

Plot("Load Factor", True)