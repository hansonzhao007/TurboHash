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
    4: 'x', 
    2: ''}

# Plot load factor 
fig, ax = plt.subplots(figsize=(6, 5))
for i in (64, 32, 16, 8, 4, 2):
    filename = "probe" + str(i) + ".data.parse"
    df = pd.read_csv(filename, delimiter=' ' ,header=None)
    df.index = np.arange(1, len(df) + 1)
    df.columns = ["loadfactor", "probe_dis"]
    
    df[0:14]["loadfactor"].plot(
        ax=ax, 
        linewidth=1.5,
        fontsize=18,
        marker=markers[i],
        markersize=12,
        fillstyle='none')


ax.legend([64, 32, 16, 8, 4, 2], fontsize=11)
ax.set_ylabel('Load Factor', fontsize=22, color='k')
ax.set_xlabel("Number of Rehash", fontsize=22)
ax.yaxis.grid(linewidth=1, linestyle='--')
plt.savefig("probe_loadfactor.pdf", bbox_inches='tight', pad_inches=0)

# Plot Average Log Distance
fig, ax = plt.subplots(figsize=(6, 5))
for i in (64, 32, 16, 8, 4, 2):
    filename = "probe" + str(i) + ".data.parse"
    df = pd.read_csv(filename, delimiter=' ' ,header=None)
    df.index = np.arange(1, len(df) + 1)
    df.columns = ["loadfactor", "probe_dis"]
    
    df[0:14]["probe_dis"].plot(
        ax=ax, 
        linewidth=1.5,
        fontsize=18,
        marker=markers[i],
        markersize=12,
        fillstyle='none')

ax.legend([64, 32, 16, 8, 4, 2], fontsize=11)
ax.set_ylabel('Average Log Length', fontsize=22, color='k')
ax.set_xlabel("Number of Rehash", fontsize=22)
ax.yaxis.grid(linewidth=1, linestyle='--')
plt.savefig("probe_log_dis.pdf", bbox_inches='tight', pad_inches=0)
