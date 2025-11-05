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
import { MatSnackBar } from '@angular/material/snack-bar';

import { SlotArrayProxy } from "../../parsing/parser";

import * as Plot from "@observablehq/plot";
import * as d3 from "d3";

@Component({
  selector: 'app-history-plot',
  templateUrl: './generic-plot.component.html'
})
export abstract class GenericPlot {
  _dataframe: SlotArrayProxy[] = [];
  _nodeSet: number[] = [];

  _updateFlipper: boolean = false;

  _textSize: number = 10;

  @Input() set textSize(val: number) {
    this._textSize = val;

    this.update()
  }

  get textSize() {
    return this._textSize;
  }

  @Input() set updateFlipper(val: boolean) {
    this._updateFlipper = val;
    this.update();
  }

  get updateFlipper() {
    return this._updateFlipper;
  }

  @Input() set dataframe(val) {
    this._dataframe = val;

    this.update()
  }

  get dataframe() {
    return this._dataframe;
  }

  @Input() set nodeSet(val) {
    this._nodeSet = val

    this.update()
  }

  get nodeSet() {
    return this._nodeSet;
  }

  abstract get_plot_definition(): {
    x_range: {min: number, max: number},
    width: number,
    plot_def: any
  };

  get_node_id() {
    return '#myplot';
  }

  protected plot_def: any;
  protected x_range: {min: number, max: number} = {min:0, max:100000};
  protected width: number = 100000;

  protected transform: {k: number, x: number, y: number} = {k: 1, x: 0, y:0};


  resetView() {
    this.transform = {k:1, x:0, y:0};
  }

  select() {
    this.resetView()
  }

  deselect() {
  }

  update(){
    if (this._dataframe.length === 0) {
      // TODO: No data to show
      return;
    }

    const plot_definition = this.get_plot_definition();

    this.plot_def = plot_definition.plot_def;
    this.width = plot_definition.width;
    this.x_range = plot_definition.x_range;

    const boundingBox = document.querySelector(this.get_node_id())?.getBoundingClientRect();
    const plotDiv = d3.select(this.get_node_id())

    this.updatePlot(boundingBox, plotDiv, this.transform)

    const zoom = d3.zoom().on("zoom", ({ transform }) => this.updatePlot(boundingBox, plotDiv, transform));
    // @ts-ignore
    plotDiv.call(zoom);
  }

  updatePlot(boundingBox: any, plotDiv: any, transform: any) {
    if (isFinite(transform['x']) && isFinite(this.width) && isFinite(this.x_range.min) && isFinite(this.x_range.max) &&
        !isNaN(transform['x']) && !isNaN(this.width) && !isNaN(this.x_range.min) && !isNaN(this.x_range.max)) {
      if((transform['x'] - (this.width/2)/this.transform['k'] < this.x_range.min)) {
        transform['x'] = this.x_range.min - (transform['x'] - (this.width/2)/this.transform['k'])
      }

      if (transform['k'] !== this.transform['k']) {
        this.transform['k'] = transform['k']
      } else {
        this.transform['x'] = Math.max(transform['x'] - (this.width/2)/this.transform['k'], this.x_range.min);
        //this.transform['y'] = transform['y'];
      }
    }

    const x_range = {
      min: this.transform['x'] - (this.width/2)/this.transform['k'],
      max: this.transform['x'] + (this.width/2)/this.transform['k']
    };

    x_range.min = Math.max(this.x_range.min, x_range.min);
    x_range.max = x_range.min + this.width/this.transform['k'];

    try{
      const plot = Plot.plot({ ...{
          width: boundingBox?.width,
          height: boundingBox?.height,
          marginTop: this._textSize*1 + 3,
          marginBottom: this._textSize*1.8 + 20,
          marginLeft: this._textSize + this._textSize*3*0.65,
          marginRight: this._textSize*1,
      }, ...this.plot_def, ...{
          y: {grid: true, ...this.plot_def.y},
          x: {style: 'font-size: 200px', domain: [x_range.min, x_range.max], ...this.plot_def.x}
      }});

      plot.removeAttribute('width');
      plot.removeAttribute('height');
      plot.setAttribute('style', 'font-size: ' + this._textSize + 'px');

      plotDiv.html("").append(() => plot);

    } catch (err) {
      console.error(err);
      this.snackBar.open('Some error happened during the generation of the plot', 'Close', {
        duration: 2000,
      });

      plotDiv.html("");
    }
  }

  constructor(private snackBar: MatSnackBar) {}
}

export function coerce_to_default(func: (arg: any) => (any|undefined), def?: any, component?: any) {
  return (x: any) => {
    try{
      return func(x);
    } catch(err) {
      console.error(err);
      component?.snackBar.open('An error happened while getting some data. See the console log', 'Close', {
        duration: 2000,
      });
      return def;
    }
  };
}

