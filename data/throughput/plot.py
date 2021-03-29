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

hashtables = ['turbo', 'cceh', 'turbo30', 'cceh30', 'clevel30', 'clht30']
legend_name = ('TURBO16', 'CCEH16', 'TURBO30', 'CCEH30', 'CLEVEL30', 'CLHT30')

# plot value on top of standard bar
def add_value_labels(ax, spacing, labels, pick_standard, units="Mops/s"):
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
            x_value = rect.get_x() + rect.get_width() / 2 + 0.07

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
            label = label + units
            # if not unit:
            #     label = label + " Mops/s"
            #     unit = True
            # Create annotation
            ax.annotate(
                label,                      # Use `label` as label
                (x_value, y_value),         # Place label at end of the bar
                xytext=(-7, space),          # Vertically shift label by `space`
                textcoords="offset points", # Interpret `xytext` as offset in points
                ha='center',                # Horizontally center label
                va=va, rotation=90, fontsize=18)                      # Vertically align label differently for
                                            # positive and negative values.
            j = j + 1
        i = i + 1

def PlotNormal(df, ax, filename, rotation = 0, pick = 1, units="Mops/s", ytitle='Normalized Throughput', is_while=False):    
    pick_standard = pick
    normalized = df.copy()
    for kv in hashtables:
        normalized.loc[kv] = normalized.loc[kv] / df.iloc[pick_standard]
    normalized = normalized.T

    normalized.plot(ax=ax, kind="bar", rot=0, colormap='Spectral', width=0.75, edgecolor='k', linewidth=1.7, fontsize=26, alpha=0.8)
    # plot marker in bar
    bars = ax.patches
    patterns =('//', ' ', '\\\\', ' ', '..', 'xx')
    hatches = [p for p in patterns for i in range(len(normalized))]
    for bar, hatch in zip(bars, hatches):
        bar.set_hatch(hatch)
    
    labels = (df).values.tolist()[pick_standard] 
    # print(labels)
    add_value_labels(ax, 7, labels, pick_standard, units)
    # draw legend
    ax.get_legend().remove()
    ax.legend(legend_name, fontsize=22) #, edgecolor='k',facecolor='w', framealpha=0, mode="expand", ncol=3, bbox_to_anchor=(0, 1.22, 1, 0))
    ax.yaxis.grid(linewidth=1, linestyle='--')
    ax.set_axisbelow(True)
    ax.set_ylabel(ytitle, fontsize=26)
    # ax.set_ylim([0.1, 11.9])
    plt.xticks(rotation=rotation)
    plt.savefig(filename, bbox_inches='tight', pad_inches=0.05, dpi=600)


def PlotIO():
    fig, ax = plt.subplots(figsize=(7, 7))

    data = pd.DataFrame()

    for i in hashtables:
        filename = "io_" + str(i) + ".parse"
        df = pd.read_csv(filename)
        data = data.append(df.iloc[0])
    data.index=legend_name
    data = data / 1024.0
    # print(data)

    # Plot bw
    data[['read_r', 'read_w']].plot.bar(ax=ax, color=("#5E88C2", "red"), edgecolor='k',  stacked=True, width=0.25, position=1, fontsize=16, alpha=0.6)
    data[['readnon_r', 'readnon_w']].plot.bar(ax=ax, color=("white", "grey"), edgecolor='k',  stacked=True, width=0.25, position=0, fontsize=16, alpha=0.6)
    data[['load_r', 'load_w']].plot.bar(ax=ax, color=("white", "#F2AA3F"), edgecolor='k',  stacked=True, width=0.25, position=-1, fontsize=16, alpha=0.6)
    bars = ax.patches
    patterns =(' ', ' ', ' ',' ', 'xxx', '...')
    hatches = [p for p in patterns for i in range(6)]
    for bar, hatch in zip(bars, hatches):
        bar.set_hatch(hatch)

    ax.set_axisbelow(True)
    ax.grid(axis='y', linestyle='-.', linewidth=0.5)    
    ax.set_ylabel("Total IO (GB)", fontsize=22)
    ax.legend(["Positive Read - R", "Positive Read - W", "Negative Read - R", "Negative Read - W", "Load - R", "Load - W"], loc="upper left", fontsize=16, framealpha=1)
    ax.tick_params(axis='y', labelsize=18)
    ax.tick_params(axis='x', labelsize=20, rotation=30)
    ax.set_xlim([-0.5, 5.75])
    fig.savefig('io.pdf', bbox_inches='tight', pad_inches=0.05) 
    data.index = hashtables
    
    # Plot io normalized
    data['Write'] = data['load_r'] + data['load_w']
    data['Positive'] = data['read_r'] + data['read_w']
    data['Negative'] = data['readnon_r'] + data['readnon_w']
    print(data)

    
    ax = plt.figure(figsize=(7, 9)).add_subplot(111)
    df = data
    # df = data
    pick_standard = 1
    df_normalized = df.copy()
    # for kv in hashtables:
    #     df_normalized.loc[kv] = df_normalized.loc[kv] / df.iloc[pick_standard]
    print(df_normalized)
    df_less = df[['Write', 'Positive', 'Negative']]
    normalized = df_normalized[['Write', 'Positive', 'Negative']]
    normalizedT = normalized.T
    normalizedT.plot(ax=ax, kind="bar", rot=0, colormap='Spectral', width=0.75, edgecolor='k', linewidth=1.7, fontsize=26, alpha=0.8)
    # plot marker in bar
    bars = ax.patches
    patterns =('//', ' ', '\\\\', ' ', '..', 'xx')
    hatches = [p for p in patterns for i in range(len(normalizedT))]
    for bar, hatch in zip(bars, hatches):
        bar.set_hatch(hatch)
    

    df_less = df[['load_w', 'read_w', 'readnon_w']]
    normalized = df_normalized[['load_w', 'read_w', 'readnon_w']]
    normalizedT = normalized.T
    normalizedT.plot(ax=ax, kind="bar", rot=0, color='#D9DADC', width=0.75, edgecolor='k', linewidth=1.7, fontsize=26, alpha=1)

    ax.get_legend().remove()
    ax.legend(legend_name, fontsize=22)
    ax.yaxis.grid(linewidth=1, linestyle='--')
    ax.set_axisbelow(True)
    ax.set_ylabel('Pmem I/O (GB)', fontsize=26)
    ax.set_xticklabels(['Write', 'Positive\nRead', 'Negative\nRead'])
    plt.xticks(rotation=0)
    plt.savefig('io_normalized.pdf', bbox_inches='tight', pad_inches=0.05, dpi=600)

    return data

def PlotNormalThroughput():
    data = pd.DataFrame(columns=['load','read','readnon'])

    for i in hashtables:
        filename = "throughput_" + str(i) + ".parse"
        df = pd.read_csv(filename)
        data = data.append(df.iloc[0])
    
    data.index = hashtables
    data.columns = ['Write', 'Positive\nRead', 'Negative\nRead']
    # Plot 
    ax = plt.figure(figsize=(7, 9)).add_subplot(111)
    df = data
    PlotNormal(df, ax, 'throughput_normalized.pdf')

PlotIO()

PlotNormalThroughput()


