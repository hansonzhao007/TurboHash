#/usr/bin/python3
import csv
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import matplotlib
from matplotlib.ticker import FuncFormatter
from matplotlib.ticker import (MultipleLocator, FormatStrFormatter,
                               AutoMinorLocator)

plt.rcParams["font.family"] = "serif"


data = pd.DataFrame()
data.index = np.arange(1, 17)
# read data
for i in range(1, 10):
    filename = "probe_dis_" + str(i) + "0per.data.parse"
    df = pd.read_csv(filename, header=None)
    df.index = np.arange(1, len(df) + 1)
    column_name = str(i*10)
    df.columns = [column_name]
    data[column_name] = df[column_name]

    average = 0
    total_search = 0
    total_probe = 0
    for row in df.iterrows():
        probe_dis    = row[0]
        search_count = int(row[1])
        print("probe dis", probe_dis, "search count", search_count)
        total_search = total_search + search_count
        total_probe = total_probe + probe_dis * search_count

    avg_probe_dis = total_probe / total_search

    # fig, ax = plt.subplots(figsize=(8, 6))
    # df.plot(ax=ax, kind="bar", width=0.8)
    # ax.get_legend().remove()
    
    # probe_str = "%.5f" % avg_probe_dis
    # ax.text(0.82, 0.95, "Avg Probe Distance: " + probe_str,
    #     horizontalalignment='center',
    #     verticalalignment='center',
    #     transform = ax.transAxes)

    # ax.set_xlim(-0.5, 15)
    # ax.set_yscale('log')
    # ax.set_ylabel('Frequency', fontsize=20, color='k')
    # ax.set_xlabel("Probe Distance (Number of Buckets)", fontsize=20)
    # ax.set_title("Probe Distance Histogram with Loadfactor " + str(i * 10) + "%")
    # plt.savefig(filename + ".pdf", bbox_inches='tight', pad_inches=0)

print(data.to_numpy())
def heatmap(data, row_labels, col_labels, ax=None,
            cbar_kw={}, cbarlabel="", **kwargs):
    """
    Create a heatmap from a numpy array and two lists of labels.

    Parameters
    ----------
    data
        A 2D numpy array of shape (N, M).
    row_labels
        A list or array of length N with the labels for the rows.
    col_labels
        A list or array of length M with the labels for the columns.
    ax
        A `matplotlib.axes.Axes` instance to which the heatmap is plotted.  If
        not provided, use current axes or create a new one.  Optional.
    cbar_kw
        A dictionary with arguments to `matplotlib.Figure.colorbar`.  Optional.
    cbarlabel
        The label for the colorbar.  Optional.
    **kwargs
        All other arguments are forwarded to `imshow`.
    """

    if not ax:
        ax = plt.gca()

    # Plot the heatmap
    im = ax.imshow(data, **kwargs)

    # Create colorbar
    cbar = ax.figure.colorbar(im, ax=ax, **cbar_kw)
    cbar.ax.set_ylabel(cbarlabel, rotation=-90, va="bottom")

    # We want to show all ticks...
    ax.set_xticks(np.arange(data.shape[1]))
    ax.set_yticks(np.arange(data.shape[0]))
    # ... and label them with the respective list entries.
    ax.set_xticklabels(col_labels)
    ax.set_yticklabels(row_labels)

    # Let the horizontal axes labeling appear on top.
    ax.tick_params(top=True, bottom=False,
                   labeltop=True, labelbottom=False)

    # Rotate the tick labels and set their alignment.
    plt.setp(ax.get_xticklabels(), rotation=-30, ha="right",
             rotation_mode="anchor")

    # Turn spines off and create white grid.
    ax.spines[:].set_visible(False)

    ax.set_xticks(np.arange(data.shape[1]+1)-.5, minor=True)
    ax.set_yticks(np.arange(data.shape[0]+1)-.5, minor=True)
    ax.grid(which="minor", color="w", linestyle='-', linewidth=3)
    ax.tick_params(which="minor", bottom=False, left=False)

    return im, cbar


def annotate_heatmap(im, data=None, valfmt="{x:.2f}",
                     textcolors=("black", "white"),
                     threshold=None, **textkw):
    """
    A function to annotate a heatmap.

    Parameters
    ----------
    im
        The AxesImage to be labeled.
    data
        Data used to annotate.  If None, the image's data is used.  Optional.
    valfmt
        The format of the annotations inside the heatmap.  This should either
        use the string format method, e.g. "$ {x:.2f}", or be a
        `matplotlib.ticker.Formatter`.  Optional.
    textcolors
        A pair of colors.  The first is used for values below a threshold,
        the second for those above.  Optional.
    threshold
        Value in data units according to which the colors from textcolors are
        applied.  If None (the default) uses the middle of the colormap as
        separation.  Optional.
    **kwargs
        All other arguments are forwarded to each call to `text` used to create
        the text labels.
    """

    if not isinstance(data, (list, np.ndarray)):
        data = im.get_array()

    # Normalize the threshold to the images color range.
    if threshold is not None:
        threshold = im.norm(threshold)
    else:
        threshold = im.norm(data.max())/2.

    # Set default alignment to center, but allow it to be
    # overwritten by textkw.
    kw = dict(horizontalalignment="center",
              verticalalignment="center")
    kw.update(textkw)

    # Get the formatter in case a string is supplied
    if isinstance(valfmt, str):
        valfmt = matplotlib.ticker.StrMethodFormatter(valfmt)

    # Loop over the data and create a `Text` for each "pixel".
    # Change the text's color depending on the data.
    texts = []
    for i in range(data.shape[0]):
        for j in range(data.shape[1]):
            kw.update(color=textcolors[int(im.norm(data[i, j]) > threshold)])
            text = im.axes.text(j, i, valfmt(data[i, j], None), **kw)
            texts.append(text)

    return texts



vegetables = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]
farmers = [1, 2, 3, 4, 5, 6, 7, 8, 9]

data_per = (100. * data / data.sum()).round(0)
data_per_log = np.log(data_per)
harvest = data_per_log.to_numpy()

print(data_per_log)

fig, ax = plt.subplots()

im, cbar = heatmap(harvest, vegetables, farmers, ax=ax,
                   cmap="YlGn", cbarlabel="Percentage")
# texts = annotate_heatmap(im, valfmt="{x:.1f}")

fig.tight_layout()

plt.savefig("heatmap.pdf", bbox_inches='tight', pad_inches=0)