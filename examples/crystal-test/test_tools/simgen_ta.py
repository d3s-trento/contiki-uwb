#!/usr/bin/env python3
import sys
import argparse
import traceback
import os
from collections import namedtuple, OrderedDict
from shutil import copy, rmtree
import traceback
import subprocess
import itertools
import re
import random

import datetime
import json
from   jinja2 import Template


run_path = os.path.dirname(sys.argv[0])
crystal_path = os.path.join(run_path, "..")

print(run_path)
print(crystal_path)



ap = argparse.ArgumentParser(description='Simulation generator')
ap.add_argument('--basepath', required=False, default=crystal_path,
                   help='Base path')
ap.add_argument('-c', '--config', required=False, default="params.py",
                   help='Configuration python file')

args = ap.parse_args()


basepath = args.basepath
print("Base path:", basepath)
apppath = basepath #os.path.join(basepath, "crystal-test")
sys.path += [".", os.path.join(apppath,"test_tools")]
params = args.config

def rnd_unique(nodes, n):
    """@deprecated in favor of random.sample"""
    l = 0
    r = []
    while (l<n):
        x = random.choice(nodes)
        if x not in r:
            r.append(x)
            l += 1
    return r

def generate_table_array(nodes, num_epochs, concurrent_txs):
    tbl = []
    unique_nodes = set(nodes)
    if seed is not None:
        random_tbl = random.Random(seed)
    else:
        random_tbl = random.Random()
    for _ in range(num_epochs):
        tbl += random_tbl.sample(unique_nodes, concurrent_txs)
    return "static const uint8_t sndtbl[] = {%s};"%",".join([str(x) for x in tbl])



def prepare_binary(simdir, binary_name, nodes, num_epochs, concurrent_txs, new_env):
    env = os.environ.copy()
    env.update(new_env)

    abs_bname = os.path.join(apppath, binary_name)
    abs_ihex_name = abs_bname + ".ihex"
    abs_tbl_name = os.path.join(apppath, "sndtbl.c")
    abs_env_name = abs_bname + ".env"

    print(abs_tbl_name)
    with open(abs_tbl_name, "w") as f:
        f.write(generate_table_array(nodes, num_epochs, concurrent_txs))
    print("Written table file at")
    print(abs_tbl_name)

    pwd = os.getcwd()
    os.chdir(apppath)
    subprocess.check_call(["sh","-c","./build_simgen.sh"], env=env)
    os.chdir(pwd)

    try:
        os.makedirs(simdir)
    except OSError as e:
        print(e)

    nodelist = os.path.join(simdir, "nodelist.txt")
    with open(nodelist, "w") as f:
        for n in nodes:
            f.write("%d\n"%n)

    copy(abs_bname, simdir)
    if os.path.isfile(abs_ihex_name):
        copy(abs_ihex_name, simdir)
    copy(abs_env_name, simdir)


version_map = {"txo":"GLOSSY_TX_ONLY_VERSION", "std":"GLOSSY_STANDARD_VERSION"}


def mk_env(power, radio_cfg, sink, num_senders, n_empty, glossy_version):
    cflags = [
    "-DDW1000_CONF_SMART_TX_POWER=%d"%power.smart,
    "-DDW1000_CONF_TX_POWER=0x%06x" % power.power,
    "-DAPP_RADIO_CONF=%d"%radio_cfg,
    "-DSINK_ID=%d"%sink,
    "-DSTART_EPOCH=%d"%start_epoch,
    "-DCONCURRENT_TXS=%d"%num_senders,
    "-DNUM_ACTIVE_EPOCHS=%d"%active_epochs,
    "-DPAYLOAD_LENGTH=%d"%payload,
    "-DCRYSTAL_CONF_PERIOD_MS=%d"%(int(period*1000)),
    "-DCRYSTAL_CONF_NTX_S=%d"%n_tx_s,
    "-DCRYSTAL_CONF_NTX_T=%d"%n_tx_t,
    "-DCRYSTAL_CONF_NTX_A=%d"%n_tx_a,
    "-DCRYSTAL_CONF_DUR_S_MS=%.2f"%dur_s,
    "-DCRYSTAL_CONF_DUR_T_MS=%.2f"%dur_t,
    "-DCRYSTAL_CONF_DUR_A_MS=%.2f"%dur_a,
    "-DCRYSTAL_CONF_SYNC_ACKS=%d"%sync_ack,
    "-DCRYSTAL_CONF_SINK_MAX_EMPTY_TS=%d"%n_empty.r,
    "-DCRYSTAL_CONF_MAX_SILENT_TAS=%d"%n_empty.y,
    "-DCRYSTAL_CONF_MAX_MISSING_ACKS=%d"%n_empty.z,
    # "-DCRYSTAL_CONF_SINK_MAX_NOISY_TS=%d"%n_empty.x,
    "-DCRYSTAL_CONF_SINK_MAX_RCP_ERRORS_TS=%d"%n_empty.x,
    "-DCRYSTAL_CONF_DYNAMIC_NEMPTY=%d"%dyn_nempty,
    "-DCRYSTAL_CONF_CHHOP_MAPPING=CHMAP_%s"%chmap,
    "-DCRYSTAL_CONF_BSTRAP_CHHOPPING=BSTRAP_%s"%boot_chop,
    "-DCRYSTAL_CONF_N_FULL_EPOCHS=%d"%full_epochs,
    "-DGLOSSY_VERSION_CONF=%s"%version_map[glossy_version],
    ]

    if logging:
        cflags += ["-DCRYSTAL_CONF_LOGLEVEL=CRYSTAL_LOGS_ALL"]
        cflags += ["-DCRYSTAL_CONF_TIME_FOR_APP=\(RTIMER_SECOND/5\)"]
        #cflags += ["-DCRYSTAL_CONF_LOGLEVEL=CRYSTAL_LOGS_EPOCH_STATS"]
        #cflags += ["-DCRYSTAL_CONF_TIME_FOR_APP=(RTIMER_SECOND/10)"]

    cflags += ["-DSTART_DELAY_SINK=20", "-DSTART_DELAY_NONSINK=10"]

    cflags = " ".join(cflags)
    new_env = {"CFLAGS":cflags}
    return new_env


glb = {}
pars = {}
sys.path.insert(0, ".")
import params
PARAMS = {x:params.__dict__[x] for x in params.__dict__.keys() if not x.startswith("__")}

def set_defaults(dst, src):
    for k,v in src.items():
        if k not in dst:
            dst[k] = v

NemptyTuple = namedtuple("NemptyTuple", "r y z x")
PowerTuple = namedtuple("PowerTuple", "smart power")

defaults = {
    "period":2,
    "sync_ack":1,
    "dyn_nempty":0,
    #"n_emptys":[(2, 2, 4, 0)],
    "payload":2,
    #"chmap":"nohop",
    #"boot_chop":"nohop",
    "logging":True,
    "seed":None,
    }

set_defaults(PARAMS, defaults)

print("Using the following params")
print(PARAMS)

globals().update(PARAMS)


print("--- Preparing simulations ------------------------")

# GOAL: Generate "crystal_test.json" to be used in the testbed automatically
from simgen_templates import TESTBED_TEMPLATE

if not duration:
    duration = 600
if not ts_init:
    ts_init = "asap"

TESTBED_INIT_TIME_FORMAT = "%Y-%m-%d %H:%M"
SIMULATION_OFFSET = 60

testbed_template  = Template(TESTBED_TEMPLATE)
SIM_DURATION = duration
START_TIME  =  ts_init
sim_start_time = None
try:
    sim_start_time = datetime.datetime.strptime(START_TIME, TESTBED_INIT_TIME_FORMAT)
except ValueError:
    pass


binary_name = "crystal_test.bin"
simnum = 0
for (power, sink, num_senders, n_empty, glossy_version) in itertools.product(powers, sinks, num_senderss, n_emptys, glossy_versions):
    n_empty = NemptyTuple(*n_empty)
    power = PowerTuple(*power)
    simdir = "sink%03d_snd%02d_%s_p%1d_%06x_c%02d_e%.2f_ns%02d_nt%02d_na%02d_ds%.2f_dt%.2f_da%.2f_syna%d_pl%03d_r%02dy%02dz%02dx%02d_dyn%d_fe%02d_%s_%s_B%s"%(sink, num_senders, glossy_version, power.smart, power.power, radio_cfg, period, n_tx_s, n_tx_t, n_tx_a, dur_s, dur_t, dur_a, sync_ack, payload, n_empty.r, n_empty.y, n_empty.z, n_empty.x, dyn_nempty, full_epochs, testbed, chmap, boot_chop)

    testbed_filled_template   = testbed_template.render(
        ts_init = START_TIME,
        duration_seconds = SIM_DURATION,
        duration_minutes = int(SIM_DURATION / 60),
        abs_bin_path = "",
        targets = "\"\""
        )
    sim_testfile = json.loads(testbed_filled_template)
    if sim_start_time is not None:
        sim_testfile["ts_init"] = datetime.datetime.\
                strftime(sim_start_time, TESTBED_INIT_TIME_FORMAT)
        # increase sim_start_time for the next simulation
        sim_start_time += datetime.timedelta(seconds= SIM_DURATION + SIMULATION_OFFSET)

    if os.path.isdir(simdir):
        continue
    try:
        # Store the list of nodes to the "target" field within the testfile
        sim_testfile["image"]["target"] = list(nodes)

        if sink not in nodes:
            raise Exception("Sink node doesn't exist")

        all_senders = [x for x in nodes if x!=sink]
        if (num_senders > len(all_senders)):
            raise Exception("More senders than nodes: %d > %d, skipping test"%(num_senders, len(all_senders)))

        new_env = mk_env(power, radio_cfg, sink, num_senders, n_empty, glossy_version)
        prepare_binary(simdir, binary_name, all_senders, active_epochs, num_senders, new_env)

        # Write testbed file in the simulation folder
        abs_simdir = os.path.abspath(simdir)
        abs_bname  = os.path.join(simdir, binary_name)
        sim_testfile["image"]["file"] = os.path.relpath(abs_bname, abs_simdir)
        sim_testfile_path = os.path.join(simdir, "sim_crystal_test.json")
        json.dump(sim_testfile, open(sim_testfile_path, "w"), indent=2)

        num_nodes = len(all_senders)

        with open(os.path.join(simdir, "params_tbl.txt"), "w") as f:
            p = OrderedDict()
            p["testbed"] = testbed
            p["num_nodes"] = num_nodes
            p["active_epochs"] = active_epochs
            p["start_epoch"] = start_epoch
            p["seed"] = seed
            p["glossy_version"] = glossy_version
            p["smart_pow"] = power.smart
            p["power"] = "0x%06x" % power.power
            p["radio_cfg"] = radio_cfg
            p["period"] = period
            p["senders"] = num_senders
            p["sink"] = sink
            p["n_tx_s"] = n_tx_s
            p["n_tx_t"] = n_tx_t
            p["n_tx_a"] = n_tx_a
            p["dur_s"] = dur_s
            p["dur_a"] = dur_a
            p["dur_t"] = dur_t
            p["sync_ack"] = sync_ack
            p["n_empty"] = n_empty.r
            p["n_empty.y"] = n_empty.y
            p["n_empty.z"] = n_empty.z
            p["n_empty.x"] = n_empty.x
            p["payload"] = payload
            p["chmap"] = chmap
            p["boot_chop"] = boot_chop
            p["full_epochs"] = full_epochs
            header = " ".join(p.keys())
            values = " ".join([str(x) for x in p.values()])
            f.write(header)
            f.write("\n")
            f.write(values)
            f.write("\n")
        simnum += 1
        print("-"*40)
    except Exception as e:
        traceback.print_exc()
        if os.path.isdir(simdir):
            rmtree(simdir)
        raise e


print("%d simulation(s) generated"%simnum)
