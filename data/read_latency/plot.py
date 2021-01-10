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
    fig, ax = plt.subplots(figsize=(4, 3.6))

    data = pd.DataFrame(columns=['readlat_r', 'readlat_w', 'readnonlat_r', 'readnonlat_w'])

    for i in hashtables:
        filename = "readio_" + str(i) + ".parse"
        df = pd.read_csv(filename)
        data = data.append(df.iloc[0])

    data.index=legend_name
    data = data / 1024.0
    print(data)

    # Plot bw
    data[['readlat_r', 'readlat_w']].plot.bar(ax=ax, color=("#5E88C2", "red"), edgecolor='k',  stacked=True, width=0.25, position=1, fontsize=16, alpha=0.6)
    data[['readnonlat_r', 'readnonlat_w']].plot.bar(ax=ax, color=("white", "grey"), edgecolor='k',  stacked=True, width=0.25, position=0, fontsize=16, alpha=0.6)
    ax.set_axisbelow(True)
    ax.grid(axis='y', linestyle='-.', linewidth=0.5)    
    ax.set_ylabel("Total IO (GB)", fontsize=16)
    ax.legend(["Positive Read", "Positive Write", "Negative Read", "Negative Write"], loc="upper left", fontsize=10, framealpha=1)
    ax.tick_params(axis='y', labelsize=16)
    ax.tick_params(axis='x', labelsize=14, rotation=45)
    fig.savefig('read_latency_io.pdf', bbox_inches='tight', pad_inches=0.05)

def PlotLat():
    
    for i in hashtables:
        fig, axes = plt.subplots(ncols=2, sharey=True, figsize=(4, 6))

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
        ax.set_xlabel("Positive", fontsize=16)
        ax.get_legend().remove()
        ax.fill_betweenx(df.us, df['frequency'], 1, alpha=0.5, color="#5E88C2")
        ax.set_xticks([])
        ax.set_ylim([0.03, 1000 - 1])
        # text avg, std, p99
        mysize=12
        df_data = pd.read_csv(file_readlat_data)
        ax.text(0.43, 0.97, " avg: " + '%.2f' % (df_data.iloc[0]['avg'] / 1000.0),
            horizontalalignment='center',
            verticalalignment='center',
            transform = ax.transAxes, fontsize=mysize)
        ax.text(0.43, 0.92, " mid: " + '%.2f' % (df_data.iloc[0]['median'] / 1000.0),
            horizontalalignment='center',
            verticalalignment='center',
            transform = ax.transAxes, fontsize=mysize)
        ax.text(0.43, 0.87, " p99: " + '%.2f' % (df_data.iloc[0]['p99'] / 1000.0),
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
        ax2.set_xlabel("Negative", fontsize=16)
        ax2.get_legend().remove()
        ax2.fill_betweenx(df2.us, df2['frequency'], 1, alpha=0.5, color="#5E88C2")
        ax2.set_xticks([])
        df2_data = pd.read_csv(file_readnonlat_data)
        ax2.text(0.57, 0.97, " avg: " + '%.2f' % (df2_data.iloc[0]['avg'] / 1000.0),
            horizontalalignment='center',
            verticalalignment='center',
            transform = ax2.transAxes, fontsize=mysize)
        ax2.text(0.57, 0.92, " mid: " + '%.2f' % (df2_data.iloc[0]['median'] / 1000.0),
            horizontalalignment='center',
            verticalalignment='center',
            transform = ax2.transAxes, fontsize=mysize)
        ax2.text(0.57, 0.87, " p99: " + '%.2f' % (df2_data.iloc[0]['p99'] / 1000.0),
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


# plot value on top of standard bar
def add_value_labels(ax, spacing, labels, pick_standard):
    """Add labels to the end of each bar in a bar chart.
    Arguments:
        ax (matplotlib.axes.Axes): The matplotlib object containing the axes
            of the plot to annotate.
        spacing (int): The distance between the labels and the bars.
    """
    i = 0
    j = 0
    interval = len(labels)
    # For each bar: Place a label
    unit = False
    for rect in ax.patches:
        if i >= pick_standard * interval and i < (pick_standard+1) * interval:
            # Get X and Y placement of label from rect.
            y_value = rect.get_height() + 0.05
            x_value = rect.get_x() + rect.get_width() / 2 + 0.05

            # Number of points between bar and label. Change to your liking.
            space = spacing
            # Vertical alignment for positive values
            va = 'bottom'

            # If value of bar is negative: Place label below bar
            if y_value < 0:
                # Invert space to place label below
                space *= -1
                # Vertically align label at top
                va = 'top'

            # Use Y value as label and format number with one decimal place
            label = "{:.1f}".format(labels[j])
            if not unit:
                label = label + " ns"
                unit = True
            # Create annotation
            ax.annotate(
                label,                      # Use `label` as label
                (x_value, y_value),         # Place label at end of the bar
                xytext=(-7, space),          # Vertically shift label by `space`
                textcoords="offset points", # Interpret `xytext` as offset in points
                ha='center',                # Horizontally center label
                va=va, rotation=90, fontsize=20)                      # Vertically align label differently for
                                            # positive and negative values.
            j = j + 1
        i = i + 1

def PlotNormal(df, ax, filename):    
    pick_standard = 0
    normalized = df.copy()
    for kv in hashtables:
        normalized.loc[kv] = normalized.loc[kv] / df.iloc[pick_standard]
    normalized = normalized.T
    print(normalized)
    normalized.plot(ax=ax, kind="bar", rot=0, colormap='Spectral', width=0.75, edgecolor='k', linewidth=1.7, fontsize=26, alpha=0.8)
    # plot marker in bar
    bars = ax.patches
    patterns =('///', ' ', '///', ' ', '..', 'xx')
    hatches = [p for p in patterns for i in range(len(normalized))]
    for bar, hatch in zip(bars, hatches):
        bar.set_hatch(hatch)
    
    labels = (df).values.tolist()[pick_standard] 
    print(labels)
    # amplification1 = normalized['cceh'].tolist()
    # amplification2 = normalized['cceh30'].tolist()
    add_value_labels(ax, 7, labels, pick_standard)
    # add_value_labels(ax, 7, amplification1, 1)
    # draw legend
    ax.get_legend().remove()
    ax.legend(legend_name, fontsize=14) #, edgecolor='k',facecolor='w', framealpha=0, mode="expand", ncol=3, bbox_to_anchor=(0, 1.22, 1, 0))
    ax.yaxis.grid(linewidth=1, linestyle='--')
    ax.set_axisbelow(True)
    ax.set_ylabel('Normalized Latency', fontsize=22)
    ax.set_ylim([0.1, 12.9])
    plt.savefig(filename, bbox_inches='tight', pad_inches=0.05)

def PlotNormalLat():
    data_positive = pd.DataFrame(columns=['avg','std','min','median','max','p50','p75','p99','p999','p9999','non'])
    data_negetive = pd.DataFrame(columns=['avg','std','min','median','max','p50','p75','p99','p999','p9999','non'])

    for i in hashtables:
        filename = "readlat_" + str(i) + "_exist_stat.parse"
        df = pd.read_csv(filename)
        data_positive = data_positive.append(df.iloc[0])
    
    for i in hashtables:
        filename = "readlat_" + str(i) + "_non_stat.parse"
        df = pd.read_csv(filename)
        data_negetive = data_negetive.append(df.iloc[0])

    data_positive.index = hashtables
    data_negetive.index = hashtables

    # Plot positive
    ax = plt.figure(figsize=(7, 5)).add_subplot(111)
    df = data_positive
    df = df.iloc[:,:-2]
    df = df.drop(['min', 'std', 'p50', 'p75', 'max', 'p999', 'p9999'], axis=1)
    print(df)
    PlotNormal(df, ax, 'readlat_normalized.pdf')

    # Plot Negetive
    ax = plt.figure(figsize=(7, 5)).add_subplot(111)
    df = data_negetive
    df = df.iloc[:,:-2]
    df = df.drop(['min', 'std', 'p50', 'p75', 'max', 'p999', 'p9999'], axis=1)
    print(df)
    PlotNormal(df, ax, 'readnonlat_normalized.pdf')

PlotIO()

PlotLat()

PlotNormalLat()


