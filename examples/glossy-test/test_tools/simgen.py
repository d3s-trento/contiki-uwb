#!/usr/bin/env python3
import os
import sys

BASE_PATH = os.path.abspath(os.path.dirname(sys.argv[0])) # point to the test_tools folder
CUR_DIR = os.path.abspath(os.path.curdir) # required to include params

sys.path.insert(0, BASE_PATH)
sys.path.insert(0, os.path.join(BASE_PATH, "analysis"))

# keep it as last in order to look for params.py in this folder first!
sys.path.insert(0, CUR_DIR)

import build_setting

from params import PARAMS
from simgen_templates import TESTBED_TEMPLATE

# -----------------------------------------------------------------------------
import re
import json
import copy
import datetime
import itertools
import subprocess
import git        # pick last commit hash
import pandas as pd
import shutil
from random import randint
from collections import OrderedDict

from jinja2 import Template

# -----------------------------------------------------------------------------
# Compute paths to required files and directories
# -----------------------------------------------------------------------------
SIMS_DIR  = os.path.abspath(os.path.join(CUR_DIR, PARAMS["sims_dir"]))
APP_DIR   = os.path.join(CUR_DIR, PARAMS["app_folder"])
APP_BINARY= os.path.abspath(os.path.join(APP_DIR, "glossy_test.bin"))
APP_SOURCE= os.path.abspath(os.path.join(APP_DIR, "glossy_test.c"))
APP_PRJCONF = os.path.abspath(os.path.join(APP_DIR, "project-conf.h"))
# -----------------------------------------------------------------------------

# text of the project conf generated by the simgen
PROJECT_CONF_TEXT =\
"""\
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_
#include "radio-conf.h"

{}

#endif /* PROJECT_CONF_H_ */
"""

PRJ_SMARTTX = "smarttx"
PRJ_NTX = "ntx"
PRJ_TX_POWER = "tx_power"
PRJ_VERSION = "version"
PRJ_DYN_SLOT_ESTIMATION = "dyn_slot_estimation"
PRJ_PAYLOAD = "payload"
PRJ_LOG_LEVEL = "log_level"
PRJ_INITIATOR = "initiator"

# -----------------------------------------------------------------------------
# define the available glossy configurations

GVERSION_STD_DYN = "std_dynamic"
GVERSION_STD_STATIC = "std_static"
GVERSION_TXO_DYN = "txo_dynamic"
GVERSION_TXO_STATIC = "txo_static"
GVERSIONS = {GVERSION_STD_DYN: ("GLOSSY_STANDARD_VERSION", 1),
             GVERSION_STD_STATIC: ("GLOSSY_STANDARD_VERSION", 0),
             GVERSION_TXO_DYN: ("GLOSSY_TX_ONLY_VERSION", 1),
             GVERSION_TXO_STATIC: ("GLOSSY_TX_ONLY_VERSION", 0)}

# -----------------------------------------------------------------------------
# Extract parameters from from the params.py to generate simulations

ALL_NODES = set(PARAMS["nodes"])
INITS     = set(PARAMS["initiator"])
SIM_DURATION = int(PARAMS["duration"])
TS_INIT   = PARAMS["ts_init"]
VERSIONS = PARAMS["versions"]
PAYLOADS  = PARAMS["payloads"]
NTXS  = PARAMS["ntxs"]
POWERS   = PARAMS["powers"]
LOG_LEVEL = PARAMS["log_level"]

LOG_INFO = "info"
LOG_DEBUG = "debug"
LOG_ERROR = "error"
LOG_ALL = "all"
LOG_NONE = "none"
LOG_LEVELS = {LOG_ALL:   "GLOSSY_LOG_ALL_LEVELS",
              LOG_NONE:  "GLOSSY_LOG_NONE_LEVEL",
              LOG_DEBUG: "GLOSSY_LOG_DEBUG_LEVEL",
              LOG_INFO:  "GLOSSY_LOG_INFO_LEVEL",
              LOG_ERROR: "GLOSSY_LOG_ERROR_LEVEL"}

SIMULATION_OFFSET = 60                  # Time used to separate two consecutive simulations

# -----------------------------------------------------------------------------
# SINGLE SIMULATION VARIABLES
# -----------------------------------------------------------------------------
TESTBED_FILENAME   = "glossy_test_simulation.json"
BINARY_FILENAME    = "glossy_test_simulation.bin"
PROJECT_CONF       = "project_conf.h"
BUILD_SETTINGS     = "build_settings.txt"
BUILD_OUT          = "build_out.txt"
# -----------------------------------------------------------------------------
TESTBED_INIT_TIME_FORMAT = "%Y-%m-%d %H:%M"


def check_params():
    """Check on params.

    * every initiator is an available node
    * the application folder contains effectively glossy_test.c
    * the application folder contains the app binary glossy_test.bin
    * check if simulations directory **can** be created (creation occurs later on)
    * duration is an integer number
    * versions are legitimate
    * smarttxs are either True or False and power is within the range given
      by the 4 bytes numerical value
    * log level is valid
    """
    if len(INITS) < 1:
        raise ValueError("No initiator node defined")
    if not INITS.issubset(ALL_NODES):
        raise ValueError("Initiators are not a subset of nodes available")

    application_path = os.path.join(APP_DIR, "glossy_test.c")
    if not os.path.exists(application_path):
        raise ValueError("The given app folder doesn't contain glossy_test.c\n" +
            "Given: {}".format(application_path))

    # check the simulations dir is not a normal file (wtf?!)
    if os.path.exists(SIMS_DIR) and os.path.isfile(SIMS_DIR):
        raise ValueError("Cannot create simulations directory. A file with the same name already exists.")

    if os.path.exists(APP_PRJCONF):
        raise ValueError("project-conf.h file already present. Rename it to avoid conflict with the simgen.")

    if len(VERSIONS) < 1:
        raise ValueError("No version defined")
    for version in VERSIONS:
        if not version in GVERSIONS.keys():
            raise ValueError("Invalid version defined in params.py: {}".format(version))

    if len(PAYLOADS) < 1:
        raise ValueError("No payload defined")
    for payload in PAYLOADS:
        if payload < 0 or payload > 115:
            raise ValueError("Defined payload size is not within the boundary 0-115, given {}".format(payload))

    for smarttx, power in POWERS:
        if not smarttx in (0,1):
            raise ValueError("Invalid value for smarttx: {}".format(smarttx))
        if power < 0 or power > 0xffffffff:
            raise ValueError("Invalid power value given, allowed range " +
                    "0x" + "00"*4 + " - 0x" + "ff"*4 +
                    " Given {}".format(power))

    if len(NTXS) < 1:
        raise ValueError("No NTX defined")
    for ntx in NTXS:
        try:
            if ntx < 0 or ntx > 255:
                raise ValueError("Defined ntx value is not within the range 0-255, given {}".format(ntx))
        except TypeError:
            raise ValueError("NTXS must be a list of integers")

    if LOG_LEVEL not in LOG_LEVELS.keys():
        raise ValueError("Invalid log level given: {}".format(LOG_LEVEL))

    return True

def get_project_conf_content(flag_values):
    defines = [
        "#define INITIATOR_ID\t%d"                      % flag_values[PRJ_INITIATOR],
        "#define DW1000_CONF_SMART_TX_POWER_6M8\t%d"    % flag_values[PRJ_SMARTTX],
        "#define DW1000_CONF_TX_POWER\t%s"              % hex(flag_values[PRJ_TX_POWER]),
        "#define GLOSSY_N_TX\t%d"                       % flag_values[PRJ_NTX],
        "#define GLOSSY_VERSION_CONF\t%s"               % flag_values[PRJ_VERSION],
        "#define GLOSSY_DYNAMIC_SLOT_ESTIMATE_CONF\t%d" % flag_values[PRJ_DYN_SLOT_ESTIMATION],
        "#define GLOSSY_TEST_CONF_PAYLOAD_DATA_LEN\t%d" % flag_values[PRJ_PAYLOAD],
        "#define GLOSSY_LOG_LEVEL_CONF\t%s"             % flag_values[PRJ_LOG_LEVEL],

        # constant values
        "#define START_DELAY_INITIATOR\t30",
        "#define START_DELAY_RECEIVER\t10",
    ]
    return PROJECT_CONF_TEXT.format(str.join("\n", defines))


def get_glossy_test_conf():
    """Extract configuration define in the glossy_test.c file.
    """
    RTIMER_SYMBOLIC = r"rtimer_second"
    # assume millisecond granularity
    # if this is not sufficient (IT SHOULD BE!) throw an error an inform
    # the user that this code should be changed to the new granularity
    # by changing the value of RTIMER_VALUE

    RTIMER_VALUE = "1000"               # ms granularity
    GRANULARITY  = "ms"
    period_reg = re.compile(r"\s*#define\s+glossy_period\s+(\([^\)]*\))")
    slot_reg   = re.compile(r"\s*#define\s+glossy_t_slot\s+(\([^\)]*\))")
    guard_reg  = re.compile(r"\s*#define\s+glossy_t_guard\s+(\([^\)]*\))")
    period, slot, guard = tuple( ("NA " * 3).split() )

    with open(APP_SOURCE, "r") as fh:
        for line in fh:
            period_match = period_reg.match(line.lower())
            slot_match   = slot_reg.match(line.lower())
            guard_match  = guard_reg.match(line.lower())

            if period_match:
                period = period_match.group(1)
                period = re.sub(RTIMER_SYMBOLIC, RTIMER_VALUE, period)
                period = int(eval(period))
            if slot_match:
                slot = slot_match.group(1)
                slot = re.sub(RTIMER_SYMBOLIC, RTIMER_VALUE, slot)
                slot = int(eval(slot))
            if guard_match:
                guard = guard_match.group(1)
                guard = re.sub(RTIMER_SYMBOLIC, RTIMER_VALUE, guard)
                guard = int(eval(guard))

    # period, slot and guard should be greater than 0
    if not all(map(lambda x: x and x > 0, (period, slot, guard))):
        raise ValueError("Some parameters in the glossy_test.c are not defined " +\
                "or are their value is below 1ms granularity."+\
                " In the latter case, change the reference granualarity in simgen.py")

    return period, slot, guard

def generate_simulations(overwrite=False):
    """Create a simulation folder for each configuration

    * the simulation has a name made by:

        a. from glossy_test.c
            1. epoch
            2. slot
            3. guard time
            4. number of transmission

        b. the glossy configuration
            1. version and
            2. estimation approach used

        c. smarttx and tx power configuation
        d. payload length
        e. duration of the simulation
        f. initiator id

        The simulation name will be used in the simulation folder creation
        and to determine the path used to store results later


    * generate the project conf file for the simulation
    * produce the binary, printing compilation settings
      to a specific build_settings.txt file
      in the simulation directory

        - if compilation fails, return the error to stdout

    * create a json file to use in the unitn testbed:

        - contains the commit id at the current moment
        - assigns simulation name to sim_id
        - schedule ts_init if a base datetime is given
    """
    testbed_template   = Template(TESTBED_TEMPLATE)

    # create dir in which collecting simulations
    if not os.path.exists(SIMS_DIR):
        os.makedirs(SIMS_DIR)

    # Render the json test template with common values
    # In each simulation fill the differing details using
    # directly the json structure
    testbed_filled_template   = testbed_template.render(\
            duration_seconds = SIM_DURATION,\
            duration_minutes = int(SIM_DURATION / 60),\
            ts_init = TS_INIT)
    json_testbed  = json.loads(testbed_filled_template)

    # check if the json define a scheduled init
    plan_schedule = False
    try:
        sim_ts_init = datetime.datetime.\
                strptime(json_testbed["ts_init"], TESTBED_INIT_TIME_FORMAT)
        plan_schedule = True
    except ValueError:
        pass

    # get a prefix based on glossy configuration which are
    # common to all simulations currently generated
    PERIOD, SLOT, GUARD = list(get_glossy_test_conf())
    NAME_PREFIX = "period%d_slot%d_guard%d" % (PERIOD, SLOT, GUARD)

    simulations = 0
    for init_id, version_label, ntx, power_conf, payload in itertools.product(INITS,\
                                       VERSIONS,\
                                       NTXS,
                                       POWERS,
                                       PAYLOADS):
        json_sim = copy.deepcopy(json_testbed)

        smarttx, txpower = power_conf
        version, slot_estimation = GVERSIONS[version_label]

        # give a name to current simulation and create
        # the corresponding folder
        sim_name = NAME_PREFIX + "_ntx{}_smarttx{}_txpower{}_{}_payload{}_duration{}_init{}"\
                .format(ntx, smarttx, hex(txpower), version_label, payload, SIM_DURATION, init_id)
        sim_dir  = os.path.join(SIMS_DIR, sim_name)

        # if a folder with the same name already existed then
        # check if overwrite is set
        # * if it is, then remove the previous directory and
        #   create a brand new dir
        # * if not, raise an exception
        if os.path.exists(sim_dir):
            if overwrite:
                shutil.rmtree(sim_dir)
            else:
                raise ValueError("A simulation folder with the same "+\
                        "name already exists: {}".format(sim_dir))

        try:
            os.mkdir(sim_dir)

            # abs path for testbed file
            sim_testbed_file  = os.path.join(sim_dir, TESTBED_FILENAME)

            # copy binary and the project conf and get related abs paths
            abs_sim_binary = os.path.abspath(os.path.join(sim_dir, BINARY_FILENAME))
            abs_sim_prconf = os.path.abspath(os.path.join(sim_dir, PROJECT_CONF))
            abs_build_settings  = os.path.abspath(os.path.join(sim_dir, BUILD_SETTINGS))
            abs_build_out  = os.path.abspath(os.path.join(sim_dir, BUILD_OUT))
            abs_sim_settings = os.path.abspath(os.path.join(sim_dir, build_setting.SIM_SETTINGS))

            # make the project conf for the current simulation
            project_conf_text = get_project_conf_content(
                                  {
                                  PRJ_INITIATOR: init_id,
                                  PRJ_VERSION: version,
                                  PRJ_DYN_SLOT_ESTIMATION: slot_estimation,
                                  PRJ_NTX: ntx,
                                  PRJ_SMARTTX: smarttx,
                                  PRJ_TX_POWER: txpower,
                                  PRJ_PAYLOAD: payload,
                                  PRJ_LOG_LEVEL: LOG_LEVELS[LOG_LEVEL]})
            with open(APP_PRJCONF, "w") as fh:
                fh.write(project_conf_text)

            current_dir = os.getcwd()
            os.chdir(APP_DIR)
            print("-"*40 + "\nCleaning previous build")
            subprocess.check_call(["make","clean"], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)

            print("-"*40 + "\nBuilding simulation\n" + "-" * 40)
            # save configuration settings printed at compile time (through pragmas)
            # to the simulation directory.
            #
            # If compilation went wrong, print all the output to stdout
            pragmas = []
            outlines  = []
            pragma_message = re.compile(".*note:\s+#pragma message:\s+(.*)")
            make_process = subprocess.Popen(["make"], stderr=subprocess.STDOUT, stdout=subprocess.PIPE)
            for line in make_process.stdout:
                line = line.decode("utf-8")
                match = pragma_message.match(line)
                if match:
                    pragmas.append(match.group(1))
                outlines.append(line)
            make_process.wait()

            if make_process.returncode != 0:
                print(str.join("", outlines))
                raise subprocess.CalledProcessError(returncode=make_process.returncode, cmd="make")
            else:
                # print to stdout the pragmas as feedback
                print(str.join("\n", pragmas))


            bsettings = build_setting.parse_build_setting_lines(pragmas)
            channel, prf, prlen, drate, sfd_mode = build_setting.get_radio_fixed_settings(bsettings)
            sim_conf=build_setting.get_settings_summary(bsettings)

            # save settings to the corresponding csv
            values = list(sim_conf.values())
            columns= list(sim_conf.keys())
            simconf_sf = pd.DataFrame([values], columns=columns)
            simconf_sf.to_csv(abs_sim_settings, index=False)
            # save build settings to simdir
            with open(abs_build_settings, "w") as fh:
                fh.write(str.join("\n", pragmas) + "\n")
            # save build output
            with open(abs_build_out, "w") as fh:
                fh.write(str.join("", outlines) + "\n")

            # return to previous directory
            os.chdir(current_dir)

            shutil.copy(APP_BINARY, abs_sim_binary)
            shutil.copy(APP_PRJCONF, abs_sim_prconf)

            # fill templates with simulation particulars
            # set relative link to bin file in .json test file
            json_sim["image"]["file"] = os.path.relpath(abs_sim_binary, sim_dir)
            json_sim["image"]["target"] = list(ALL_NODES)

            # removed sim_id field from the resulting testbed json.
            # In case it is desired to have it again: add the following
            # line to the testbed template in simgen_templates.py
            # and decomment python command here, right below
            #   TO TEMPLATE -> "sim_id": "< sim_id >",
            #   UNCOMMENT   -> json_sim["sim_id"] = sim_name

            # include the last commit of the repo in the json file.
            # Still, the user has to commit any new change in order
            # for this info to be useful at all
            json_sim["commit_id"] = str(git.Repo(APP_DIR, search_parent_directories=True).head.commit)

            # if scheduling has to be planned, then add to each
            # simulation an offset equal to the test duration
            if plan_schedule:
                json_sim["ts_init"] = datetime.datetime\
                        .strftime(sim_ts_init, TESTBED_INIT_TIME_FORMAT)
                sim_ts_init += datetime.timedelta(seconds=SIM_DURATION + SIMULATION_OFFSET)

            # convert json back to string
            sim_testbed = json.dumps(json_sim, indent=1)

            with open(sim_testbed_file, "w") as fh:
                fh.write(sim_testbed)

        except BaseException as e:
            clean_simgen_temp()
            # delete current simulation directory
            shutil.rmtree(sim_dir)
            raise e

        # increse sim counter
        simulations += 1

    # try to clean the project
    try:
        clean_simgen_temp()
    except:
        pass

    print("{} Simulations generated\n".format(simulations) + "-"*40)

def clean_simgen_temp():
    print("-"*40 + "\nCleaning simgen temporary files...\n" + "-"*40)
    current_dir = os.getcwd()
    os.chdir(APP_DIR)
    subprocess.check_call(["make","clean"], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    os.remove(APP_PRJCONF)
    os.chdir(current_dir)

def delete_simulations():
    if not os.path.exists(SIMS_DIR):
        raise ValueError("Simulations directory doesn't exist. Nothing done")
    # generate a random sequence of a given length
    code_len = 5
    code = ""
    for i in range(0, code_len):
        code += str(randint(0, 9))
    user_code = input("Write the following security code before " +
        "deletion to be performed: {}\n".format(code))
    if user_code.strip() == code:
        print("Removing dir {}".format(SIMS_DIR))
        shutil.rmtree(SIMS_DIR)
        print("DONE!")
    else:
        print("Wrong security code given. Abort deletion...")


if __name__ == "__main__":

    from argparse import ArgumentParser

    parser = ArgumentParser(description="Glossy SIMGEN. Manage simulations (hopefully) without pain!")
    parser.add_argument("-fg" ,"--force-generation", action="store_true", help="REMOVE(!) previous sim folder in case of name clashes")
    parser.add_argument("-ti" ,"--testbed-info", action="store_true", help="Return the testbed file used in generated simulations")
    parser.add_argument("-pi" ,"--params-info", action="store_true", help="Return parameters used in generated simulations")
    parser.add_argument("-d"  ,"--delete-simulations", action="store_true", help="DELETE(!) the main simulation folder and all its contents")

    args = parser.parse_args()

    if args.testbed_info:
        print(TESTBED_TEMPLATE)
        sys.exit(0)
    elif args.params_info:
        import pprint
        pprint.pprint(PARAMS)
        sys.exit(0)
    if args.delete_simulations:
        delete_simulations()
        sys.exit(0)

    if not check_params():
        raise ValueError("Some parameter was not set correctly!")

    overwrite = False
    if args.force_generation:
        overwrite = True
    generate_simulations(overwrite)

