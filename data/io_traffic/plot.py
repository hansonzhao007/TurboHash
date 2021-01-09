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
plt.rcParams['axes.linewidth'] = 1.2

hashtables = ('turbo', 'cceh', 'turbo30', 'cceh30', 'clevel30', 'clht30')
legend_name = ('TURBO16', 'CCEH16', 'TURBO30', 'CCEH30', 'CLEVEL30', 'CLHT30')

def PlotIO():
    fig, ax = plt.subplots(figsize=(5, 3.1))

    data = pd.DataFrame(columns=['readlat_r', 'readlat_w', 'readnonlat_r', 'readnonlat_w'])
    data_load = pd.DataFrame(columns=['loadlat_r', 'loadlat_w'])

    for i in hashtables:
        filename = "readio_" + str(i) + ".parse"
        df = pd.read_csv(filename)
        data = data.append(df.iloc[0])
    data.index=legend_name
    data = data / 1024.0
    print(data)

    for i in hashtables:
        filename = "loadio_" + str(i) + ".parse"
        df = pd.read_csv(filename)
        data_load = data_load.append(df.iloc[0])
    data_load.index=legend_name
    data_load = data_load / 1024.0
    print(data_load)


    # Plot bw
    data[['readlat_r', 'readlat_w']].plot.bar(ax=ax, color=("#5E88C2", "red"), edgecolor='k',  stacked=True, width=0.25, position=1, fontsize=16, alpha=0.6)
    data[['readnonlat_r', 'readnonlat_w']].plot.bar(ax=ax, color=("white", "grey"), edgecolor='k',  stacked=True, width=0.25, position=0, fontsize=16, alpha=0.6)
    data_load[['loadlat_r', 'loadlat_w']].plot.bar(ax=ax, color=("white", "#F2AA3F"), edgecolor='k',  stacked=True, width=0.25, position=-1, fontsize=16, alpha=0.6)
    bars = ax.patches
    patterns =(' ', ' ', ' ',' ', 'xxx', '...')
    hatches = [p for p in patterns for i in range(6)]
    for bar, hatch in zip(bars, hatches):
        bar.set_hatch(hatch)


    ax.set_axisbelow(True)
    ax.grid(axis='y', linestyle='-.', linewidth=0.5)    
    ax.set_ylabel("Total IO (GB)", fontsize=16)
    ax.legend(["Positive Read", "Positive Write", "Negative Read", "Negative Write", "Load Read", "Load Write"], loc="upper left", fontsize=9.5, framealpha=1)
    ax.tick_params(axis='y', labelsize=13)
    ax.tick_params(axis='x', labelsize=12, rotation=45)
    ax.set_xlim([-0.5, 5.75])
    fig.savefig('io.pdf', bbox_inches='tight', pad_inches=0.05)

PlotIO()



