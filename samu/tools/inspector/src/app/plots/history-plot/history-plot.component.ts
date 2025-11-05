/*
 * Copyright (c) 2025, University of Trento.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *   Enrico Soprana <enrico.soprana@unitn.it>
 */
import { Component, Input } from '@angular/core';

import { GenericPlot } from '../generic-plot';

import { TsmSlotStatus  } from '../../../parsing/tsmStatus';

import * as Plot from "@observablehq/plot";

function rgbHexToLuminance(str: string) {
  let r = Math.pow(parseInt(str.substring(1,3), 16)/255, 2.2);
  let g = Math.pow(parseInt(str.substring(3,5), 16)/255, 2.2);
  let b = Math.pow(parseInt(str.substring(5,7), 16)/255, 2.2);

  const res = 0.2126 * r + 0.7152 * g + 0.0722 * b;

  return res
}

@Component({
  selector: 'app-history-plot',
  templateUrl: '../generic-plot.component.html',
})
export class HistoryPlotComponent extends GenericPlot {
  _epoch: number = 0;

  @Input() set epoch(val) {
    this._epoch = val

    this.update()
  }

  get epoch() {
    return this._epoch;
  }

  get_plot_definition() {
    const res = {width: 1, x_range: {min:0, max: 1}, plot_def: {}};
    let data = this.dataframe
                   .filter(w => w.epoch == this.epoch)
                   .map(w => w.slots.map(k => {
                     const o = JSON.parse(JSON.stringify(k));
                     o['node_id'] = w.node_id;
                     o['repetition'] = w.repetition;
                     return o;
                   }))
                   .flat();

    const enumValues = [
      TsmSlotStatus.RX_SUCCESS, TsmSlotStatus.RX_TIMEOUT, TsmSlotStatus.RX_ERROR,
      TsmSlotStatus.RX_MALFORMED, TsmSlotStatus.TIMER_EVENT, TsmSlotStatus.TX_DONE,
      TsmSlotStatus.FS_EMPTY, TsmSlotStatus.FS_DETECTED, TsmSlotStatus.FS_DETECTED_AND_PROPAGATED,
      TsmSlotStatus.FS_ERROR, TsmSlotStatus.RX_WITH_SYNCH, TsmSlotStatus.SCAN_WITH_SYNCH,
      TsmSlotStatus.PARSING_ERROR, TsmSlotStatus.PROCESSING_ERROR, TsmSlotStatus.EPOCH_EXTRAPOLATED,
      TsmSlotStatus.END, TsmSlotStatus.UNKNOWN];

    res.width = Math.max(...data.map(d => d['start_minislot'] + d['duration']))
    res.x_range = {min:0, max: res.width};;

    res.plot_def = {
      y: {
        label: 'Node ID',
        tickFormat: (x: number) => this.nodeSet[x],
      },
      x: {
        label: 'Slivers',
      },
      color: {
        type: "categorical",
        domain: enumValues,
        range: enumValues.map(TsmSlotStatus.status_to_color),
      },
      hovermode: "closest",
      hoverinfo: "text",
      marks: [
        Plot.rect(data, {
          y1: (d) => this.nodeSet.indexOf(d.node_id) - 0.5,
          y2: (d) => this.nodeSet.indexOf(d.node_id) + 0.5,
          x1: 'start_minislot',
          x2: (d) => d.start_minislot + d.duration,
          tip: true,
          channels: {
            'Status name': (d) => TsmSlotStatus.status_to_pretty_name(d.status),
            'Node ID': (d) => d.node_id,
            'Duration in slivers': (d) => d.duration,
          },
          fill: (d) => d.status,
          stroke: (d) => (d.repetition != 0)?'#000':TsmSlotStatus.status_to_color(d.status),
        }),
        Plot.text(data, {
          x: (d) => d.start_minislot + d.duration/2,
          y: (d) => this.nodeSet.indexOf(d.node_id),
          text: (d) => TsmSlotStatus.status_to_short_name(d.status),
          fontWeight: 'bold',

          fill: (d) => rgbHexToLuminance(TsmSlotStatus.status_to_color(d.status))>0.5?'#000':'#fff',
          fontSize: this.textSize,
        }),
        Plot.frame()
      ]
    }

    return res;
  }
}
