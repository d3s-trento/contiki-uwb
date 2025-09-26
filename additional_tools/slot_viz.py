#!/usr/bin/env python3
import json
from pathlib import Path
from collections import OrderedDict

import pandas as pd

import matplotlib.pyplot as plt
import matplotlib.patches as patch
import matplotlib.text as texts

from matplotlib.widgets import Slider


SLOT_PLOT_REFERENCE = None

class SlotRectangle(patch.Rectangle):
    def __init__(self, xy, width, height, angle=0.0, status="NA", node=None, slot_idx=None, **kwargs):
        if status == "NA":
            kwargs["color"] = "linen"

        elif status == "R":
            kwargs["color"] = "darkseagreen"

        elif status == "E":
            kwargs["color"] = "tomato"

        elif status == "L":
            kwargs["color"] = "silver"

        elif status == "T":
            kwargs["color"] = "cornflowerblue"

        elif status == "O":
            kwargs["color"] = "orange"

        elif status == "Ra":
            kwargs["color"] = "goldenrod"
        elif status == "~":
            kwargs["color"] = "blueviolet"
        elif status == "D":
            kwargs["color"] = "aquamarine"
        elif status == "!":
            kwargs["color"] = "aqua"
        elif status == "X":
            kwargs["color"] = "saddlebrown"
        else:
            kwargs["color"] = "deeppink"

        super().__init__(xy, width=width, height=height, angle=angle, **kwargs)

        self.node = node
        self.slot_idx = slot_idx

class SlotPlotSlider:
    def __init__(self, ax, slot_plot, step=0.2):
        if step < 0.0 or step > 1.0:
            raise ValueError(f"Invalid step value. Step = {step} not in range 0.0-1.0")

        self.ax = ax
        self.step = step
        self.slider = Slider(self.ax, '', 0.0, 1.0 - step, valinit=0.0)
        self.slot_plot = slot_plot

        axis_size = (self.slot_plot.xlim[1] - self.slot_plot.xlim[0])
        min_ = self.slider.val * axis_size
        max_ = min_ + (axis_size * step)
        self.slot_plot.current_xlim = (min_,  max_)
        self.current_step = 0

        self.slider.on_changed(self.update)

    def update(self, val):
        pos = self.slider.val
        step = int(pos / self.step)
        if step == self.current_step:
            return

        axis_size = (self.slot_plot.xlim[1] - self.slot_plot.xlim[0])
        min_ = pos * axis_size
        max_ = min_ + self.step * axis_size
        self.slot_plot.current_xlim = (min_, max_)
        self.slot_plot.clean_draw()

        self.current_step = step

class SlotPlot:
    def __init__(self, ax):
        self.ax = ax
        self.slot_annotations = {}
        self.slot_rectangles  = {}
        self.slot_texts = {}
        self.background = None

        self.xlim = (0, 100)
        self.current_xlim = self.xlim
        self.ylim = (0, 100)
        self.current_ylim = self.ylim
        self.yticks      = []
        self.yticklabels = []
        self.xticks      = []
        self.xticklabels = []

        self.pick_event = self.ax.figure.canvas.mpl_connect('pick_event', self.show_annotation_on_click)
        self.ax.figure.canvas.mpl_connect('resize_event', self.on_resize)

    def clean_draw(self):
        # perform a first draw with all slots - this is our clear state
        self.ax.clear()
        canvas = self.ax.figure.canvas

        self.ax.set_axisbelow(True)
        self.ax.grid(True, which='both', linestyle='dashed')

        self.ax.set_yticks([])
        self.ax.set_yticks(self.yticks)
        self.ax.set_yticklabels(self.yticklabels, fontsize=24)

        self.ax.set_xticks([])
        self.ax.set_xticks(self.xticks)
        self.ax.set_xticklabels(self.xticklabels, fontsize=24)

        self.ax.set_ylim(self.current_ylim)
        self.ax.set_xlim(self.current_xlim)

        for rect in self.slot_rectangles.values():
            self.ax.add_artist(rect)

        for text in self.slot_texts.values():
            self.ax.add_artist(text)

        canvas.draw()
        # save the background for future
        self.background = canvas.copy_from_bbox(self.ax.bbox)

        for annot in self.slot_annotations.values():
            self.ax.add_artist(annot)

        for annot in self.slot_annotations.values():
            self.ax.draw_artist(annot)

        canvas.blit(self.ax.bbox) # blit annotations

    def on_resize(self, event):
        self.clean_draw()

    def show_annotation_on_click(self, event):
        """When overing over a slot rectangle, make the corresponsing
        annotation visible."""
        canvas = self.ax.figure.canvas
        canvas.mpl_disconnect(self.pick_event) # avoid f*** recursion, stupid matplotlib
        canvas.stop_event_loop()
        canvas.flush_events()

        slot_rect = event.artist
        if not slot_rect.contains(event.mouseevent):
            return

        try:
            annot = self.slot_annotations[(slot_rect.node, slot_rect.slot_idx)]

            if annot.get_visible() is True:
                annot.set_visible(False)
            else:
                annot.set_visible(True)

            if self.background is not None:
                canvas.restore_region(self.background)
                for a in self.slot_annotations.values():
                    self.ax.draw_artist(a)

                canvas.blit(self.ax.bbox)
            else:
                self.clean_draw()

        except KeyError: # clicked on a NA
            pass

        finally:
            self.pick_event = self.ax.figure.canvas.mpl_connect('pick_event', self.show_annotation_on_click)
            canvas.start_event_loop()


# ======================================================================

def serialize_slots(epoch_pd, nodes, savedir="."):

    epoch = epoch_pd.epoch.unique()
    if len(epoch) > 1 or len(epoch) == 0:
        print(epoch_pd)
        raise ValueError("The datagram must contain entries of single epoch")
    epoch = epoch[0]

    nodes  = list(sorted(nodes))

    # determine the y position (row) of each node - this won't change
    nodes_ypos = {nodes[i]: i for i in range(0, len(nodes))}

    slots = sorted(epoch_pd["slot_idx"].unique())
    slots = list(range(slots[0], slots[-1] + 1))

    # compute the x pos (column) of each slot
    slots_xpos = {slots[i]: i for i in range(0, len(slots))}
    cells = OrderedDict(((node, OrderedDict()) for node in nodes))

    for slot_idx, slot_pd in epoch_pd.groupby("slot_idx", as_index=False):

        # small check
        #if sorted(list(slot_pd["node_id"])) != nodes:
        #    raise ValueError(f"Error in data processing. A node is present twice in slot {slot_idx}.")

        node_data = {row["node_id"]: {field: row[field] for field in row.keys()} for row in slot_pd.to_dict(orient="row")}
        nodes_xpos = {node: slots_xpos[slot_idx] for node in nodes}

        for node in slot_pd["node_id"].unique():
            status = node_data[node]['status']
            if not isinstance(status, str):
                status = "NA"

            if status == "NA":
                # data is nan
                lhs, dist, sender_id, acked, buffer_ = tuple([None] * 5)

            cells[node][slot_idx] = { "status": status }

    with open(str(Path(savedir).joinpath(f"epoch_{epoch}.json")), "w") as fh:
        json.dump(cells, fp=fh, indent=4)

def plot_slots(epoch_pd, nodes, slots_per_fig=5, nodes_per_fig=5, savedir=".", interactive=False):
    global SLOT_PLOT_REFERENCE

    epoch = epoch_pd.epoch.unique()
    if len(epoch) > 1 or len(epoch) == 0:
        raise ValueError("The datagram must contain entries of single epoch")
    epoch = epoch[0]

    def draw_text(x, y, text, ax=None, fontsize=30):
        if str(text).split()[0] == "nan":
            text = "NA"
        kwargs = {"horizontalalignment": "center", "verticalalignment": "center"}
        if ax is None:
            return plt.text(x, y, s=text, fontsize=fontsize, clip_on=True, **kwargs) # clip_on: if true do not draw text outside axes...
        else:
            return  ax.text(x, y, s=text, fontsize=fontsize, clip_on=True, **kwargs)

    ratio  = 16/9
    box_size = 1 #px
    offset = 1
    box_w = box_size / ratio
    box_h = box_size * ratio

    nodes  = list(sorted(nodes))

    # determine the y position of each node - this won't change
    nodes_ypos = {nodes[i]: i * (box_h) for i in range(0, len(nodes))}

    slots = sorted(epoch_pd["slot_idx"].unique())
    slots = list(range(slots[0], slots[-1] + 1))

    epoch_pd = epoch_pd[epoch_pd['slot_idx'].isin(slots)]

    # compute the x pos of each slot
    slots_xpos = {slots[i]: i * (box_w) for i in range(0, len(slots))}
    max_x, max_y = len(slots) * (box_w), len(nodes) * (box_h)

    figsize= (int(5 * (len(slots) / slots_per_fig + 1)), int(5 * (len(nodes) / nodes_per_fig + 1)))
    fig = plt.figure(figsize=figsize)
    ax = fig.gca()

    slot_plot = SlotPlot(ax)
    SLOT_PLOT_REFERENCE = slot_plot
    slot_plot.xlim = (0, max_x)
    slot_plot.current_xlim = (0, max_x)
    slot_plot.ylim = (0, max_y)
    slot_plot.current_ylim = (0, max_y)
    slot_plot.yticks = [y + box_h / 2 for y in nodes_ypos.values()]
    slot_plot.yticklabels = nodes
    slot_plot.xticks = [x + box_w / 2 for x in slots_xpos.values()]
    slot_plot.xticklabels = slots

    for index, row in epoch_pd.iterrows():

        node = row['node_id']
        slot_idx = row['slot_idx']
        status = row['status']

        if not isinstance(status, str):
            status = "NA"

        xpos = slots_xpos[slot_idx]
        ypos = nodes_ypos[node]

        slot_rect  = SlotRectangle((xpos, ypos), width=box_w, height=box_h,\
                                         status=status, node=node, slot_idx=slot_idx, picker=True)
        slot_plot.slot_rectangles[(node, slot_idx)] = slot_rect


        text = draw_text(slots_xpos[slot_idx] + box_w / 2, nodes_ypos[node] + box_h / 2, text=f"{status}", ax=slot_plot.ax)
        slot_plot.slot_texts[(node, slot_idx)] = text

    # plot the chart
    slot_plot.clean_draw()

    plt.tight_layout()
    plt.savefig(str(Path(savedir).joinpath(f"epoch_{epoch}.pdf")))

    plt.close()
    SLOT_PLOT_REFERENCE = None


if __name__ == "__main__":
    pass
