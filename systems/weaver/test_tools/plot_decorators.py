import matplotlib
import matplotlib.pyplot as plt

def tight_decorator(func):
    """The tight_layout function here issued has
    to be called before saving the picture, hence the
    plot function using this decorator MUST NOT attempt
    to save any figure, but rather return it and let the caller
    save it."""
    def dec(*args, **kwargs):
        retval = func(*args, **kwargs)
        matplotlib.pyplot.tight_layout()
        return retval
    return dec

def boxplot_decorator(func):
    #plt.style.use('ggplot')
    def dec(*args, **kwargs):
        params = {
                #"patch.linewidth"   : .5,       # edge width in points.
                #"patch.antialiased" : True,     # render patches in antialiased (no jaggies)
                #"patch.facecolor"   : "C1",
                #"boxplot.whiskers"  : 1.0,
                #"boxplot.patchartist" : True,
                #"boxplot.showfliers"  : True,
                #"boxplot.flierprops.linewidth" : .8,
                #"boxplot.boxprops.linewidth"   : .8,
                #"boxplot.whiskerprops.linewidth" : .8,
                #"boxplot.capprops.linewidth"     : .8,
                "boxplot.medianprops.linewidth" : 1.5,
                #"boxplot.meanprops.linewidth"   : 2.0,
                }
        matplotlib.rcParams.update(params)
        retval =  func(*args, **kwargs)
        matplotlib.rcdefaults()
        plt.style.use('default')
        return retval

    return dec


def grid_decorator(func):
    #plt.style.use('ggplot')
    AXES_COLOR = "dimgray"
    txt_size = 8
    def dec(*args, **kwargs):
        params = {

                ## ***************************************************************************
                ## * AXES                                                                    *
                ## ***************************************************************************
                #"axes.facecolor"     : white   ## axes background color
                #"axes.edgecolor"     : black   ## axes edge color
                "axes.edgecolor"      : AXES_COLOR,
                #"axes.linewidth"     : 0.8     ## edge linewidth
                "axes.linewidth"      : 1.4,
                #"axes.grid"          : False   ## display grid or not
                "axes.grid"           : True,
                #"axes.grid.axis"     : both    ## which axis the grid should apply to
                #"axes.grid.which"    : major   ## gridlines at {major, minor, both} ticks
                #"axes.titlelocation" : center  ## alignment of the title: {left, right, center}
                #"axes.titlesize"     : large   ## fontsize of the axes title
                "axes.titlesize"      : txt_size,   ## fontsize of the axes title
                #"axes.titleweight"   : normal  ## font weight of title
                #"axes.titlecolor"    : auto    ## color of the axes title, auto falls back to text.color
                                                ## as default value
                #"axes.titlepad"      : 6.0     ## pad between axes and title in points
                #"axes.labelsize"     : medium  ## fontsize of the x any y labels
                "axes.labelsize"      : txt_size - 0.5,
                #"axes.labelpad"      : 4.0     ## space between label and axis
                "axes.labelpad"       : 1.0,
                #"axes.labelweight"   : normal  ## weight of the x and y labels
                #"axes.labelcolor"    : black
                #"axes.axisbelow"     : line    ## draw axis gridlines and ticks:
                                                ##     - below patches (True)
                                                ##     - above patches but below lines ('line')
                                                ##     - above all (False)

                #"axes.formatter.limits" : -5, 6  ## use scientific notation if log10
                                                  ## of the axis range is smaller than the
                                                  ## first or larger than the second
                #"axes.formatter.use_locale" : False  ## When True, format tick labels
                                                      ## according to the user's locale.
                                                      ## For example, use ',' as a decimal
                                                      ## separator in the fr_FR locale.
                #"axes.formatter.use_mathtext" : False  ## When True, use mathtext for scientific
                                                        ## notation.
                #"axes.formatter.min_exponent" : 0  ## minimum exponent to format in scientific notation
                #"axes.formatter.useoffset" : True  ## If True, the tick label formatter
                                                    ## will default to labeling ticks relative
                                                    ## to an offset when the data range is
                                                    ## small compared to the minimum absolute
                                                    ## value of the data.
                #"axes.formatter.offset_threshold" : 4  ## When useoffset is True, the offset
                                                      ## will be used when it can remove
                                                      ## at least this number of significant
                                                     ## digits from tick labels.

                #"axes.spines.left"   : True  ## display axis spines
                #"axes.spines.bottom" : True
                #"axes.spines.top"    : True
                #"axes.spines.right"  : True

                #"axes.unicode_minus" : True  ## use Unicode for the minus symbol
                                              ## rather than hyphen.  See
                                              ## https://en.wikipedia.org/wiki/Plus_and_minus_signs#Character_codes
                #"axes.prop_cycle" : cycler('color', ['1f77b4', 'ff7f0e', '2ca02c', 'd62728', '9467bd', '8c564b', 'e377c2', '7f7f7f', 'bcbd22', '17becf'])
                #"axes.autolimit_mode" : data ## How to scale axes limits to the data.  By using:
                                              ##     - "data" to use data limits, plus some margin
                                              ##     - "round_numbers" move to the nearest "round" number
                #"axes.xmargin"   : .05   ## x margin.  See `axes.Axes.margins`
                "axes.xmargin"    : .1,   ## x margin.  See `axes.Axes.margins`
                #"axes.ymargin"   : .05   ## y margin.  See `axes.Axes.margins`
                "axes.ymargin"    : .1,   ## y margin.  See `axes.Axes.margins`
                #"polaraxes.grid" : True  ## display grid on polar axes
                #"axes3d.grid"    : True  ## display grid on 3d axes
                ## ***************************************************************************
                ## * GRIDS                                                                   *
                ## ***************************************************************************
                #"grid.color"     : b0b0b0  ## grid color
                "grid.color"      : AXES_COLOR,
                #"grid.linestyle" : -       ## solid
                "grid.linestyle" : ":",       ## solid
                #"grid.linewidth" : 0.8     ## in points
                "grid.linewidth" : 1.4,     ## in points
                #"grid.alpha"     : 1.0     ## transparency, between 0.0 and 1.0
                ## ***************************************************************************
                ## * TICKS                                                                   *
                ## ***************************************************************************
                ## See https://matplotlib.org/api/axis_api.html#matplotlib.axis.Tick
                #"xtick.top"           : False   ## draw ticks on the top side
                #"xtick.bottom"        : True    ## draw ticks on the bottom side
                #"xtick.labeltop"      : False   ## draw label on the top
                #"xtick.labelbottom"   : True    ## draw label on the bottom
                #"xtick.major.size"    : 3.5     ## major tick size in points
                #"xtick.minor.size"    : 2       ## minor tick size in points
                #"xtick.major.width"   : 0.8     ## major tick width in points
                #"xtick.minor.width"   : 0.6     ## minor tick width in points
                #"xtick.major.pad"     : 3.5     ## distance to major tick label in points
                "xtick.major.pad"      : 1.0,
                #"xtick.minor.pad"     : 3.4     ## distance to the minor tick label in points
                #"xtick.color"         : black   ## color of the tick labels
                #"xtick.labelsize"     : medium  ## fontsize of the tick labels
                "xtick.labelsize"      : txt_size - 0.8,
                #"xtick.direction"     : out     ## direction: {in, out, inout}
                #"xtick.minor.visible" : False   ## visibility of minor ticks on x-axis
                #"xtick.major.top"     : True    ## draw x axis top major ticks
                #"xtick.major.bottom"  : True    ## draw x axis bottom major ticks
                #"xtick.minor.top"     : True    ## draw x axis top minor ticks
                #"xtick.minor.bottom"  : True    ## draw x axis bottom minor ticks
                #"xtick.alignment"     : center  ## alignment of xticks

                #"ytick.left"          : True    ## draw ticks on the left side
                #"ytick.right"         : False   ## draw ticks on the right side
                #"ytick.labelleft"     : True    ## draw tick labels on the left side
                #"ytick.labelright"    : False   ## draw tick labels on the right side
                #"ytick.major.size"    : 3.5     ## major tick size in points
                #"ytick.minor.size"    : 2       ## minor tick size in points
                #"ytick.major.width"   : 0.8     ## major tick width in points
                #"ytick.minor.width"   : 0.6     ## minor tick width in points
                #"ytick.major.pad"     : 3.5     ## distance to major tick label in points
                "ytick.major.pad"      : 1.0,
                #"ytick.minor.pad"     : 3.4     ## distance to the minor tick label in points
                #"ytick.color"         : black   ## color of the tick labels
                #"ytick.labelsize"     : medium  ## fontsize of the tick labels
                "ytick.labelsize"      : txt_size - 0.8,
                #"ytick.direction"     : out     ## direction: {in, out, inout}
                #"ytick.minor.visible" : False   ## visibility of minor ticks on y-axis
                #"ytick.major.left"    : True    ## draw y axis left major ticks
                #"ytick.major.right"   : True    ## draw y axis right major ticks
                #"ytick.minor.left"    : True    ## draw y axis left minor ticks
                #"ytick.minor.right"   : True    ## draw y axis right minor ticks
                #"ytick.alignment"     : center_baseline  ## alignment of yticks
                ## ***************************************************************************
                ## * FONT                                                                    *
                ## ***************************************************************************
                #"font.family"  : sans-serif
                "font.family"   : "serif",
                #"font.style"   : normal
                #"font.variant" : normal
                #"font.weight"  : normal
                #"font.stretch" : normal
                #"font.size"    : 10.0
                "font.size"      : txt_size,
                #"font.serif"   : DejaVu Serif, Bitstream Vera Serif, Computer Modern Roman, New Century Schoolbook, Century Schoolbook L, Utopia, ITC Bookman, Bookman, Nimbus Roman No9 L, Times New Roman, Times, Palatino, Charter, serif
                "font.serif"    : ["Times"],
                #"font.sans-serif" : DejaVu Sans, Bitstream Vera Sans, Computer Modern Sans Serif, Lucida Grande, Verdana, Geneva, Lucid, Arial, Helvetica, Avant Garde, sans-serif
                #"font.cursive"    : Apple Chancery, Textile, Zapf Chancery, Sand, Script MT, Felipa, cursive
                #"font.fantasy"    : Comic Neue, Comic Sans MS, Chicago, Charcoal, ImpactWestern, Humor Sans, xkcd, fantasy
                #"font.monospace"  : DejaVu Sans Mono, Bitstream Vera Sans Mono, Computer Modern Typewriter, Andale Mono, Nimbus Mono L, Courier New, Courier, Fixed, Terminal, monospace
                ## ***************************************************************************
                ## * TEXT                                                                    *
                ## ***************************************************************************
                #"text.color" : black
                #"text.usetex" : False
                "text.usetex" : True,
                "mathtext.rm"           : "serif",
                "mathtext.fontset"      : "stix",
                #"text.latex.preview" : False
                #"text.hinting" : auto  ## May be one of the following: none | auto | native | either
                #"text.hinting_factor" : 8
                #"text.kerning_factor" : 0
                #"text.antialiased" : True
                ## ***************************************************************************
                ## * LEGEND                                                                  *
                ## ***************************************************************************
                #"legend.loc"           : best
                #"legend.frameon"       : True     ## if True, draw the legend on a background patch
                #"legend.framealpha"    : 0.8      ## legend patch transparency
                #"legend.facecolor"     : inherit  ## inherit from axes.facecolor; or color spec
                #"legend.edgecolor"     : 0.8      ## background patch boundary color
                #"legend.fancybox"      : True     ## if True, use a rounded box for the
                                                 ## legend background, else a rectangle
                #"legend.shadow"        : False    ## if True, give background a shadow effect
                #"legend.numpoints"     : 1        ## the number of marker points in the legend line
                #"legend.scatterpoints" : 1        ## number of scatter points
                #"legend.markerscale"   : 1.0      ## the relative size of legend markers vs. original
                "legend.markerscale"    : 0.8,
                #"legend.fontsize"      : medium
                "legend.fontsize"       : txt_size - .8,
                #"legend.title_fontsize" : None    ## None sets to the same as the default axes.

                #"# Dimensions as fraction of fontsize:
                #"legend.borderpad"     : 0.4  ## border whitespace
                "legend.borderpad"      : 0.2,
                #"legend.labelspacing"  : 0.5  ## the vertical space between the legend entries
                "legend.labelspacing"   : -0.06,
                #"legend.handlelength"  : 2.0  ## the length of the legend lines
                "legend.handlelength"   : 1.3,
                #"legend.handleheight"  : 0.7  ## the height of the legend handle
                #"legend.handletextpad" : 0.8  ## the space between the legend line and legend text
		"legend.handletextpad"  : 0.2,
                #"legend.borderaxespad" : 0.5  ## the border between the axes and legend edge
                #"legend.columnspacing" : 2.0  ## column separation
                "legend.columnspacing"  : 1.1,

        }
        matplotlib.rcParams.update(params)
        fig =  func(*args, **kwargs)
        for ax in fig.axes:
            format_axes(ax, "dimgray")
        matplotlib.rcdefaults()
        plt.style.use('default')
        return fig

    return dec

def lines_decorator(func):
    #plt.style.use('ggplot')
    def dec(*args, **kwargs):
        params = {
                ## ***************************************************************************
                ## * LINES                                                                   *
                ## ***************************************************************************
                #"lines.linewidth" : 1.5  ## line width in points
                #"lines.linestyle" : -    ## solid line
                #"lines.color"     : C0   ## has no affect on plot(); see axes.prop_cycle
                #"lines.marker"          : None  ## the default marker
                "lines.marker"           : "v",
                #"lines.markerfacecolor" : auto  ## the default marker face color
                #"lines.markeredgecolor" : auto  ## the default marker edge color
                #"lines.markeredgewidth" : 1.0   ## the line width around the marker symbol
                #"lines.markersize"      : 6     ## marker size, in points
                "lines.markersize"       : 3,     ## marker size, in points
                #"lines.dash_joinstyle"  : round       ## {miter, round, bevel}
                #"lines.dash_capstyle"   : butt        ## {butt, round, projecting}
                #"lines.solid_joinstyle" : round       ## {miter, round, bevel}
                #"lines.solid_capstyle"  : projecting  ## {butt, round, projecting}
                #"lines.antialiased"     : True  ## render lines in antialiased (no jaggies)

                ## The three standard dash patterns.  These are scaled by the linewidth.
                #"lines.dashed_pattern"  : 3.7, 1.6
                #"lines.dashdot_pattern" : 6.4, 1.6, 1, 1.6
                #"lines.dotted_pattern"  : 1, 1.65
                #"lines.scale_dashes"    : True

                #"markers.fillstyle"     : full  ## {full, left, right, bottom, top, none}

                #"patch.linewidth"       : .5,       # edge width in points.
                "patch.linewidth"        : 1.2,
                #"patch.antialiased"     : True,     # render patches in antialiased (no jaggies)
                #"patch.facecolor"       : "C1",
        }
        matplotlib.rcParams.update(params)
        retval =  func(*args, **kwargs)
        matplotlib.rcdefaults()
        plt.style.use('default')
        return retval

    return dec


def get_fig_dimensions(nrows=1, ncols=1, width=None, height=None, ratio_wh=1.61):
    if width is not None and height is None:
        height = width / ratio_wh

    elif height is not None and width is None:
        width  = height * ratio_wh

    if width is None and height is None:
        width = 3.25
        height = width / ratio_wh

    return ncols * width, nrows * height

def format_axes(ax, spine_color):
    for spine in ['top', 'right']:
        ax.spines[spine].set_visible(False)

    for spine in ['left', 'bottom']:
        ax.spines[spine].set_color(spine_color)
        ax.spines[spine].set_linewidth(0.8)

    ax.xaxis.set_ticks_position('bottom')
    ax.yaxis.set_ticks_position('left')

    fmt = matplotlib.ticker.StrMethodFormatter("{x}")
    for axis in [ax.xaxis, ax.yaxis]:
        axis.set_tick_params(direction = 'out', color=spine_color)
        ax.xaxis.set_major_formatter(fmt)
        ax.yaxis.set_major_formatter(fmt)

    return ax
