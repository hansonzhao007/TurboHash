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

hashtables = ['turbo', 'cceh', 'turbo30', 'cceh30', 'clevel30', 'clht30']
legend_name = ('TURBO16', 'CCEH16', 'TURBO30', 'CCEH30', 'CLEVEL30', 'CLHT30')


def Plot(df, filename):
    fig, ax = plt.subplots(figsize=(4, 3.6))
    df.plot(ax=ax, x='thread', y=hashtables)
    ax.legend(legend_name, fontsize=10)
    fig.savefig(filename, bbox_inches='tight', pad_inches=0.05)


def PlotScalability():
    data_load = pd.read_csv("scalability_load.parse")
    data_read = pd.read_csv("scalability_read.parse")
    data_readnon = pd.read_csv("scalability_readnon.parse")
    
    data_load.set_index('thread')
    data_read.set_index('thread')
    data_readnon.set_index('thread')
    
    # data_load.drop(['thread'], axis=1, inplace=True)
    # data_read.drop(['thread'], axis=1, inplace=True)
    # data_readnon.drop(['thread'], axis=1, inplace=True)
    
    print(data_load)

    Plot(data_load, "scalability_load.pdf")
    Plot(data_read, "scalability_read.pdf")
    Plot(data_readnon, "scalability_readnon.pdf")
    
    

PlotScalability()


