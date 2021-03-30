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
plt.rcParams['hatch.linewidth'] = 2  # previous pdf hatch linewidth

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

def PlotIO():
    fig, ax = plt.subplots(figsize=(4, 3.6))

    data = pd.DataFrame(columns=['loadlat_r', 'loadlat_w'])

    for i in hashtables:
        filename = "loadio_" + str(i) + ".parse"
        df = pd.read_csv(filename)
        data = data.append(df.iloc[0])

    data.index=legend_name
    data = data / 1024.0
    print(data)

    # Plot bw
    data[['loadlat_r', 'loadlat_w']].plot.bar(ax=ax, color=("#5E88C2", "red"), edgecolor='k',  stacked=True, width=0.25, position=1, fontsize=16, alpha=0.6)
    ax.set_axisbelow(True)
    ax.grid(axis='y', linestyle='-.', linewidth=0.5)    
    ax.set_ylabel("Total IO (GB)", fontsize=16)
    ax.legend(["Read", "Write"], loc="upper left", fontsize=10, framealpha=1)
    ax.tick_params(axis='y', labelsize=16)
    ax.tick_params(axis='x', labelsize=14, rotation=45)
    fig.savefig('load_latency_io.pdf', bbox_inches='tight', pad_inches=0.05)

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
    normalized.plot(ax=ax, kind="bar", rot=0, color="white", width=0.75, edgecolor='k', linewidth=2, fontsize=26, alpha=1)
    # plot marker in bar
    bars = ax.patches
    patterns =('//', ' ', '\\\\', '//', '..', 'xx', ' ')
    patterns_color = list(colors.values())
    hatches_color = [p for p in patterns_color for i in range(len(normalized))]
    hatches = [p for p in patterns for i in range(len(normalized))]
    i=0
    for bar, hatch, color in zip(bars, hatches, hatches_color):
        # bar.set_alpha(0.1)
        if (i <= 8):
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
    ax.legend(legend_name, fontsize=14) #, edgecolor='k',facecolor='w', framealpha=0, mode="expand", ncol=3, bbox_to_anchor=(0, 1.22, 1, 0))
    ax.yaxis.grid(linewidth=1, linestyle='--')
    ax.set_axisbelow(True)
    ax.set_ylabel('Normalized Latency', fontsize=22)
    ax.set_ylim([0.1, 14.9])
    plt.savefig(filename, bbox_inches='tight', pad_inches=0.05, dpi=600)

def PlotNormalLat():
    data_positive = pd.DataFrame(columns=['avg','std','min','median','max','p50','p75','p99','p999','p9999','non'])

    for i in hashtables:
        filename = "loadlat_" + str(i) + "_exist_stat.parse"
        df = pd.read_csv(filename)
        data_positive = data_positive.append(df.iloc[0])
    
    data_positive.index = hashtables

    # Plot positive
    ax = plt.figure(figsize=(7, 5)).add_subplot(111)
    df = data_positive
    df = df.iloc[:,:-2]
    df = df.drop(['min', 'std', 'p50', 'p75', 'max', 'p999', 'p9999'], axis=1)
    print(df)
    PlotNormal(df, ax, 'loadlat_normalized.pdf')


PlotIO()

PlotNormalLat()


