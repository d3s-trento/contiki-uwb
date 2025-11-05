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
/// <reference lib="webworker" />

import {SlotArrayProxy} from "../parsing/parser"
import * as ascii85 from "../parsing/ascii85"

addEventListener('message', ({ data }) => {
  const e = data;

  function notEmpty<TValue>(value: TValue | null | undefined): value is TValue {
      return value !== null && value !== undefined;
  }

  const re = /^\[[0-9 :\-,]+\] INFO:[a-zA-Z0-9]+\.(?<node_id>[0-9]+): [0-9]+\.[a-zA-Z0-9]+ < b(?<sep>[\'"])\[samul (?<epoch>[0-9]+)\]Slots: (?<slots>.*)[\'"]$/;
  const tmp = new TextDecoder()
    .decode(e)
    .split('\n')
    .filter(s => s.includes('[samul'))
    .map(s => {
      const r = s.match(re);
      if (r === null) {
        console.warn("x");
        return null;
      } else if (r.groups === undefined) {
        console.warn("y");
        return null;
      } else {
        return r.groups;
      }
    })
    .filter(notEmpty)
    .map( (s: {[key: string]: string; }) => {
      const unescape_string = eval(s['sep'] + s['slots'] + s['sep']);
      let buffer;
      try {
        buffer = ascii85.decode(unescape_string).buffer;
      } catch (error) {
        buffer = new ArrayBuffer(0);
      }

      return SlotArrayProxy.fromArrayBuffer(+s['node_id'], +s['epoch'], buffer);
    });

    const lastEpochMap = new Map<number, [number, number]>();

    for (const t of tmp) {
      const [lastSeenEpoch, repetition] = lastEpochMap.get(t.node_id) ?? [-1, 0];

      if (t.epoch < lastSeenEpoch) {
        console.warn(`Seems that epochs went back in time for node ${t.node_id} (${t.epoch} < ${lastSeenEpoch})`);
      }

      if (t.epoch == lastSeenEpoch) {
        t.epoch = lastSeenEpoch + repetition;
        lastEpochMap.set(t.node_id, [lastSeenEpoch, repetition+1]);
        t.repetition = repetition;
      } else {
        lastEpochMap.set(t.node_id, [t.epoch, 1]);
        t.repetition = 0;
      }

    }

    postMessage(tmp);

    // // @ts-ignore
    // const ds = new DecompressionStream("gzip");
    // const decompressedStream = file.stream().pipeThrough(ds);

    // const stream = new WritableStream({
    //   write(chunk){console.log(chunk);},
    //   close(){console.log("close");},
    //   abort(err){console.log("err")}
    // })

    // decompressedStream.pipeTo(stream);
});

