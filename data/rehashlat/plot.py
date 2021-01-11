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
plt.rcParams['axes.linewidth'] = 1.8

def PlotLat():
    filename = "rehashlat.parse"
    df = pd.read_csv(filename)
    print(df) 
   
    # Plot 
    fig, ax = plt.subplots(figsize=(4, 3))
    df.plot(ax=ax, x='bucket_count', style=['|--', 'o-', '-.'], alpha=0.8, colormap='Spectral', fontsize=12)

    ax.legend(
        ['Rehash Small Key', 'Rehash Large Key'],
        loc="lower right", 
        fontsize=10, 
        edgecolor='k',
        facecolor='white',
        )
    ax.set_ylabel('Average Rehash Time (us)', fontsize=12, color='k')
    ax.set_xlabel("Number of Buckets", fontsize=16)
    ax.yaxis.grid(linewidth=1, linestyle='--')
    ax.xaxis.grid(linewidth=1, linestyle='--')
    
    fig.savefig('rehash_lat_line.pdf', bbox_inches='tight', pad_inches=0.05)

PlotLat()


