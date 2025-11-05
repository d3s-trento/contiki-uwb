#!/usr/bin/env python

import argparse
import subprocess
from io import StringIO

import pandas as pd
import numpy as np

from matplotlib import pyplot as plt

parser = argparse.ArgumentParser(
                    prog='multi_delay.py',
                    description='Show the delay of the synchronization flood as a boxplot over multiple experiments')

def parse_name_job_ids(s: str) -> tuple[str, list[int]]:
    ls = s.split(';')

    name = str(ls[0])
    period = float(ls[1])
    job_ids = list(map(int, ls[2:]))
    
    return name, period, job_ids

parser.add_argument('jobs', type=parse_name_job_ids, nargs='+')
parser.add_argument('--node', type=int, required=False, default=None)

args = parser.parse_args()


complete_df = pd.DataFrame()

for name, period, job_ids in args.jobs:
    for job_id in job_ids:
        p = subprocess.Popen(['get_delay.sh', f'{job_id}'], stdout=subprocess.PIPE)
        sio = StringIO(p.communicate()[0].decode('utf-8'))
        ddDelay = pd.read_csv(sio)

        ddDelay['epoch'] -= 1 # These are written after increasing the epoch number, so reduce it by 1 (to get the real one)

        p = subprocess.Popen(['get_bootstrap.sh', f'{job_id}'], stdout=subprocess.PIPE)
        sio = StringIO(p.communicate()[0].decode('utf-8'))
        ddBootstrap = pd.read_csv(sio)

        #dd = ddDelay
        dd = ddDelay.merge(ddBootstrap, how='inner', on=['node_id', 'epoch'])

        if args.node is not None:
            dd = dd[dd['node_id'] == args.node]

        #dd = dd[dd['epoch'].between(dd['epoch'].min() + 30, dd['epoch'].max() - 10)]
        dd = dd[dd['epoch'].between(max(dd['epoch'].min() + 5, 200), dd['epoch'].max() - 10)]
        #dd = dd[dd['epoch'].between(max(dd['epoch'].min() + 5, 40), dd['epoch'].max() - 10)]

        dd['delay_us'] = dd['delay_tick'] * 4.0064102564 / 1000

        dd['name'] = name
        dd['period'] = period
        dd['job_id'] = job_id

        dd = dd[dd['scan'] == 0]

        # dd['is_scan'] = (dd['scan'] == 0)

        # def has_been_in_scan_from(x, n_iter):
        #     v = x
        #     for i in range(n_iter):
        #         shifted_bitmap = [*([True]*i), *(x[:len(x)-i])]
        #         print(f'{i}, {len(shifted_bitmap)}, {len(v)}')
        #         v = v & shifted_bitmap

        #     return v

        # # Make sure we remove a) the results of when we have been in scan, b) the results immediately after
        # dd['has_not_been_in_scan_from'] = dd.groupby('node_id')['is_scan'].transform(lambda x: has_been_in_scan_from(x, 6))
        # dd = dd[dd['has_not_been_in_scan_from']]


        complete_df = pd.concat([complete_df, dd])

print(complete_df)

'''
Latexify Function obtained from:
http://nipunbatra.github.io/2014/08/latexify/
'''

def latexify(fig_width = None, fig_height = None, columns = 1, font_size = 8):
        """
        Set up matplotlib's RC params for LaTeX plotting.
        Call this before plotting a figure.

        Parameters
        ----------
        fig_width : float, optional, inches
        fig_height : float,  optional, inches
        columns : {1, 2}
        """

        # code adapted from http://www.scipy.org/Cookbook/Matplotlib/LaTeX_Examples

        # Width and max height in inches for IEEE journals taken from
        # computer.org/cms/Computer.org/Journal%20templates/transactions_art_guide.pdf

        assert(columns in [1,2])
        from math import sqrt
        import matplotlib
        TXT_SIZE = font_size

        if fig_width is None:
                fig_width = 3.39 if columns == 1 else 6.9 # width in inches

        if fig_height is None:
                golden_mean = (sqrt(5) - 1.0) / 2.0    # Aesthetic ratio
                fig_height = fig_width * golden_mean # height in inches

        MAX_HEIGHT_INCHES = 8.0
        if fig_height > MAX_HEIGHT_INCHES:
            print("WARNING: fig_height too large: {} so will reduce to {} inches".format(
                fig_height,
                MAX_HEIGHT_INCHES))
            fig_height = MAX_HEIGHT_INCHES

        print(fig_width, fig_height)

        # Qualifying Presentation Configuration
        params = {'backend': 'ps',

                "pgf.texsystem": "pdflatex",        # change this if using xetex or lautex

                'pgf.preamble': "\n".join([
                    r"\usepackage[utf8x]{inputenc}",    # use utf8 fonts 
                    r"\usepackage[T1]{fontenc}",        # plots will be generated
                    r"\usepackage[detect-all,locale=US]{siunitx}",
                    r"\usepackage{palatino, mathpazo}"
                ]),

                'text.usetex': True,
                'text.latex.preamble': "\n".join([
                    r"\usepackage[T1]{fontenc}",        # plots will be generated
                    r"\usepackage[detect-all,locale=US]{siunitx}",
                    r"\usepackage{palatino, mathpazo}"
                ]),


                # TEXT SIZE (AXES, TICKS, LEGEND)
                'axes.labelsize': TXT_SIZE - 0.5, # fontsize for x and y labels (was 10)
                'axes.titlesize': TXT_SIZE,
                'font.size': TXT_SIZE, # was 10
                'legend.fontsize': TXT_SIZE - 2.5, # was 10
                'legend.frameon': False,
                'xtick.labelsize': TXT_SIZE - 0.8,
                'ytick.labelsize': TXT_SIZE - 0.8,

                # FONT TYPE
                'text.usetex': True,
                #'text.latex.preamble': '\\usepackage{libertine}',
                'font.family': 'serif',
                #'font.family': 'Myriad Pro',
                'font.serif': ['Palatino', 'Times New Roman'],
                #'font.serif': ['Linux Libertine'],
                #'font.sans-serif': ['Linux Libertine Sans'],
                #'font.monospace': ['Linux Libertine Mono'],

                # FIGURE SIZE
                'figure.figsize': [fig_width, fig_height],
                'lines.linewidth' : 1.2, # 1.5
                'lines.markersize' : 0.8,

                # AXES
                'axes.labelpad': 0.8,

                # TICKS
                'xtick.major.pad': 1.0,
                'ytick.major.pad': 1.0,

                # LEGEND
                'legend.markerscale': 0.8,
                'legend.handletextpad': 0.2,
                'legend.columnspacing': 1.1,
                'legend.labelspacing': -0.06,
                'legend.borderpad': 0.2,
                'legend.handlelength': 1.3,

                # GRID
                'grid.color': 'grey',
                #'grid.linewidth': 1
        }
        matplotlib.rcParams.update(params)

def format_axes(ax, show_spines=False, spine_color='gray'):

        for spine in ['left', 'bottom', 'top', 'right']:
                ax.spines[spine].set_color(spine_color)
                ax.spines[spine].set_linewidth(0.8)

        for spine in ['top', 'right']:
                ax.spines[spine].set_visible(show_spines)

        ax.xaxis.set_ticks_position('bottom')
        ax.yaxis.set_ticks_position('left')

        for axis in [ax.xaxis, ax.yaxis]:
                axis.set_tick_params(direction = 'out', color = spine_color)

        return ax

latexify(fig_width=6, fig_height=3, columns=1, font_size=12.5)

fig,ax = plt.subplots()

COLORS = ['tab:blue', 'tab:orange', 'tab:green', 'tab:red', 'tab:purple', 'tab:brown', 'tab:pink', 'tab:olive', 'tab:cyan']

n_periods = len(complete_df['period'].unique())

for pi, ((period, n), df) in enumerate(complete_df.groupby(['period', 'name'])):
    print(f'Dataset {n} ({len(df)} points) [{df["delay_us"].min()}, {df["delay_us"].max()}]')
    print(df.sort_values('delay_us').head(50))
    print(df.sort_values('delay_us').tail(50))

    color = COLORS[pi]

    for i in range(1,8):
        s = (10**(-i))/2
        b = 1 - s
        p_s = df["delay_us"].quantile(s)
        p_b = df["delay_us"].quantile(b)
        print(f'Percentiles p{s:.9f} {p_s:8.3f} p{b:.9f} {p_b:8.3f} interval p{b-s:.9f} {abs(p_b-p_s):8.3f}')

        #if i == 3:
        #    extra_bar_y = 0.5 + (pi - n_periods/2 + 1) * 0.05
        #    ax.plot([p_s, p_b], [extra_bar_y, extra_bar_y], zorder=1000-int(period*10), marker='|', markersize=5, color=color)
    print('Outside ±4 {:.4f}% ({}/{})'.format((df['delay_us'].abs() > 4).mean()*100, sum(df['delay_us'].abs() > 4), len(df)))
    print('Outside ±4 (wo 144) {:.4f}%'.format((df[df['node_id']!=144]['delay_us'].abs() > 4).mean()*100))


    ys = np.linspace(0, 1, num=1000)
    xs = [df['delay_us'].quantile(q) for q in ys]

    #ax.scatter(df['delay_us'], df['epoch'])

    ax.plot(xs,ys,label=n, zorder=1000-int(period*10),color=color)
    ax.set_ylabel('CDF')

ax.legend(loc='upper center', ncol=8, bbox_to_anchor=(0.5, 1.17), frameon=False)
#ax.legend()
ax.set_xlabel(r'$S_{err}$ [$\SI{}{\us}$]')
SLACK = 0.025
#ax.set_ylim([0-SLACK,1 + SLACK])
#ax.set_xlim([-5, +5])
#ax.set_xlim([-1,+1])
ax.grid()
fig.tight_layout()

plt.savefig('cdf_plot.pdf')


