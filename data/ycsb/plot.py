#/usr/bin/python3
import csv
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import numpy as np
from matplotlib.ticker import (MultipleLocator, FormatStrFormatter,
                               AutoMinorLocator)
# import matplotlib as mpl
# plt.rcParams["font.family"] = "serif"
plt.rcParams['axes.linewidth'] = 2

hashtables = ('turbo', 'cceh', 'dash', 'turbo30', 'cceh30', 'clevel30', 'clht30')
legend_name = ('TURBO16', 'CCEH16', 'DASH16', 'TURBO30', 'CCEH30', 'CLEVEL30', 'CLHT30')

colors= {
    'turbo'   : '#9B0522',     
    'cceh'    : '#83C047',
    'dash'    : '#f7cd6b',
    'turbo30' : '#F37F82',
    'cceh30'  : '#7e72b5', 
    'clevel30': '#3182BD', 
    'clht30'  : '#808084'}


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
            x_value = rect.get_x() + rect.get_width() / 2 + 0.11

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
            label = "{:.2f}".format(labels[j])
            label = label + " Mops/s"
            # if not unit:
            #     label = label + " Mops/s"
            #     unit = True
            # Create annotation
            ax.annotate(                
                label,                      # Use `label` as label
                (x_value, y_value),         # Place label at end of the bar
                xytext=(-8, space),          # Vertically shift label by `space`
                textcoords="offset points", # Interpret `xytext` as offset in points
                ha='center',                # Horizontally center label
                va=va, rotation=90, fontsize=10.5)                      # Vertically align label differently for
                                            # positive and negative values.
            j = j + 1
        i = i + 1

def PlotNormal(df, ax, filename):    
    pick_standard = 1
    normalized = df.copy()
    for kv in hashtables:
        normalized.loc[kv] = normalized.loc[kv] / df.iloc[pick_standard]
    normalized = normalized.T
    print(normalized)
    normalized.plot(ax=ax, kind="bar", rot=0, color='White', width=0.85, edgecolor='k', linewidth=1.7, fontsize=26, alpha=1)
    # plot marker in bar
    bars = ax.patches
    patterns =('//', None , '\\\\', '//', '..', 'xx', None)
    patterns_color = list(colors.values())
    hatches_color = [p for p in patterns_color for i in range(len(normalized))]
    hatches = [p for p in patterns for i in range(len(normalized))]
    i=0
    for bar, hatch, color in zip(bars, hatches, hatches_color):
        # bar.set_alpha(0.1)
        if (i < 18):
            bar.set_alpha(0.8)
            bar.set_color(color)
            bar.set_edgecolor('k')
        else:
            bar.set_edgecolor(color)
        bar.set_hatch(hatch)
        i=i+1

    labels = (df).values.tolist()[pick_standard] 
    print(labels)
    # amplification1 = normalized['cceh'].tolist()
    # amplification2 = normalized['cceh30'].tolist()
    add_value_labels(ax, 7, labels, pick_standard)
    # add_value_labels(ax, 7, amplification1, 1)
    # draw legend
    ax.get_legend().remove()
    ax.legend(legend_name, fontsize=14, loc='upper left', edgecolor='k',facecolor='w', framealpha=0, mode="expand", ncol=4, bbox_to_anchor=(0, 1.24, 1, 0))
    ax.yaxis.grid(linewidth=0.5, dashes=[8,8], color='gray', alpha=0.5)
    ax.set_axisbelow(True)
    ax.set_ylabel('Normalized Throughput', fontsize=22)
    plt.savefig(filename, bbox_inches='tight', pad_inches=0.05, dpi=600)

def PlotNormalYCSB():
    data = pd.DataFrame(columns=['load','ycsba','ycsbb','ycsbc','ycsbd','ycsbf'])

    for i in hashtables:
        filename = "ycsb_" + str(i) + ".parse"
        df = pd.read_csv(filename)
        data = data.append(df.iloc[0])
    
    data.index = hashtables
    data.columns = ['Load', 'A', 'B', 'C', 'D', 'F']
    # Plot positive
    ax = plt.figure(figsize=(9, 3.6)).add_subplot(111)
    df = data
    print(df)
    PlotNormal(df, ax, 'ycsb.pdf')

PlotNormalYCSB()


