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
df = pd.read_csv("load.csv", delimiter=' ', header=None)
print(df.dtypes)
df.plot()

load_throughput = df.sum(axis = 1, skipna = True) 
print(load_throughput)

fig, ax = plt.subplots(figsize=(8, 6))

def million(x, pos):
    'The two args are the value and tick position'
    return '%1.f' % (x/1000000)
formatter_million = FuncFormatter(million)

load_throughput.plot(
    ax=ax, 
    color='r',
    marker="o",
    markevery=5,
    markersize=12,
    fillstyle='none', 
    fontsize=16,
    alpha=0.8)
ax.yaxis.set_major_formatter(formatter_million)
ax.set_ylabel('Throughput (Mops/s)', fontsize=20, color='k')
ax.set_xlabel("Seconds", fontsize=20)
ax.yaxis.grid(linewidth=1, linestyle='--')
ax.set_title("Insert Performance")
plt.savefig("load.pdf", bbox_inches='tight', pad_inches=0)


# def adjust(x, pos):
#     return '%1.f' % (x*10)
# formatter_adjust = FuncFormatter(adjust)

# for k in dics.keys():
#     print(k)
#     df[k].plot(
#         ax=ax, 
#         color=dics[k][1],
#         marker=dics[k][0],
#         markevery=20,
#         markersize=12,
#         dashes=dics[k][2],
#         fillstyle='none', 
#         fontsize=16,
#         alpha=0.8)
#     ax.legend(
#         dics_legend_name,
#         loc="upper right", 
#         fontsize=12, 
#         edgecolor='k',
#         facecolor='k', 
#         framealpha=0, 
#         mode="expand", 
#         ncol=4,
#         bbox_to_anchor=(-0.0, 1.04, 1.0, .102)
#     )
#     ax.yaxis.set_major_formatter(formatter_million)
#     ax.xaxis.set_major_formatter(formatter_adjust)
#     ax.set_xlabel("Store Size (million of keys)", fontsize=20)
#     ax.set_ylabel('Throughput (Mops/s)', fontsize=20, color='k')
#     ax.yaxis.grid(linewidth=1, linestyle='--')

