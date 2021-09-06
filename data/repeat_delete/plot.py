#/usr/bin/python3
import csv
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import FuncFormatter
from matplotlib.ticker import (MultipleLocator, FormatStrFormatter,
                               AutoMinorLocator)
import matplotlib.ticker as plticker

plt.rcParams['axes.linewidth'] = 2


# read data
for d in range(0,4):
    fig, axs= plt.subplots(2,4,figsize=(10,5), sharey=True, constrained_layout=True)
    for i in range(1, 9):
        filename = "delete_repeat_" + str(d) + "_" + str(i) + "0per.data.parse"
        df = pd.read_csv(filename, header=None)
        df.index = np.arange(1, len(df) + 1)

        average = 0
        total_search = 0
        total_probe = 0
        for row in df.iterrows():
            probe_dis    = row[0]
            search_count = int(row[1])
            print("probe dis", probe_dis, "search count", search_count)
            total_search = total_search + search_count
            total_probe = total_probe + probe_dis * search_count

        avg_probe_dis = total_probe / total_search

        # fig, ax = plt.subplots(figsize=(8, 6))
        r = int((i-1) / 4)
        c = int((i-1) % 4)
        print("row", c, "col", c)
        ax = axs[r, c]
        ax.set_xlim(-0.5, 15)
        df.plot(ax=ax, kind="bar", width=0.975, edgecolor='#F37F82', linewidth=1.7, color='#F37F82')
        ax.get_legend().remove()
        

        probe_str = "%.2f" % avg_probe_dis
        ax.text(0.5, 0.84, "LF: " + str(i/10) + "\nAvg: " + probe_str,
            horizontalalignment='center',
            verticalalignment='center',
            transform = ax.transAxes, size=18)

        
        ax.grid(axis='y', linestyle='-.', linewidth=0.2)
        ax.set_axisbelow(True)
        ax.set_yscale('log')
        
        # for tick in ax.get_xticklabels():
        #     tick.set_rotation(0)
        # loc = plticker.MultipleLocator(base=3.0) # this locator puts ticks at regular intervals
        # ax.xaxis.set_major_locator(loc)

        if i == 1 or i == 5:
            ax.set_ylabel('Frequency', fontsize=22, color='k')
        ax.set_xlim(-0.5, 15.5)

        loc = plticker.MultipleLocator(base=5.0) # this locator puts ticks at regular intervals
        ax.xaxis.set_major_locator(loc)
        
        # ax.set_xlabel("Probe Distance (Number of Buckets)", fontsize=20)
        # ax.set_title("Probe Distance Histogram with Loadfactor " + str(i * 10) + "%")
    plt.subplots_adjust(wspace=0, hspace=0)
    plt.savefig("delete_repeat" + str(d) + ".pdf", bbox_inches='tight', pad_inches=0.05)
