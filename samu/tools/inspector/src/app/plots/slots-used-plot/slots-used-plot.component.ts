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

import { GenericPlot, coerce_to_default } from '../generic-plot';

import * as Plot from "@observablehq/plot";

@Component({
  selector: 'app-slots-used-plot',
  templateUrl: '../generic-plot.component.html',
})
export class SlotsUsedPlotComponent extends GenericPlot {
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

    let data = this.dataframe.filter(w => w.epoch == this.epoch);

    res.width = this._nodeSet.length;
    res.x_range = {min: 0, max: res.width};

    res.plot_def = {
      x: {
        label: 'Node ID',
        tickFormat: (x: number) => this._nodeSet[x],
      },
      y: {
        label: 'Number of slivers used'
      },
      marks: [
        Plot.dot(data, {
          y: coerce_to_default((d) => d.nslots, undefined, this),
          x: (d) => this._nodeSet.indexOf(d.node_id),
          tip: 'xy',
          channels: {
            'Node ID': 'node_id'
          }
        }),
        Plot.frame()
      ]
    };

    return res;
  }
}
