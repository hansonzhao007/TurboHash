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

    data = pd.DataFrame()

    for i in hashtables:
        filename = "io_" + str(i) + ".parse"
        df = pd.read_csv(filename)
        data = data.append(df.iloc[0])
    data.index=legend_name
    data = data / 1024.0
    print(data)

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
    ax.set_ylabel("Total IO (GB)", fontsize=16)
    ax.legend(["Positive Read", "Positive Write", "Negative Read", "Negative Write", "Load Read", "Load Write"], loc="upper left", fontsize=9.5, framealpha=1)
    ax.tick_params(axis='y', labelsize=13)
    ax.tick_params(axis='x', labelsize=12, rotation=45)
    ax.set_xlim([-0.5, 5.75])
    fig.savefig('io.pdf', bbox_inches='tight', pad_inches=0.05)



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
            if not unit:
                label = label + " Mops/s"
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
    pick_standard = 2
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
    ax.set_ylabel('Normalized Throughput', fontsize=22)
    # ax.set_ylim([0.1, 11.9])
    plt.savefig(filename, bbox_inches='tight', pad_inches=0.05)

def PlotNormalThroughput():
    data = pd.DataFrame(columns=['load','read','readnon'])

    for i in hashtables:
        filename = "throughput_" + str(i) + ".parse"
        df = pd.read_csv(filename)
        data = data.append(df.iloc[0])
    
    data.index = hashtables

    # Plot 
    ax = plt.figure(figsize=(5, 3.1)).add_subplot(111)
    df = data
    print(df)
    PlotNormal(df, ax, 'throughput_normalized.pdf')


PlotIO()
PlotNormalThroughput()


