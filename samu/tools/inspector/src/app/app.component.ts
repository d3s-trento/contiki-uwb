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
import { Component, ViewChildren, QueryList } from '@angular/core';
import { MatSnackBar } from '@angular/material/snack-bar';

import { SlotArrayProxy } from "../parsing/parser";

// @ts-ignore
import * as untar from 'js-untar';
import * as pako from 'pako';


@Component({
  selector: 'app-root',
  templateUrl: './app.component.html',
  styleUrls: ['./app.component.css'],
})
export class AppComponent {
  _dataframe: SlotArrayProxy[] = [];
  _test_info: any = {};

  _filtered_dataframe: SlotArrayProxy[] = [];

  private _epoch: number = 35;

  _complete_nodeSet: number[] = [];
  _nodeSet: number[] = [];
  minEpoch: number = 0;
  maxEpoch: number = 0;

  readonly tabs: string[] = ["History", "#Actions", "#Actions overview", "Lost epochs"];
  selectedTab: number = 0;

  loading: boolean = false;

  worker : Worker|undefined = undefined;

  updateFlipper: boolean = false;

  graphFontSize: number = 10;

  minislot: number = 0;

  @ViewChildren('nodeSetSlideToggle') toggles!: QueryList<any>;

  updateFilteredDataframe(){
    const ns = new Set(this._nodeSet);
    this._filtered_dataframe = this._dataframe.filter(x => ns.has(x.node_id));
  }

  updateFlip(){
    setTimeout(() => {
      this.updateFlipper = !this.updateFlipper;
    })
  }

  get dataframe(){
    const ns = new Set(this._nodeSet);
    return this._dataframe.filter(x => ns.has(x.node_id));
  }

  set dataframe(val){
    this._dataframe = val;

    const cmp = Array.from(new Set(this._dataframe.map(x => x.node_id))).sort((l,r) => l-r);
    if (cmp.some((x: number) => !this._nodeSet.includes(x))) {
      this._nodeSet = cmp;
      this.snackBar.open('The set of nodes was calculated from the log', 'Close', {
        duration: 2000,
      });
    }

    this.minEpoch = this._dataframe.map(x => x.epoch).reduce((a, b) => Math.min(a,b), this._dataframe[0].epoch);
    this.maxEpoch = this._dataframe.map(x => x.epoch).reduce((a, b) => Math.max(a,b), this._dataframe[0].epoch);

    this.epoch = this.minEpoch;

    this.toggles.forEach((x) => {
      x.checked = true;
    })

    this.updateFilteredDataframe();
  }

  emptyFilteredDataframe() {
    return this._filtered_dataframe.length === 0;
  }

  get epoch() {
    return this._epoch;
  }

  set epoch(val) {
    this._epoch = val;

    this.updateFlip();
  }

  add_to_nodeset(val: number){
    const tmp = this._nodeSet.concat([val])
                    .sort((l: number, r: number) => l-r);

    this._nodeSet = tmp;
  }

  remove_from_nodeset(val: number){
    this._nodeSet = this._nodeSet.filter((x) => x !== val);
  }

  toggleNode(a: any, node_id: number) {
    const ns = new Set(this._nodeSet);
    if (ns.has(node_id) === a['checked'])
      return;

    if (a['checked'] === true) {
      this.add_to_nodeset(node_id)
      this.updateFilteredDataframe();
    }

    if (a['checked'] === false) {
      this.remove_from_nodeset(node_id)
      this.updateFilteredDataframe();
    }
  }

  togglesInvert(){
    this.toggles.forEach((x) => {
      x.checked = !x.checked;
    })

    const cns = new Set(this._complete_nodeSet);
    const ns = new Set(this._nodeSet);

    this._complete_nodeSet.forEach((x) => {
      this.remove_from_nodeset(x);
    })

    Array.from(cns).filter((x: number) => !ns.has(x)).forEach((x: number) => {
      this.add_to_nodeset(x);
    })

    this.updateFilteredDataframe();
  }

  togglesDisable(){
    this.toggles.forEach((x) => {
      x.checked = false;
    });

    this._complete_nodeSet.forEach((x) => {
      this.remove_from_nodeset(x);
    })

    this.updateFilteredDataframe();
  }

  onFileSelected() {
    this.loading = true;
    this._nodeSet = [];

    const inputNode: any = document.querySelector('#file');
    const file: File = inputNode.files[0];

    this.worker?.terminate();
    this.worker = new Worker(new URL('./app.worker', import.meta.url));
    this.worker.addEventListener("message", (e) => {
      let tmpDataframe: SlotArrayProxy[] = e.data.map((x: Object) => SlotArrayProxy.fromObject(x));

      // Filter duplicate (epoch, node). Keep just the first
      let anyDuplicate = false;
      const epochsSeenByNodes: Map<number, Set<number>> = new Map();

      tmpDataframe = tmpDataframe.filter(x => {
        let epochsAlreadySeen = epochsSeenByNodes.get(x.node_id);

        if (epochsAlreadySeen === undefined)
          epochsAlreadySeen = new Set();

        if (epochsAlreadySeen.has(x.epoch)) {
          console.warn(`Duplicate for node_id ${x.node_id} and epoch ${x.epoch}`)
          anyDuplicate = true;
          return false;
        }

        epochsAlreadySeen = epochsAlreadySeen.add(x.epoch);
        epochsSeenByNodes.set(x.node_id, epochsAlreadySeen);
        return true;
      });

      if (anyDuplicate) {
        this.snackBar.open('Duplicate(s) (epoch, node id) found', 'Close', {duration: 2000});
      }

      if ((this._nodeSet.length == 0) && (this._complete_nodeSet.length == 0)) {
        this._complete_nodeSet = [...new Set(tmpDataframe.map(x => x.node_id))].sort((l: number, r: number) => l-r);
        this._nodeSet = this._complete_nodeSet;
      }

      this.dataframe = tmpDataframe;
      this.loading = false;
    });

    const reader = new FileReader();
    reader.onload = e => {
      this._nodeSet = [];
      this._complete_nodeSet = [];

      if ((file.type === 'application/gzip') || (file.type === 'application/x-gzip')) {
        this.snackBar.open('Gzip file was used. Assuming Cloves format', 'Close', {duration: 2000});

        // @ts-ignore
        const out = pako.ungzip(reader.result);

        untar(out.buffer)
          //.progress((extractedFile: any) => {})
          .then((extractedFiles: any) => {
            for (const f of extractedFiles) {
              if (f.name.endsWith('job.log')) {
                this.worker?.postMessage(f.buffer);
              } else if (f.name.endsWith('jobFile.json')) {
                const tmp = new TextDecoder()
                  .decode(f.buffer);

                this._test_info = JSON.parse(tmp);
                document.title = 'Inspector - Cloves ' + this._test_info['test_id'];

                this._complete_nodeSet = this._test_info['binaries']
                  .map((x: {'targets': number[]}) => x['targets'])
                  .reduce((w: number[], a: number[]) => [...w, ...a])
                  .sort((l: number, r: number) => l-r)
                  ;

                this._nodeSet = this._complete_nodeSet
              }
            }
          });
      } else {
        document.title = file.name;

        this.snackBar.open('Normal file was used. Assuming txt log format', 'Close', {duration: 2000});
        this.worker?.postMessage(reader.result);
      }
    };

    reader.readAsArrayBuffer(file);
  }

  constructor(private snackBar: MatSnackBar) {}
}
