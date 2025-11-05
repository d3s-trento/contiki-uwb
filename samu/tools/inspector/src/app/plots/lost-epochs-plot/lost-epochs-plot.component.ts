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

import * as Plot from "@observablehq/plot";
import * as d3 from "d3";

@Component({
  selector: 'app-lost-epochs-plot',
  templateUrl: '../generic-plot.component.html',
})
export class LostEpochsPlotComponent extends GenericPlot {
  _minEpoch: number = 0;
  _maxEpoch: number = 0;

  @Input() set minEpoch(val: number) {
    this._minEpoch = val;
  }

  @Input() set maxEpoch(val: number) {
    this._maxEpoch = val;
  }

  get_plot_definition() {
    const res = {width: 1, x_range: {min:0, max: 1}, plot_def: {}};

    res.x_range = {min: this._minEpoch, max: this._maxEpoch};
    res.width = this._maxEpoch - this._minEpoch;

    const data = d3.groups(this.dataframe, (d) => d.epoch)
        .map(x => {
          const nodeInThisEpoch = new Set(x[1].map(w => w.node_id));

          const res: [number, number[]] = [x[0], this._nodeSet.filter((x: number) => !nodeInThisEpoch.has(x))];

          return res;
        }).filter((x: [number, number[]]) => x[1].length > 0)
        .map(x => x[1].map(y => {return {epoch: x[0], node_id: y}}))
        .flat()

    res.plot_def = {
      y: {
        label: 'Node ID',
        tickFormat: (x: number) => this._nodeSet[x],
        domain: [0, this._nodeSet.length]
      },
      x: {
        label: 'Epoch',
        domain: [this._minEpoch, this._maxEpoch],
      },
      marks: [
        // @ts-ignore
        Plot.rect(data, {
          y1: (d) => this._nodeSet.indexOf(d.node_id) - 0.5,
          y2: (d) => this._nodeSet.indexOf(d.node_id) + 0.5,
          x1: (d) => d.epoch-0.5,
          x2: (d) => d.epoch + 0.5,
          //tip: 'xy',
        }),
        Plot.tip(data, Plot.pointer({
          y1: (d) => this._nodeSet.indexOf(d.node_id) - 0.5,
          y2: (d) => this._nodeSet.indexOf(d.node_id) + 0.5,
          x1: (d) => d.epoch-0.5,
          x2: (d) => d.epoch + 0.5,
          title: (d) => {
            return `Node ID: ${d.node_id}\n` +
                   `Epoch: ${d.epoch}`;
          }
        })),
        Plot.frame()
      ]
    };

    return res;
  }
}
