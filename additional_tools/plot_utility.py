import argparse

import io
import subprocess

def add_plot_arguments(parser: argparse.ArgumentParser):
    parser.add_argument('--no-plot', dest='plot', default=True, action='store_false', help='Do not show the plot')

    parser.add_argument('--xcm', dest='xcm', default=None, type=float, help='cm dimensions in x')
    parser.add_argument('--ycm', dest='ycm', default=None, type=float, help='cm dimensions in y')

    parser.add_argument('--slides', dest='slides', default=False, action='store_true', help='Generate for slides')

    parser.add_argument('--save', dest='save', default=None, type=str, help='Name with which to save the file')

    parser.add_argument('--minY', dest='minY', default=None, type=float, help='Minimum y value to use')
    parser.add_argument('--maxY', dest='maxY', default=None, type=float, help='Maximum y value to use')

    parser.add_argument('--minX', dest='minX', default=None, type=float, help='Minimum x value to use')
    parser.add_argument('--maxX', dest='maxX', default=None, type=float, help='Maximum x value to use')

    parser.add_argument('--tex', dest='tex', default=False, action='store_true', help='Show using latex or not')


def plot(args_namespace: argparse.Namespace, *, attach_subplots=False):
    if args_namespace.xcm is not None and args_namespace.ycm is None:
        args_namespace.ycm = args_namespace.xcm/1.618033988749

    if args_namespace.save is not None and args_namespace.xcm is None:
        raise Exception('You forgot to put the dimensions')

    def wrapper(func):
        def inner(*args, **kwargs):
            if not args_namespace.plot:
                return

            import matplotlib
            from matplotlib import pyplot as plt

            if args_namespace.tex:
                if args_namespace.save is not None and args_namespace.save.endswith('pdf'):
                    matplotlib.use('pgf')

                    matplotlib.rcParams.update({
                        'backend': 'ps',
                        "pgf.texsystem": "pdflatex",        # change this if using xetex or lautex

                        'pgf.preamble': "\n".join([
                            r"\usepackage[utf8x]{inputenc}",    # use utf8 fonts 
                            r"\usepackage[T1]{fontenc}",        # plots will be generated
                            r"\usepackage[detect-all,locale=US]{siunitx}",
                            r"\usepackage{palatino, mathpazo}"
                        ])
                    })
                else:
                    matplotlib.rcParams['text.latex.preamble'] += "\n".join([
                            r"\usepackage[T1]{fontenc}",        # plots will be generated
                            r"\usepackage[detect-all,locale=US]{siunitx}",
                            r"\usepackage{palatino, mathpazo}"
                        ])

                if args_namespace.xcm is not None or args_namespace.ycm is not None:
                    matplotlib.rcParams.update({
                        'figure.constrained_layout.use': True,
                        'figure.constrained_layout.wspace': 0,
                        'figure.constrained_layout.hspace': 0,
                        'figure.constrained_layout.w_pad':  0,
                        'figure.constrained_layout.h_pad':  0,

                        'xtick.labelsize': 7,
                        'ytick.labelsize': 7,
                        'axes.labelsize':  8,

                        'legend.fontsize': 8,
                        'legend.frameon': False,
                        'legend.borderaxespad': 0
                    })


                matplotlib.rcParams.update({
                    'text.usetex': True,
                    'font.family': 'serif',
                    'font.sans-serif': ['Palatino']
                })

            fig = func(*args, **kwargs)

            import sys
            fig.canvas.manager.set_window_title(' '.join(sys.argv))

            if (args_namespace.minY is not None) or (args_namespace.maxY is not None):
                for ax in fig.axes:
                    ax.set_ylim((args_namespace.minY, args_namespace.maxY))

            if (args_namespace.minX is not None) or (args_namespace.maxX is not None):
                for ax in fig.axes:
                    ax.set_xlim((args_namespace.minX, args_namespace.maxX))

            if not args_namespace.tex:
                plt.show()
                return

            if args_namespace.xcm is not None and args_namespace.ycm is not None:
                fig.set_size_inches((args_namespace.xcm*0.3937008, args_namespace.ycm*0.3937008))
                fig.canvas.draw()
                fig.set_constrained_layout(False)

                if attach_subplots and len(fig.axes) == 2: # TODO: This only works with 2 subplots, but it should be enough
                    ax, ax2 = fig.axes

                    bbox1 = ax.get_position()
                    bbox2 = ax2.get_position()
                    ax2.set_position([bbox1.xmax, bbox2.ymin, bbox2.xmax-bbox1.xmax, bbox2.ymax-bbox2.ymin])
            else:
                plt.subplots_adjust(wspace=0, hspace=0)
                plt.tight_layout()

            if args_namespace.save is None:
                def add_figure_to_clipboard(event):

                    if event.key == "ctrl+c":
                       with io.BytesIO() as buffer:
                            fig.savefig(buffer, format='png')

                            output = subprocess.Popen(("wl-copy", "-t", "image/png"), stdin=subprocess.PIPE)
                            output.stdin.write(buffer.getvalue())
                            output.stdin.close()

                fig.canvas.mpl_connect('key_press_event', add_figure_to_clipboard)

                plt.show()
            else:
                plt.savefig(args_namespace.save, dpi=300)

            return fig

        return inner

    return wrapper

