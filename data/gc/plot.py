#/usr/bin/python3
import csv
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import numpy as np
from matplotlib.ticker import (MultipleLocator, FormatStrFormatter,
                               AutoMinorLocator)
# import matplotlib as mpl
plt.rcParams['axes.linewidth'] = 2
plt.rcParams['hatch.linewidth'] = 2  # previous pdf hatch linewidth

hashtables = ('load', 'deleteoverwrite', 'gc')
legend_name = ('Load', 'Overwrite', 'GC')


def PlotLat():
    
    for i in hashtables:
        fig, axes = plt.subplots(ncols=2, sharey=True, figsize=(4, 4))

        file_readlat = "readlat_" + str(i) + "_exist.parse"
        file_readnonlat = "readlat_" + str(i) + "_non.parse"

        file_readlat_data = "readlat_" + str(i) + "_exist_stat.parse"
        file_readnonlat_data = "readlat_" + str(i) + "_non_stat.parse"

        # Plot positive hist
        ax=axes[0]
        df = pd.read_csv(file_readlat)
        df['us'] = df['ns'] / 1000.0
        df.set_index('us')
        df.plot(ax=ax, x='frequency', y='us')
        ax.set_xscale('log')
        ax.set_yscale('log')
        ax.set_axisbelow(True)
        ax.set_xlabel("Positive Search", fontsize=12)
        ax.get_legend().remove()
        ax.fill_betweenx(df.us, df['frequency'], 1, alpha=0.5, color="#427996")
        ax.set_xticks([])
        ax.set_ylim([0.03, 1000 - 1])
        # text avg, std, p99
        mysize=12
        df_data = pd.read_csv(file_readlat_data)
        ax.text(0.43, 0.95, " avg: " + '%.2f' % (df_data.iloc[0]['avg'] / 1000.0),
            horizontalalignment='center',
            verticalalignment='center',
            transform = ax.transAxes, fontsize=mysize)
        ax.text(0.43, 0.89, " mid: " + '%.2f' % (df_data.iloc[0]['median'] / 1000.0),
            horizontalalignment='center',
            verticalalignment='center',
            transform = ax.transAxes, fontsize=mysize)
        ax.text(0.43, 0.83, " p99: " + '%.2f' % (df_data.iloc[0]['p99'] / 1000.0),
            horizontalalignment='center',
            verticalalignment='center',
            transform = ax.transAxes, fontsize=mysize)       
        # xmin, xmax = ax.get_ylim()
        # ax.axhline(y=df_data.iloc[0]['avg'] / 1000.0, xmin=xmin, xmax=xmax, color='#B31512')

        # Plot negetive hist
        ax2=axes[1]
        df2 = pd.read_csv(file_readnonlat)
        df2['us'] = df2['ns'] / 1000.0
        df2.set_index('us')
        df2.plot(ax=ax2, x='frequency', y='us')
        ax2.set_xscale('log')
        # ax2.set_yscale('log')
        ax2.set_axisbelow(True)
        ax2.set_xlabel("Negative Search", fontsize=12)
        ax2.get_legend().remove()
        ax2.fill_betweenx(df2.us, df2['frequency'], 1, alpha=0.5, color="#427996")
        ax2.set_xticks([])
        df2_data = pd.read_csv(file_readnonlat_data)
        ax2.text(0.57, 0.95, " avg: " + '%.2f' % (df2_data.iloc[0]['avg'] / 1000.0),
            horizontalalignment='center',
            verticalalignment='center',
            transform = ax2.transAxes, fontsize=mysize)
        ax2.text(0.57, 0.89, " mid: " + '%.2f' % (df2_data.iloc[0]['median'] / 1000.0),
            horizontalalignment='center',
            verticalalignment='center',
            transform = ax2.transAxes, fontsize=mysize)
        ax2.text(0.57, 0.83, " p99: " + '%.2f' % (df2_data.iloc[0]['p99'] / 1000.0),
            horizontalalignment='center',
            verticalalignment='center',
            transform = ax2.transAxes, fontsize=mysize)
        # xmin, xmax = ax2.get_ylim()
        # ax2.axhline(y=df2_data.iloc[0]['avg'] / 1000.0, xmin=xmin, xmax=xmax, color='#B31512')

        axes[0].invert_xaxis()
        axes[0].yaxis.tick_right()
        fig.subplots_adjust(wspace=0.32)
        plt.text(1.17, 0.98,'us',
            horizontalalignment='center',
            verticalalignment='center',
            transform = ax.transAxes, fontsize=16)
        
        fig.savefig('read_latency_' + i + '.pdf', bbox_inches='tight', pad_inches=0.05)



PlotLat()



