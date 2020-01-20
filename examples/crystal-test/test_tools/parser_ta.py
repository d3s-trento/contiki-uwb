#!/usr/bin/env python3
import sys
import re
import argparse
from collections import OrderedDict

ap = argparse.ArgumentParser(description='Simulation log parser')
ap.add_argument('--input', required=False, default=("test.log",),
                   nargs="+",
                   help='File(s) to parse')
ap.add_argument('--format', required=False, default="unitn", 
                   help='File to parse')

args = ap.parse_args()

input_files = args.input
record_format = args.format

record_pattern = {
        "graz":"^(?P<date>[\d-]+) (?P<time>[\d:.-]+)\|%s$", # Graz
        "cooja":"^(?P<time>[\d:.]+)	ID:(?P<self_id>\d+)	%s$", # Cooja
        #"cooja":"^(?P<time>\d+):(?P<self_id>\d+):%s", # Cooja without GUI
        "fbk":"^%s\s+(?P<time>\d+)\s+[0-9-]+\s+[0-9:]+\s(?P<self_id>\d+)\s+\d+$", # FBK Motelab
        "indriya":"^%s\s+[0-9-]+\s+[0-9:]+\s40(?P<self_id>\d+)\s+(?P<time>\d+)\s+\d+$", # Indriya Motelab
        "twist":"^(?P<time>\d+[.]\d+)\s+(?P<self_id>\d+)\s+%s", # TWIST testbed
        "flocklab":"^(?P<time>\d+[.]\d+)[,][0-9]+[,](?P<self_id>\d+)[,][a-z][,]%s$", # FlockLab Testbed
        "unitn":"^\[(?P<time>.{23})\] INFO:evb1000\.\d+: (?P<self_id>\d+).evb1000 < b'%s'$"
        }.get(record_format, None)

fname_pattern = re.compile("log_(?P<self_id>\d+).txt")

if record_pattern is None:
    sys.stderr.write("Unknown record format: %s\n"%record_format)
    sys.exit(1)

# to microseconds
def convert_time(time):
    if record_format == "fbk":
        return int(time)/1000
    elif record_format == "indriya":
        return int(time)*1000
    elif record_format == "flocklab":
        return float(time)*1000000
    elif record_format == "twist":
        return float(time)*1000000
    elif record_format == "cooja":
        minutes,rest = time.split(":")
        seconds,millis = rest.split(".")
        return 1000*(int(millis)+int(seconds)*1000+int(minutes)*60*1000)
    elif record_format == "graz":
        return 0 # TODO
    elif record_format == "unitn":
        return 0 # TODO
    else:
        return time

S = re.compile(record_pattern%"S (?P<epoch>\d+):(?P<n_tx>\d+) (?P<n_acks>\d+) (?P<sync_acks>\d+):(?P<sync_missed>\d+) (?P<skew>-?\d+) (?P<hops>\d+)")
A = re.compile(record_pattern%"A (?P<epoch>\d+):(?P<seqn>\d+) (?P<acked>\d+) (?P<log_seqn>\d+)")
B = re.compile(record_pattern%"B (?P<epoch>\d+):(?P<source>\d+) (?P<seqn>\d+) (?P<log_seqn>\d+)")
P = re.compile(record_pattern%"P (?P<epoch>\d+):(?P<recvsrc_s>\d+) (?P<recvtype_s>\d+) (?P<recvlen_s>\d+):(?P<n_badtype_a>\d+) (?P<n_badlen_a>\d+) (?P<n_badcrc_a>\d+) (?P<ack_skew_err>-?\d+):(?P<end_s_time>-?\d+)")
R = re.compile(record_pattern%"R (?P<epoch>\d+):(?P<n_ta>\d+) (?P<n_rec_ta>\d+):(?P<noise>-?\d+) (?P<channel>\d+):(?P<s_tx_cnt>\d+) (?P<s_rx_cnt>\d+) (?P<cca_busy>\d+)")
T = re.compile(record_pattern%"T (?P<epoch>\d+):(?P<n_ta>\d+) (?P<status>\d+):(?P<src>\d+) (?P<seqn>\d+) (?P<type>\d+) (?P<length>\d+):(?P<t_rx_cnt>\d+) (?P<a_rx_cnt>\d+) (?P<acked>\d+) (?P<dw1000status_reg>[a-zA-Z0-9]+)")
E = re.compile(record_pattern%"E (?P<epoch>\d+):(?P<ontime>[\d\.]+):(?P<ton_s>\d+) (?P<ton_t>\d+) (?P<ton_a>\d+)")
F = re.compile(record_pattern%"F (?P<epoch>\d+):(?P<tf_s>\d+) (?P<tf_t>\d+) (?P<tf_a>\d+):(?P<n_short_s>\d+) (?P<n_short_t>\d+) (?P<n_short_a>\d+)")
#alive = re.compile(record_pattern%"I am alive! EUI-64: (?P<eui>[\d:abcdef]+)")
alive = re.compile(record_pattern%"I am alive! Node ID: (?P<ID>[\d]+)")




def parse():
    Slog = open("send_summary.log",'w')
    Plog = open("dbg.log",'w')
    Rlog = open("recv_summary.log",'w')
    Tlog = open("ta.log",'w')
    #Elog = open("energy.log",'w')
    #Flog = open("energy_tf.log",'w')
    Alog = open("app_send.log", 'w')
    Blog = open("sink_rcv.log", 'w')
    nodelog = open("node.log",'w')
    Slog.write("epoch\tsrc\tn_tx\tn_acks\tsync_acks\tsync_missed\tskew\thops\ttime\n")
    Plog.write("epoch\tnode\trecvsrc_s\trecvtype_s\trecvlen_s\tn_badtype_a\tn_badlen_a\tn_badcrc_a\tack_skew_err\tend_s_time\ttime\n")
    Rlog.write("epoch\tdst\tn_ta\tn_rec_ta\tnoise\tchannel\ts_tx_cnt\ts_rx_cnt\tcca_busy\ttime\n")
    Tlog.write("epoch\tn_ta\tsrc\tdst\tseqn\ttype\tlength\tstatus\tt_rx_cnt\ta_rx_cnt\tacked\tdw1000_status_reg\ttime\n")
    #Elog.write("epoch\tnode\tontime\tton_s\tton_t\tton_a\ttime\n")
    #Flog.write("epoch\tnode\ttf_s\ttf_t\ttf_a\tn_short_s\tn_short_t\tn_short_a\t\ttime\n")
    Alog.write("epoch\tsrc\tseqn\tacked\tlog_seqn\ttime\n")
    Blog.write("epoch\tsrc\tseqn\tlog_seqn\ttime\n")
    nodelog.write("s_id\tn_id\n")

    for fn in input_files:
        self_id = None
        with open(fn, 'r') as f:
            if (record_format == "graz"):
                m = fname_pattern.match(fn)
                if m is None:
                    print("Skipping unrecognized file name:", fn)
                    continue
                self_id = int(m.groupdict()["self_id"])
            
            old_msg = ""
            for l in f:
                if (record_format == "graz"):
                    tstamp, msg = l.split("|")
                    msg = msg.strip("\0\n\r\t ")
                    if not msg:
                        continue # skip empty log lines
                    if msg == old_msg:
                        continue # remove duplicated log lines
                    old_msg = msg
                    l = tstamp + "|" + msg
                m = T.match(l) 
                if m:
                        g = m.groupdict()
                        epoch = int(g["epoch"])
                        dst = self_id if self_id is not None else int(g["self_id"])
                        type_ = int(g["type"])
                        src = int(g["src"])
                        seqn = int(g["seqn"])
                        n_ta = int(g["n_ta"])
                        t_rx_cnt = int(g["t_rx_cnt"])
                        a_rx_cnt = int(g["a_rx_cnt"])
                        length = int(g["length"])
                        status = int(g["status"])
                        acked = int(g["acked"])
                        status_reg = str(g["dw1000status_reg"])
                        time = convert_time(g["time"])
                        
                        Tlog.write("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\t%d\n"%(epoch, n_ta, src, dst, seqn, type_, length, status, t_rx_cnt, a_rx_cnt, acked, status_reg, time))
                        continue
                m = A.match(l)
                if m:
                        g = m.groupdict()
                        epoch = int(g["epoch"])
                        dst = self_id if self_id is not None else int(g["self_id"])
                        log_seqn = int(g["log_seqn"])
                        time = convert_time(g["time"])
                        acked = int(g["acked"])
                        seqn = int(g["seqn"])
                        
                        Alog.write("%d\t%d\t%d\t%d\t%d\t%d\n"%(epoch, dst, seqn, acked, log_seqn, time))
                        continue
                m = B.match(l)
                if m:
                        g = m.groupdict()
                        epoch = int(g["epoch"])
                        source = int(g["source"])
                        seqn = int(g["seqn"])
                        log_seqn = int(g["log_seqn"])
                        time = convert_time(g["time"])
                        
                        Blog.write("%d\t%d\t%d\t%d\t%d\n"%(epoch, source, seqn, log_seqn, time))
                        continue
                m = S.match(l) 
                if m:
                        g = m.groupdict()
                        epoch = int(g["epoch"])
                        src = self_id if self_id is not None else int(g["self_id"])
                        n_tx = int(g["n_tx"])
                        n_acks = int(g["n_acks"])
                        sync_missed = int(g["sync_missed"])
                        sync_acks = int(g["sync_acks"])
                        skew = int(g["skew"])
                        hops = int(g["hops"])
                        time = convert_time(g["time"])
                        
                        Slog.write("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n"%(epoch, src, n_tx, n_acks, sync_acks, sync_missed, skew, hops, time))
                        continue
                m = P.match(l) 
                if m:
                        g = m.groupdict()
                        epoch = int(g["epoch"])
                        node = self_id if self_id is not None else int(g["self_id"])
                        recvsrc_s = int(g["recvsrc_s"])
                        recvtype_s = int(g["recvtype_s"])
                        recvlen_s = int(g["recvlen_s"])
                        n_badtype_a = int(g["n_badtype_a"])
                        n_badlen_a = int(g["n_badlen_a"])
                        n_badcrc_a = int(g["n_badcrc_a"])
                        ack_skew_err = int(g["ack_skew_err"])
                        end_s_time = int(g["end_s_time"])
                        time = convert_time(g["time"])
                        
                        Plog.write("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n"%(epoch,node,recvsrc_s,recvtype_s,recvlen_s,n_badtype_a,n_badlen_a,n_badcrc_a,ack_skew_err,end_s_time,time))
                        continue
                m = R.match(l) 
                if m:
                        g = m.groupdict()
                        epoch = int(g["epoch"])
                        dst = self_id if self_id is not None else int(g["self_id"])
                        n_ta = int(g["n_ta"])
                        n_rec_ta = int(g["n_rec_ta"])
                        noise = int(g["noise"])
                        channel = int(g["channel"])
                        s_tx_cnt = int(g["s_tx_cnt"])
                        s_rx_cnt = int(g["s_rx_cnt"])
                        cca_busy = int(g["cca_busy"])
                        time = convert_time(g["time"])
                        
                        Rlog.write("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n"%(epoch, dst, n_ta, n_rec_ta, noise, channel, s_tx_cnt, s_rx_cnt, cca_busy, time))
                        continue
                #m = E.match(l) 
                #if m:
                #        g = m.groupdict()
                #        epoch = int(g["epoch"])
                #        node = self_id if self_id is not None else int(g["self_id"])
                #        ton_s = int(g["ton_s"])
                #        ton_t = int(g["ton_t"])
                #        ton_a = int(g["ton_a"])
                #        ontime = float(g["ontime"])
                #        time = convert_time(g["time"])
                #        
                #        Elog.write("%d\t%d\t%f\t%d\t%d\t%d\t%d\n"%(epoch, node, ontime, ton_s, ton_t, ton_a, time))
                #        continue
                #m = F.match(l) 
                #if m:
                #        g = m.groupdict()
                #        epoch = int(g["epoch"])
                #        node = self_id if self_id is not None else int(g["self_id"])
                #        tf_s = int(g["tf_s"])
                #        tf_t = int(g["tf_t"])
                #        tf_a = int(g["tf_a"])
                #        n_short_s = int(g["n_short_s"])
                #        n_short_t = int(g["n_short_t"])
                #        n_short_a = int(g["n_short_a"])
                #        time = convert_time(g["time"])
                #        
                #        Flog.write("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n"%(epoch, node, tf_s, tf_t, tf_a, n_short_s, n_short_t, n_short_a, time))
                #        continue
                m = alive.match(l) 
                if m:
                        g = m.groupdict()
                        src = self_id if self_id is not None else int(g["self_id"])
                        time = convert_time(g["time"])
                        
                        nodelog.write("%d\t%d\n"%(src, time))
                        continue



parse()

# -*- vim: sw=4 ts=4 expandtab
