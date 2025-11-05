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
export enum TsmSlotStatus {
  RX_SUCCESS                 =  0,
  RX_TIMEOUT                 =  1,
  RX_ERROR                   =  2,
  RX_MALFORMED               =  3,
  TIMER_EVENT                =  4,
  TX_DONE                    =  5,
  FS_EMPTY                   =  6,
  FS_DETECTED                =  7,
  FS_DETECTED_AND_PROPAGATED =  8,
  FS_ERROR                   =  9,
  RX_WITH_SYNCH              = 10,
  SCAN_WITH_SYNCH            = 11,
  UNKNOWN                    = 12,

  // Special values
  PARSING_ERROR              = 252,
  PROCESSING_ERROR           = 253,
  EPOCH_EXTRAPOLATED         = 254,
  END                        = 255,
}

export namespace TsmSlotStatus {
  export function status_to_color(status: TsmSlotStatus) {
    switch(status) {
        case TsmSlotStatus.RX_SUCCESS: return "#2CA02C";
        case TsmSlotStatus.RX_TIMEOUT: return "#C0C0C0";
        case TsmSlotStatus.RX_ERROR: return "#D62627";

        case TsmSlotStatus.RX_MALFORMED: return "#FF6347";
        case TsmSlotStatus.TIMER_EVENT: return "#708090"; // slategray
        case TsmSlotStatus.TX_DONE: return "#3B7CED";
        case TsmSlotStatus.FS_EMPTY: return "#8A2BE2"; // blueviolet
        case TsmSlotStatus.FS_DETECTED: return "#7FFFD4"; // aquamarine
        case TsmSlotStatus.FS_DETECTED_AND_PROPAGATED: return "#00FFFF";
        case TsmSlotStatus.FS_ERROR: return "#8B4513";

        case TsmSlotStatus.RX_WITH_SYNCH: return "#2CA02C"; // "#FFD700"; //
        case TsmSlotStatus.SCAN_WITH_SYNCH: return "#2CA02C"; // "#FFD700"; // #2CA02C
        case TsmSlotStatus.PARSING_ERROR: return "#ffffff";
        case TsmSlotStatus.PROCESSING_ERROR: return "#ffffff";
        case TsmSlotStatus.EPOCH_EXTRAPOLATED: return "#000000";
        case TsmSlotStatus.END: return "#000000";

        default: case TsmSlotStatus.UNKNOWN: return "#000000";

    }
  }

  export function status_to_short_name(status: TsmSlotStatus) : string {
    switch (status) {
        case TsmSlotStatus.RX_SUCCESS: return "R";
        case TsmSlotStatus.RX_TIMEOUT: return "L";
        case TsmSlotStatus.RX_ERROR: return "E";
        case TsmSlotStatus.RX_MALFORMED: return "B";
        case TsmSlotStatus.TIMER_EVENT: return "U";
        case TsmSlotStatus.TX_DONE: return"T";
        case TsmSlotStatus.FS_EMPTY: return "~";
        case TsmSlotStatus.FS_DETECTED: return "D";
        case TsmSlotStatus.FS_DETECTED_AND_PROPAGATED: return "!";
        case TsmSlotStatus.FS_ERROR: return "X";
        case TsmSlotStatus.RX_WITH_SYNCH: return "RS";
        case TsmSlotStatus.SCAN_WITH_SYNCH: return "Rs";
        case TsmSlotStatus.PARSING_ERROR: return "A Parsing error happened";
        case TsmSlotStatus.PROCESSING_ERROR: return "A slot processing error happened";
        case TsmSlotStatus.EPOCH_EXTRAPOLATED: return "(SHOULD NOT BE VISIBLE) Epoch extrapolated status";
        case TsmSlotStatus.END: return "|";

        default: case TsmSlotStatus.UNKNOWN : return "?";
    }
  }

  export function status_to_pretty_name(status: TsmSlotStatus) : string {
    switch (status) {
        case TsmSlotStatus.RX_SUCCESS: return "Successful reception";
        case TsmSlotStatus.RX_TIMEOUT: return "Timed out reception";
        case TsmSlotStatus.RX_ERROR: return "Reception error";
        case TsmSlotStatus.RX_MALFORMED: return "Malformed reception";
        case TsmSlotStatus.TIMER_EVENT: return "Timer event";
        case TsmSlotStatus.TX_DONE: return "Transmission";
        case TsmSlotStatus.FS_EMPTY: return "Empty Flick";
        case TsmSlotStatus.FS_DETECTED: return "Flick detection (complete frame)";
        case TsmSlotStatus.FS_DETECTED_AND_PROPAGATED: return "Flick detection";
        case TsmSlotStatus.FS_ERROR: return "Flick error";
        case TsmSlotStatus.RX_WITH_SYNCH: return "Reception with synchronization";
        case TsmSlotStatus.SCAN_WITH_SYNCH: return "Scan with synchronization";
        case TsmSlotStatus.PARSING_ERROR: return "A Parsing error happened";
        case TsmSlotStatus.PROCESSING_ERROR: return "A slot processing error happened";
        case TsmSlotStatus.EPOCH_EXTRAPOLATED: return "(SHOULD NOT BE VISIBLE) Epoch extrapolated status";
        case TsmSlotStatus.END: return "No more slot space";

        default: case TsmSlotStatus.UNKNOWN: return "Unknown type";
    }
  }
}
