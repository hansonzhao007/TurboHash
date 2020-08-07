#/usr/bin/python3
import csv
import pandas as pd
import matplotlib.pyplot as plt
# import matplotlib as mpl
plt.rcParams["font.family"] = "serif"
data = pd.read_csv('motivation1.csv', skipinitialspace=True)
print(data.dtypes)

r_lines = ['rnd_r_l1_miss', 'rnd_r_tlb_miss', 'seq_r_l1_miss', 'seq_r_tlb_miss']
r_latency = ['rnd_read_latency', 'seq_read_latency']
r_miss_ratio = ['rnd_r_l1_miss_ratio', 'rnd_r_tlb_miss_ratio', 'seq_r_l1_miss_ratio', 'seq_r_tlb_miss_ratio']

w_lines = ['rnd_w_l1_miss', 'rnd_w_tlb_miss', 'seq_w_l1_miss', 'seq_w_tlb_miss']
w_latency = ['rnd_write_latency', 'seq_write_latency']
w_miss_ratio = ['rnd_w_l1_miss_ratio', 'rnd_w_tlb_miss_ratio', 'seq_w_l1_miss_ratio', 'seq_w_tlb_miss_ratio']


data['rnd_r_l1_miss_ratio'] = data.rnd_r_l1_miss / data.rnd_r_l1_load
data['rnd_r_tlb_miss_ratio'] = data.rnd_r_tlb_miss / data.rnd_r_tlb_load
data['seq_r_l1_miss_ratio'] = data.seq_r_l1_miss / data.seq_r_l1_load
data['seq_r_tlb_miss_ratio'] = data.seq_r_tlb_miss / data.seq_r_tlb_load
# print(r_miss_ratio.dtypes)

data['rnd_w_l1_miss_ratio'] = data.rnd_w_l1_miss / data.rnd_w_l1_load
data['rnd_w_tlb_miss_ratio'] = data.rnd_w_tlb_miss / data.rnd_w_tlb_load
data['seq_w_l1_miss_ratio'] = data.seq_w_l1_miss / data.seq_w_l1_load
data['seq_w_tlb_miss_ratio'] = data.seq_w_tlb_miss / data.seq_w_tlb_load



def PlotMiss(data, lines, latency, filename):
    markers = ['x', '+', '.', 'o']
    colors = ['#2077B4', '#FF7F0E', '#2CA02C', '#D62728']
    styles = ['bs-','rs--','b+-','r+--']
    fig, ax = plt.subplots()
    ax2 = ax.twinx()
    data[latency].plot.bar(ax=ax2, alpha=0.8, zorder=-10, color=("white", "white"), edgecolor='k', fontsize=12)
    bars = ax2.patches
    hatches = ''.join(h*len(data) for h in 'x .')
    for bar, hatch in zip(bars, hatches):
        bar.set_hatch(hatch)
    ax2.legend(bbox_to_anchor=[1.08, 1.1], loc='center right', ncol=1, edgecolor='white')
    ax2.set_ylabel('lentency(ns)', fontsize=16)

    data[lines].plot(ax=ax, style=styles, fillstyle='none', markersize=12, zorder=1000,fontsize=12)
    ax.legend(bbox_to_anchor=[-0.08, 1.1], loc='center left', ncol=2, edgecolor='white')
    ax.set_yscale('log')
    ax.set_xticklabels(data.access_size.values, fontsize=16)
    ax.set_xlim([-0.5, 5.5])
    ax.set_ylabel('Number of Misses', fontsize=16)
    ax.set_zorder(ax2.get_zorder()+1)
    ax.patch.set_visible(False)
    ax.set_xlabel("Number of 64-byte Cacheline", fontsize=16)
    fig.savefig(filename, bbox_inches='tight' )

    

def PlotMissRatio(data, lines, latency, filename):
    markers = ['x', '+', '.', 'o']
    colors = ['#2077B4', '#FF7F0E', '#2CA02C', '#D62728']
    styles = ['bs-','rs--','b+-','r+--']
    fig, ax = plt.subplots()
    ax2 = ax.twinx()
    data[latency].plot.bar(ax=ax2, alpha=0.8, zorder=-10, color=("white", "white"), edgecolor='k', fontsize=12)
    bars = ax2.patches
    hatches = ''.join(h*len(data) for h in 'x .')
    for bar, hatch in zip(bars, hatches):
        bar.set_hatch(hatch)
    ax2.legend(bbox_to_anchor=[1.15, 1.1], loc='center right', ncol=1, edgecolor='white')
    ax2.set_ylabel('lentency(ns)', fontsize=16)

    data[lines].plot(ax=ax, style=styles, fillstyle='none', markersize=12, zorder=1000,fontsize=12)
    ax.legend(bbox_to_anchor=[-0.15, 1.1], loc='center left', ncol=2, edgecolor='white')
    ax.set_xticklabels(data.access_size.values, fontsize=16)
    ax.set_xlim([-0.5, 5.5])
    ax.set_ylabel('Miss Ratio', fontsize=16)
    ax.set_zorder(ax2.get_zorder()+1)
    ax.patch.set_visible(False)
    ax.set_xlabel("Number of 64-byte Cacheline", fontsize=16)
    fig.savefig(filename, bbox_inches='tight' )

PlotMiss(data, r_lines, r_latency, "motivation_read.pdf")
PlotMiss(data, w_lines, w_latency, "motivation_write.pdf")

PlotMissRatio(data, r_miss_ratio, r_latency, "motivation_read_ratio.pdf")
PlotMissRatio(data, w_miss_ratio, w_latency, "motivation_write_ratio.pdf")