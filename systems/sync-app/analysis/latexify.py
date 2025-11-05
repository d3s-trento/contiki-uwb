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

                # TEXT SIZE (AXES, TICKS, LEGEND)
				'axes.labelsize': TXT_SIZE - 0.5, # fontsize for x and y labels (was 10)
				'axes.titlesize': TXT_SIZE,
				'font.size': TXT_SIZE, # was 10
				'legend.fontsize': TXT_SIZE - 2.5, # was 10
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
