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
markers= {
    64:'o', 
    32:'.', 
    16:'|', 
    8: '1', 
    4: '2', 
    2: ''}

def Plot(c_name, ylable, is_legend):
    # Plot Average Log Distance
    fig, ax = plt.subplots(figsize=(4, 3.6))
    for i in (64, 32, 16, 8, 4, 2):
        filename = "probe" + str(i) + ".data.parse"
        df = pd.read_csv(filename, delimiter=' ' ,header=None)
        df=df[2:14]
        df.index = np.arange(1, len(df) + 1)
        df.columns = ["probe_loadfactor", "probe_log_dis"]
        
        df[c_name].plot(
            ax=ax, 
            linewidth=1.5,
            fontsize=12,
            marker=markers[i],
            markersize=10,
            fillstyle='none')

    if (is_legend):
        ax.legend([64, 32, 16, 8, 4, 2], fontsize=11)

    ax.set_ylabel(ylable, fontsize=16, color='k')
    ax.set_xlabel("Number of Rehash", fontsize=16)
    ax.yaxis.grid(linewidth=1, linestyle='--')
    plt.savefig(c_name+".pdf", bbox_inches='tight', pad_inches=0)

Plot("probe_loadfactor", "Load Factor", False)
Plot("probe_log_dis", "Average Log Length", True)