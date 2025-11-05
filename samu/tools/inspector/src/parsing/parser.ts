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
import { TsmSlotStatus} from './tsmStatus';

export class TsmSlot {}

function _getBits(dv: DataView, offset: number, len: number) {
  if (len == 0)
    throw new Error("Can't read 0 bits");

  if (len >= Math.floor(Math.log2(Number.MAX_SAFE_INTEGER)))
    throw new Error("Too many bits requested");

  let v: number[] = [];

  for (let i=0; i<Math.ceil(len/8); ++i) {
    try {
    const bef = dv.getUint8(Math.floor(offset/8) + i);
    let after = 0;

    const bitOffset = offset - Math.floor(offset/8)*8;

      if (bitOffset + len - i*8 > 8)
        after = dv.getUint8(Math.floor(offset/8 + i + 1));

    const res = ((bef << bitOffset) & 255) | ((after >> (8 - bitOffset)) & 255);

    v.push(res);
    } catch (error) {
      console.log("AA");
    }
  }

  const revBitOffset = (Math.ceil(len/8)*8 - len);

  const lenn = v.length;
  const res = v.map((v,i) => v * Math.pow(256, lenn-1-i)).reduce((x,y) => x+y, 0);

  return (res >> revBitOffset);
}

function _getUintLe(dv: DataView, offset: number, len: number) {
  return _getBits(dv, offset, len);
}

function _getIntLe(dv: DataView, offset: number, len: number) {
  if (len != 16) {
    throw new Error("Unsupported");
  }

  const res = _getBits(dv, offset, len);
  if (res & Math.pow(2, 15))  {
    return - (Math.pow(2, 16) - res);
  } else {
    return res;
  }

}

class BitView {
  readonly dv: DataView;
  bitOffset: number;

  getUintLe(len: number) {
    const res = _getUintLe(this.dv, this.bitOffset, len);
    this.bitOffset = this.bitOffset + len

    return res;
  }

  getIntLe(len: number) {
    const res = _getIntLe(this.dv, this.bitOffset, len);
    this.bitOffset = this.bitOffset + len

    return res;
  }

  public constructor(dv: DataView, initialBitOffset: number) {
    this.dv = dv;
    this.bitOffset = initialBitOffset;
  }
}

enum HdrMasks {
  MASK_MINISLOTS          =  1,
  MASK_MINISLOTS_IDX_DIFF = (1<<1),
  MASK_MINISLOTS_TO_USE   = (1<<2),
  MASK_PROGRESS_MINISLOTS = (1<<3)
};


function dv_to_incomplete_tmp_slots(input: DataView, max_nslots: number): any[] {
    const bv = new BitView(input, 5*8);

    const slots: any[] = [];
    for (let i = 0; i<max_nslots; ++i) {
      const obj: any = {
        '_m_hdr': bv.getUintLe(1) != 0,
        'status': bv.getUintLe(4),
      };

      if (obj['_m_hdr']) {
        obj['_hdr'] = bv.getUintLe(8)
      } else {
        obj['_hdr'] = 0;
      }

      if (obj['_hdr'] & HdrMasks.MASK_MINISLOTS) {
        obj['minislot'] = bv.getUintLe(24);
      }

      if (obj['_hdr'] & HdrMasks.MASK_MINISLOTS_IDX_DIFF) {
        obj['minislot_idx_diff'] = bv.getIntLe(16);

        if ((obj['status'] === TsmSlotStatus.RX_WITH_SYNCH) || (obj['status'] === TsmSlotStatus.SCAN_WITH_SYNCH)) {
          console.warn("Minislots idx diff should not be present when synchronizing. Check the logging code. Forcing value to 0")
          obj['minislot_idx_diff'] = 0;
        }
      } else {
        obj['minislot_idx_diff'] = 0;
      }

      if (obj['_hdr'] & HdrMasks.MASK_MINISLOTS_TO_USE) {
        obj['minislots_to_use'] = bv.getUintLe(8)
      }

      if (obj['_hdr'] & HdrMasks.MASK_PROGRESS_MINISLOTS) {
        obj['progress_minislots'] = bv.getUintLe(16)
      }

      slots[i] = obj;
    }

    return slots
}

class TmpSlot {
  status: TsmSlotStatus;
  minislot: number|undefined;
  minislot_idx_diff: number|undefined;
  minislots_to_use: number;
  progress_minislots: number;

  public constructor(status: TsmSlotStatus,
                     minislot: number,
                     minislot_idx_diff: number,
                     minislots_to_use: number,
                     progress_minislots: number) {
    this.status = status;
    this.minislot = minislot;
    this.minislot_idx_diff = minislot_idx_diff;
    this.minislots_to_use = minislots_to_use;
    this.progress_minislots = progress_minislots;
  }
}

function incomplete_tmp_slots_to_complete_slots(input: any[], default_minislots_to_use: number): TmpSlot[] {
    return input.map(x => {
      if (x['progress_minislots'] === undefined) {
        x['progress_minislots'] = default_minislots_to_use;
      }

      if (x['minislots_to_use'] === undefined) {
        x['minislots_to_use'] = default_minislots_to_use;
      }

      return new TmpSlot(x['status'], x['minislot'], x['minislot_idx_diff'], x['minislots_to_use'], x['progress_minislots'])
    })
}

class PlottableTsmSlot {
  start_minislot: number;
  duration: number;
  status: TsmSlotStatus;

  extra: any

  public constructor(start_minislot: number, duration: number, status: TsmSlotStatus) {
    this.start_minislot = start_minislot;
    this.duration = duration;
    this.status = status;
  }
}

export class PartialSlotArray {
  readonly default_minislots_to_use: number;
  readonly max_nslots: number;
  readonly nslots: number;
  readonly _slots: DataView;

  get slots() {
    if ((this.default_minislots_to_use < 0) ||
        (this.max_nslots < 0) ||
        (this.nslots < 0) ||
        (this._slots.byteLength == 0)
    ) {
      return [new PlottableTsmSlot(0, 0.15, TsmSlotStatus.PARSING_ERROR)];
    }

    const slots = incomplete_tmp_slots_to_complete_slots(
      dv_to_incomplete_tmp_slots(this._slots, this.max_nslots),
      this.default_minislots_to_use
    );

    const tmp = slots.map((v,i): [TmpSlot, number] => [v,i]).filter((v,j,ar) => (v[0]).minislot !== undefined);

    let first_minislot: [number, number];

    if (tmp.length == 0) {
      first_minislot = [0,0]
    } else {
      // @ts-ignore
      first_minislot = [tmp[0][1], tmp[0][0].minislot - tmp[0][0].minislots_to_use + 1];
    }

    let offset_minislots = slots.slice(0, first_minislot[0]).map(x => x.progress_minislots);

    const cumulativeSum = ((sum:number) => (value:number) => sum+=value)(0);
    offset_minislots = offset_minislots.reverse().map(cumulativeSum).reverse().map(x => first_minislot[1] - x);

    let prev_minislot: number|undefined = ((first_minislot[0] == 0) && (first_minislot[1] == 0))?0:undefined;

    const finalSlots = slots.map((x,i) => {
      const tt: any = {};

      if (x.minislot !== undefined) {
        tt['start_minislot'] = x.minislot - x.minislots_to_use + 1
        prev_minislot = x.minislot + 1;
      } else if (prev_minislot !== undefined) {
        tt['start_minislot'] = prev_minislot + x.progress_minislots - x.minislots_to_use
        prev_minislot = tt['start_minislot'] + x.minislots_to_use
      } else {
        tt['start_minislot'] = offset_minislots[i];
      }

      tt['duration'] = x.minislots_to_use;

      const res = new PlottableTsmSlot(tt['start_minislot'], tt['duration'], x.status);
      res.extra = x;

      return res;
    });

    if ((finalSlots.length > 0) && (this.max_nslots != this.nslots)) {
      const lastSlot = finalSlots[finalSlots.length -1];

      finalSlots.push(
        new PlottableTsmSlot(lastSlot['start_minislot'] + lastSlot['duration'] - 1 + 1, 0.15, TsmSlotStatus.END)
      );
    }

    return finalSlots;
  }

  constructor(default_minislots_to_use: number, max_nslots: number, nslots: number, slots: DataView) {
    this.default_minislots_to_use = default_minislots_to_use;
    this.max_nslots = max_nslots;
    this.nslots = nslots;
    this._slots = slots;
  }
}

export class SlotArrayProxy {
  readonly node_id: number;
  epoch: number;
  repetition: number;
  private data: ArrayBuffer | PartialSlotArray;


  get default_minislots_to_use(): number {
    if (this.data instanceof ArrayBuffer) {
      const dv = new DataView(this.data)
      this.data = new PartialSlotArray(dv.getUint8(0), dv.getUint16(1, false), dv.getUint16(3, false), dv);

      return (this.data as PartialSlotArray).default_minislots_to_use;
    }

    return (this.data as PartialSlotArray).default_minislots_to_use;
  }

  get max_nslots() {
    if (this.data instanceof ArrayBuffer) {
      const dv = new DataView(this.data)
      this.data = new PartialSlotArray(dv.getUint8(0), dv.getUint16(1, false), dv.getUint16(3, false), dv);

      return (this.data as PartialSlotArray).max_nslots;
    }

    return (this.data as PartialSlotArray).max_nslots;
  }

  get nslots() {
    if (this.data instanceof ArrayBuffer) {
      const dv = new DataView(this.data)
      this.data = new PartialSlotArray(dv.getUint8(0), dv.getUint16(1, false), dv.getUint16(3, false), dv);

      return (this.data as PartialSlotArray).nslots;
    }

    return (this.data as PartialSlotArray).nslots;
  }

  get slots() {
    if (this.data instanceof ArrayBuffer) {
      const dv = new DataView(this.data)
      this.data = new PartialSlotArray(dv.getUint8(0), dv.getUint16(1, false), dv.getUint16(3, false), dv);

      return (this.data as PartialSlotArray).slots;
    }

    return (this.data as PartialSlotArray).slots;
  }

  private constructor(node_id: number, epoch: number, data: ArrayBuffer) {
    this.node_id = node_id;
    this.epoch = epoch;
    this.data = data;

    this.repetition = 0; // TODO: Don't like it
  }

  static fromArrayBuffer(node_id: number, epoch: number, data: ArrayBuffer): SlotArrayProxy {
    return new SlotArrayProxy(node_id, epoch, data);
  }

  static fromObject(obj: Object): SlotArrayProxy {
    // @ts-ignore
    if (!(obj['data'] instanceof ArrayBuffer)) {
      throw new Error("Data field is not an ArrayBuffer")
    }

    // @ts-ignore
    const res = new SlotArrayProxy(obj['node_id'], obj['epoch'], obj['data']);
    // @ts-ignore
    res.repetition = obj['repetition']

    return res

  }
}
