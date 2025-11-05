/*
  * Code from https://github.com/huandu/node-ascii85
  */
/**
 * Copyright 2015 Huan Du. All rights reserved.
 * Licensed under the MIT license that can be found in the LICENSE file.
 */
'use strict';

import {Buffer} from "buffer";

// Buffer api is changed since v6.0.0. this wrapper is designed to leverage new
// api features if possible without breaking old node.
//
// it's not a completed polyfill implementation. i just do necessary shims for this
// package only.
var _BufferFrom = Buffer.from || function() {
  switch (arguments.length) {
    case 1: return new Buffer(arguments[0]);
    case 2: return new Buffer(arguments[0], arguments[1]);
    //case 3: return new Buffer(arguments[0], arguments[1], arguments[2]);
    //default: throw new Exception('unexpected call.');
  }

  throw "a";
};
var _BufferAlloc = Buffer.alloc || function(size, fill, encoding) {
  var buf = new Buffer(size);

  if (fill !== undefined) {
    if (typeof encoding === "string") {
      // @ts-ignore
      buf.fill(fill, encoding);
    } else {
      buf.fill(fill);
    }
  }

  return buf;
};
var _BufferAllocUnsafe = Buffer.allocUnsafe || function(size) {
  return new Buffer(size);
};
var _NewUint8Array = (function() {
  if (typeof Uint8Array === 'undefined') {
    return function(size: number) {
      return new Array(size);
    };
  } else {
    return function(size: number) {
      return new Uint8Array(size);
    }
  }
})();

var ASCII85_BASE = 85;
var ASCII85_CODE_START = 33;
var ASCII85_CODE_END = ASCII85_CODE_START + ASCII85_BASE;
var ASCII85_NULL = String.fromCharCode(0);
var ASCII85_NULL_STRING = ASCII85_NULL + ASCII85_NULL + ASCII85_NULL + ASCII85_NULL;
var ASCII85_ZERO = 'z';
var ASCII85_ZERO_VALUE = ASCII85_ZERO.charCodeAt(0);
var ASCII85_PADDING_VALUE = 'u'.charCodeAt(0);
var ASCII85_ENCODING_GROUP_LENGTH = 4;
var ASCII85_DECODING_GROUP_LENGTH = 5;
var ASCII85_BLOCK_START = '<~';
var ASCII85_BLOCK_START_LENGTH = ASCII85_BLOCK_START.length;
var ASCII85_BLOCK_START_VALUE = _BufferFrom(ASCII85_BLOCK_START).readUInt16BE(0);
var ASCII85_BLOCK_END = '~>';
var ASCII85_BLOCK_END_LENGTH = ASCII85_BLOCK_END.length;
var ASCII85_BLOCK_END_VALUE = _BufferFrom(ASCII85_BLOCK_END).readUInt16BE(0);
var ASCII85_GROUP_SPACE = 'y';
var ASCII85_GROUP_SPACE_VALUE = ASCII85_GROUP_SPACE.charCodeAt(0);
var ASCII85_GROUP_SPACE_CODE = 0x20202020;
var ASCII85_GROUP_SPACE_STRING = '    ';

var ASCII85_DEFAULT_ENCODING_TABLE = (function() {
  var arr = new Array(ASCII85_BASE);
  var i;

  for (i = 0; i < ASCII85_BASE; i++) {
    arr[i] = String.fromCharCode(ASCII85_CODE_START + i);
  }

  return arr;
})();

const ASCII85_DEFAULT_DECODING_TABLE: Array<number> = (function() {
  var arr = new Array(1 << 8);
  var i: number;

  for (i = 0; i < ASCII85_BASE; i++) {
    arr[ASCII85_CODE_START + i] = i;
  }

  return arr;
})();

/**
 * Decode a string to binary data.
 * @param {String|Buffer} data is a string or Buffer.
 * @param {Array|Object} [table] is a sparse array to map char code and decoded value for decoding.
 *                               Default is standard table.
 */
export const decode = function(str: any) {
  var buf = str;
  var enableZero = true;
  var enableGroupSpace = true;
  var output, offset, digits, cur, i, c, t, len, padding;

  const table = ASCII85_DEFAULT_DECODING_TABLE;

  // // convert a key/value format char map to code array.
  // if (!Array.isArray(table)) {
  //   table = table.table || table;

  //   if (!Array.isArray(table)) {
  //     t = [];
  //     Object.keys(table).forEach(function(v) {
  //       t[v.charCodeAt(0)] = table[v];
  //     });
  //     table = t;
  //   }
  // }

  enableZero = !table[ASCII85_ZERO_VALUE];
  enableGroupSpace = !table[ASCII85_GROUP_SPACE_VALUE];

  if (!(buf instanceof Buffer)) {
    buf = _BufferFrom(buf);
  }

  // estimate output length and alloc buffer for it.
  t = 0;

  if (enableZero || enableGroupSpace) {
    for (i = 0, len = buf.length; i < len; i++) {
      c = buf.readUInt8(i);

      if (enableZero && c === ASCII85_ZERO_VALUE) {
        t++;
      }

      if (enableGroupSpace && c === ASCII85_GROUP_SPACE_VALUE) {
        t++;
      }
    }
  }

  offset = 0;
  len = Math.ceil(buf.length * ASCII85_ENCODING_GROUP_LENGTH / ASCII85_DECODING_GROUP_LENGTH) +
        t * ASCII85_ENCODING_GROUP_LENGTH +
        ASCII85_DECODING_GROUP_LENGTH;
  output = _BufferAllocUnsafe(len);

  // if str starts with delimiter ('<~'), it must end with '~>'.
  if (buf.length >= ASCII85_BLOCK_START_LENGTH + ASCII85_BLOCK_END_LENGTH && buf.readUInt16BE(0) === ASCII85_BLOCK_START_VALUE) {
    for (i = buf.length - ASCII85_BLOCK_END_LENGTH; i > ASCII85_BLOCK_START_LENGTH; i--) {
      if (buf.readUInt16BE(i) === ASCII85_BLOCK_END_VALUE) {
        break;
      }
    }

    if (i <= ASCII85_BLOCK_START_LENGTH) {
      throw new Error('Invalid ascii85 string delimiter pair.');
    }

    buf = buf.slice(ASCII85_BLOCK_START_LENGTH, i);
  }

  for (i = digits = cur = 0, len = buf.length; i < len; i++) {
    c = buf.readUInt8(i);

    if (enableZero && c === ASCII85_ZERO_VALUE) {
      offset += output.write(ASCII85_NULL_STRING, offset);
      continue;
    }

    if (enableGroupSpace && c === ASCII85_GROUP_SPACE_VALUE) {
      offset += output.write(ASCII85_GROUP_SPACE_STRING, offset);
      continue;
    }

    if (table[c] === undefined) {
      continue;
    }

    cur *= ASCII85_BASE;
    cur += table[c];
    digits++;

    if (digits % ASCII85_DECODING_GROUP_LENGTH) {
      continue;
    }

    offset = output.writeUInt32BE(cur, offset);
    cur = 0;
    digits = 0;
  }

  if (digits) {
    padding = ASCII85_DECODING_GROUP_LENGTH - digits;

    for (i = 0; i < padding; i++) {
      cur *= ASCII85_BASE;
      cur += ASCII85_BASE - 1;
    }

    for (i = 3, len = padding - 1; i > len; i--) {
      offset = output.writeUInt8((cur >>> (i * 8)) & 0xFF, offset);
    }
  }

  return output.slice(0, offset);
};
