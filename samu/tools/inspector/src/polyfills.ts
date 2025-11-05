
import {Buffer} from 'buffer';

declare var require: any;
declare var global: any;
declare var window: any;

(window as any).global = window;
// @ts-ignore
window.Buffer = window.Buffer || Buffer;
// declare type BigInt = number;
