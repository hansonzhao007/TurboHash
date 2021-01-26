#/usr/bin/python3
import csv
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import FuncFormatter
from matplotlib.ticker import (MultipleLocator, FormatStrFormatter,
                               AutoMinorLocator)

plt.rcParams["font.family"] = "serif"

# read data
for i in range(1, 10):
    filename = "probe_dis_" + str(i) + "0per.data.parse"
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

    fig, ax = plt.subplots(figsize=(8, 6))
    df.plot(ax=ax, kind="bar", width=0.8)
    ax.get_legend().remove()

    probe_str = "%.5f" % avg_probe_dis
    ax.text(0.82, 0.95, "Avg Probe Distance: " + probe_str,
        horizontalalignment='center',
        verticalalignment='center',
        transform = ax.transAxes)

    ax.set_xlim(-0.5, 15)
    ax.set_yscale('log')
    ax.set_ylabel('Frequency', fontsize=20, color='k')
    ax.set_xlabel("Probe Distance (Number of Buckets)", fontsize=20)
    ax.set_title("Probe Distance Histogram with Loadfactor " + str(i * 10) + "%")
    plt.savefig(filename + ".pdf", bbox_inches='tight', pad_inches=0)
