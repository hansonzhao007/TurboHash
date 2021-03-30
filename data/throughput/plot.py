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

    normalized.plot(ax=ax, kind="bar", rot=0, color='White', width=0.85, edgecolor='k', linewidth=2, fontsize=23, alpha=1)
    # plot marker in bar
    bars = ax.patches
    patterns =('//', ' ', '\\\\', '//', '..', 'xx', ' ')
    patterns_color = list(colors.values())
    hatches_color = [p for p in patterns_color for i in range(len(normalized))]
    hatches = [p for p in patterns for i in range(len(normalized))]
    i=0
    for bar, hatch, color in zip(bars, hatches, hatches_color):
        # bar.set_alpha(0.1)
        if (i < 9):
            bar.set_alpha(0.8)
            bar.set_color(color)
            bar.set_edgecolor('k')
        else:
            bar.set_edgecolor(color)
        bar.set_hatch(hatch)
        i=i+1
    
    labels = (df).values.tolist()[pick_standard] 
    # print(labels)
    add_value_labels(ax, 7, labels, pick_standard, units)
    # draw legend
    ax.get_legend().remove()
    # ax.legend(legend_name, fontsize=22) #, edgecolor='k',facecolor='w', framealpha=0, mode="expand", ncol=3, bbox_to_anchor=(0, 1.22, 1, 0))
    ax.yaxis.grid(linewidth=0.5, dashes=[8,8], color='gray', alpha=0.5)
    ax.set_axisbelow(True)
    ax.set_ylabel(ytitle, fontsize=23)
    ax.set_ylim([0.1, 11.9])
    plt.xticks(rotation=rotation)
    


def PlotIO(ax):
    data = pd.DataFrame()

    for i in hashtables:
        filename = "io_" + str(i) + ".parse"
        df = pd.read_csv(filename)
        data = data.append(df.iloc[0])
    data.index=legend_name
    data = data / 1024.0

    # Plot io normalized
    data['Write'] = data['load_r'] + data['load_w']
    data['Positive'] = data['read_r'] + data['read_w']
    data['Negative'] = data['readnon_r'] + data['readnon_w']
    print(data)

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
    normalizedT.plot(ax=ax, kind="bar", rot=0, color='White', width=0.85, edgecolor='k', linewidth=2, fontsize=23)
    # plot marker in bar
    bars = ax.patches
    patterns =('//', ' ', '\\\\', '//', '..', 'xx', ' ')
    patterns_color = list(colors.values())
    hatches_color = [p for p in patterns_color for i in range(len(normalizedT))]
    hatches = [p for p in patterns for i in range(len(normalizedT))]
    i=0
    for bar, hatch, color in zip(bars, hatches, hatches_color):
        # bar.set_alpha(0.1)
        if (i < 9):
            bar.set_alpha(0.8)
            bar.set_color(color)
            bar.set_edgecolor('k')
        else:
            bar.set_edgecolor(color)
        bar.set_hatch(hatch)
        i=i+1
    
    ax2=ax.twinx()
    df_less = df[['load_w', 'read_w', 'readnon_w']]
    normalized = df_normalized[['load_w', 'read_w', 'readnon_w']]
    normalizedT = normalized.T
    normalizedT.plot(ax=ax2, kind="bar", rot=0, color='#D9DADC', width=0.85, edgecolor='k', linewidth=2, fontsize=23, alpha=1)
    # plot marker in bar
    bars = ax2.patches
    patterns =('//', ' ', '\\\\', '//', '..', 'xx', ' ')
    patterns_color = list(colors.values())
    hatches_color = [p for p in patterns_color for i in range(len(normalizedT))]
    hatches = [p for p in patterns for i in range(len(normalizedT))]
    for bar, hatch, color in zip(bars, hatches, hatches_color):  
        bar.set_alpha(1)
        bar.set_hatch('-|-|')      
        bar.set_color('#FFFFFF')
        bar.set_edgecolor('k')
        
    ymin,ymax = ax.get_ylim()
    ax2.set_ylim([ymin, ymax])
    ax2.set_yticklabels([])
    ax2.get_legend().remove()
    ax2.legend(['Write I/O'], fontsize=20, loc='upper center')

    ax.get_legend().remove()
    ax.legend(legend_name, fontsize=22, edgecolor='k',facecolor='w', framealpha=0, mode="expand", ncol=4, bbox_to_anchor=(-1.2, 0.98, 2.2, 0))

    ax.yaxis.grid(linewidth=0.5, dashes=[8,8], color='gray', alpha=0.5)
    ax.set_axisbelow(True)
    ax.set_ylabel('Pmem I/O (GB)', fontsize=23)
    ax.set_xticklabels(['Insert', 'Positive\nSearch', 'Negative\nSearch'])
    ax.tick_params(axis='y', labelsize=19)
    plt.xticks(rotation=0)
    
    return data

def PlotNormalThroughput():
    data = pd.DataFrame(columns=['load','read','readnon'])

    for i in hashtables:
        filename = "throughput_" + str(i) + ".parse"
        df = pd.read_csv(filename)
        data = data.append(df.iloc[0])
    
    data.index = hashtables
    data.columns = ['Insert', 'Positive\nSearch', 'Negative\nSearch']
    # Plot 
    fig, (ax1, ax2) = plt.subplots(1,2,figsize=(15,6))
    df = data
    PlotNormal(df, ax1, 'throughput_normalized.pdf')
    PlotIO(ax2)
    fig.savefig("throughput_normalized.pdf", bbox_inches='tight', pad_inches=0.05, dpi=600)


PlotNormalThroughput()


