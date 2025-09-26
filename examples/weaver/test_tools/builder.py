#!/usr/bin/env python3
import os, sys, subprocess
import re, json
import copy, shutil
import logging
import random, itertools

from pathlib import Path
from collections import OrderedDict
from functools import reduce

import git                                      # pick last commit hash
from jinja2 import Template
from utility import Params

# -----------------------------------------------------------------------------
TEST_TOOLS_PATH = Path(sys.argv[0]).parent.resolve()  # point to the test_tools folder
sys.path.insert(0, str(TEST_TOOLS_PATH))
#sys.path.insert(0, os.path.join(TEST_TOOLS_PATH, "analysis")) # no analysis folder atm
from builder_templates import TESTBED_TEMPLATE
# -----------------------------------------------------------------------------
logger = logging.getLogger(__name__)
logging.basicConfig(level=logging.DEBUG)
logging.getLogger(__name__).setLevel(level=logging.DEBUG)

git_logger = logging.getLogger(git.__name__)
git_logger.setLevel(logging.ERROR)
# -----------------------------------------------------------------------------
# SINGLE SIMULATION VARIABLES
# -----------------------------------------------------------------------------
JOB_FILENAME    = "weaver.json"
BINARY_FILENAME = "weaver.bin"
BUILD_OUT       = "build_out.txt"
CONFIG_FILENAME = "simulation.conf"
# -----------------------------------------------------------------------------
def get_make_params(simulation):
    # please keep the indentation. It will make easier multiline editing
    make_params = [
        "SIMGEN=true",
        "NODES_DEPLOYED=\"%s\""                % str.join(",", map(str, sorted(get_set_originators(simulation)))),# no space after comma, or Make will complain :(
        "WEAVER_ORIGINATORS_TABLE=\"%s\"" % str.join(",", map(str, simulation.originators_table)),
        "SINK_ID=%d"                        % simulation.sink_id,
        "SINK_RADIUS=%d"                    % simulation.sink_radius,
        # "WEAVER_MAX_EPOCH_SLOT=%d"     % simulation.max_slot,
        "WEAVER_N_ORIGINATORS=%d"      % simulation.n_orig,
        "WEAVER_APP_START_EPOCH=%d"    % simulation.start_epoch,
        "WEAVER_EPOCHS_PER_CYCLE=%d"   % simulation.epochs_per_cycle,
        "EXTRA_PAYLOAD_LEN=%d"              % simulation.payload,
        "WEAVER_WITH_FS=%d"                 % simulation.weaver_with_fs,
    ]
    return str.join(" ", make_params)

def check_params(params):
    assert params.start_time.lower().startswith("asap")     # the simgen is no more concerned with scheduling
    assert Path(".").joinpath(Path(params.app_dir)).resolve().exists() 
    assert isinstance(params.duration, int)
    # assert isinstance(params.max_slot, int)
    assert isinstance(params.payload, int) and params.payload >= 0 and params.payload < 104 # Weaver + TSM add 23B
    assert isinstance(params.start_epoch, int)
    assert isinstance(params.epochs_per_cycle, int)
    assert isinstance(params.seed, int)
    assert all(map(lambda x: isinstance(x, int), params.nodes))
    assert all(map(lambda x: params.nodes.count(x) == 1, params.nodes))
    assert all(map(lambda x: len(x) == 2 and isinstance(x[0], int) and isinstance(x[1], int), params.sinks))
    assert all(map(lambda x: isinstance(x, int), params.num_originators))

    if hasattr(params, "originators_schedule"):
        assert not(hasattr(params, "fixed_originators")) # fixed originators cannot be defined with originators schedule
        assert not(hasattr(params, "random_originators")) # random originators cannot be defined with originators schedule
        assert all(map(lambda x: isinstance(x, list), params.originators_schedule))
        max_n_orig = max(params.num_originators)
        assert all(map(lambda epoch_origs: len(epoch_origs) >= max_n_orig, params.originators_schedule))

        originators  = reduce(lambda x,y: x + y, params.originators_schedule)
        assert all(map(lambda x: isinstance(x, int), originators))
        assert set(originators).issubset(set(params.nodes)) == True

        # check each orig schedule to contain unique ids
        for schedule in params.originators_schedule:
            for nid, count in {e: schedule.count(e) for e in schedule}.items():
                assert count == 1, f"Node {nid} is defined twice in a schedule"

    explicit_originators = set()
    if hasattr(params, "fixed_originators"):
        assert not(hasattr(params, "originators_schedule")) # originators schedule cannot be defined with fixed originators
        assert all(map(lambda x: params.fixed_originators.count(x) == 1, params.fixed_originators))
        explicit_originators.update(set(params.fixed_originators))

    if hasattr(params, "random_originators"):
        assert not(hasattr(params, "originators_schedule")) # originators schedule cannot be defined with fixed originators
        assert all(map(lambda x: params.random_originators.count(x) == 1, params.random_originators))
        explicit_originators.update(set(params.random_originators))

    if MOBILE_NODES is False:
        assert explicit_originators.issubset(set(params.nodes)) == True

    return True

def check_simulation(simulation):
    check_params(simulation)
    assert isinstance(simulation.n_orig, int)
    assert isinstance(simulation.sink_id, int)
    assert all(map(lambda x: isinstance(x, int), simulation.originators_table))
    assert len(simulation.originators_table) == simulation.epochs_per_cycle * simulation.n_orig
    return True

def get_deployed_nodes(simulation):
    deployed_nodes = set(simulation.nodes)
    try:
        deployed_nodes.update(simulation.fixed_originators)
    except AttributeError:
        pass
    try:
        deployed_nodes.update(simulation.random_originators)
    except AttributeError:
        pass
    return sorted(deployed_nodes)

def get_set_originators(simulation):
    table = []
    table_random = random.Random(simulation.seed)
    # check if all originators are scheduled per epoch
    try:
        for epoch_origs in simulation.originators_schedule:
            table += copy.deepcopy(epoch_origs[0:simulation.n_orig])
        return table
    except AttributeError:
        pass

    # then, try use fixed senders
    try:
        # the sink cannot be a fixed sender
        fixed = set(simulation.fixed_originators) - set([simulation.sink_id])
    except AttributeError:
        fixed = set() # no node is fixed

    # Extract the remaining U - fixed senders.
    # Use explicit random originators if defined, pick testbed nodes otherwise.
    try:
        rnd_originators = set(simulation.random_originators)
    except AttributeError:
        rnd_originators = set(simulation.nodes)

    rnd_originators = list(rnd_originators - fixed - set([simulation.sink_id]))

    # pick at most n_orig fixed senders
    fixed = list(fixed)[:simulation.n_orig] # from set to list

    return set(fixed).union(set(rnd_originators))


def get_originators(simulation):
    table = []
    table_random = random.Random(simulation.seed)
    # check if all originators are scheduled per epoch
    try:
        for epoch_origs in simulation.originators_schedule:
            table += copy.deepcopy(epoch_origs[0:simulation.n_orig])
        return table
    except AttributeError:
        pass

    # then, try use fixed senders
    try:
        # the sink cannot be a fixed sender
        fixed = set(simulation.fixed_originators) - set([simulation.sink_id])
    except AttributeError:
        fixed = set() # no node is fixed

    # Extract the remaining U - fixed senders.
    # Use explicit random originators if defined, pick testbed nodes otherwise.
    try:
        rnd_originators = set(simulation.random_originators)
    except AttributeError:
        rnd_originators = set(simulation.nodes)

    rnd_originators = list(rnd_originators - fixed - set([simulation.sink_id]))

    # pick at most n_orig fixed senders
    fixed = list(fixed)[:simulation.n_orig] # from set to list

    u_rnd = simulation.n_orig - len(fixed)
    for epoch in range(simulation.epochs_per_cycle):
        epoch_origs = fixed + table_random.sample(rnd_originators, u_rnd)
        assert len(set(epoch_origs)) == simulation.n_orig
        assert not simulation.sink_id in epoch_origs
        table +=  epoch_origs
    return table

def generate_simulations(params, overwrite=False):
    """Create a simulation folder for each configuration
    """
    # -----------------------------------------------------------------------------
    # Compute paths to required files and directories
    # -----------------------------------------------------------------------------
    app_dir      = Path(params.app_dir).resolve()
    app_binary   = app_dir.joinpath("weaver.bin").resolve()
    app_prjconf  = app_dir.joinpath("project-conf.h").resolve()

    testbed_template   = Template(TESTBED_TEMPLATE)
    testbed_filled_template   = testbed_template.render(\
            duration_seconds = params.duration,\
            duration_minutes = int(params.duration / 60),\
            start_time = params.start_time)
    json_testbed  = json.loads(testbed_filled_template)

    # create dir in which collecting simulations
    sims_dir = Path(params.sims_dir).resolve()
    if not sims_dir.exists():
        os.makedirs(str(sims_dir))

    # get a prefix based on configuration which are
    # common to all simulations currently generated
    NAME_PREFIX = "weaver"
    NAME_SUFFIX = "_duration%d" % params.duration

    nsimulations = 0
    for (sink_id, sink_radius), n_orig, withFS in itertools.product(params.sinks, params.num_originators, [0, 1]):

        json_sim = copy.deepcopy(json_testbed)
        simulation = params.update(\
          **{"sink_id": sink_id,
             "sink_radius": sink_radius,
             "n_orig":  n_orig,
             "weaver_with_fs": withFS})
        simulation = simulation.update(**{"originators_table": get_originators(simulation)})
        simulation = simulation.update(**{"deployed_nodes":    get_deployed_nodes(simulation)})
        check_simulation(simulation)


        # give a name to current simulation and create
        # the corresponding folder
        sim_name = NAME_PREFIX +\
                "_s{}o{}w{}FS".format(sink_id, n_orig, "o" if withFS == 0 else "") +\
                NAME_SUFFIX

        sim_dir  = sims_dir.joinpath(sim_name).resolve()
        # if a folder with the same name already exists then
        # check if overwrite is set
        # * if it is, then remove the previous directory and
        #   create a brand new dir
        # * if not, raise an exception
        if sim_dir.exists():
            if overwrite:
                shutil.rmtree(str(sim_dir))
            else:
                raise ValueError("A simulation folder with the same "+\
                        "name already exists: {}".format(str(sim_dir)))

        try:
            clean_simgen_temp(app_dir)
            os.mkdir(str(sim_dir))

            # abs path for testbed file
            sim_testbed_file  = os.path.join(sim_dir, JOB_FILENAME)

            # copy binary and the project conf and get related abs paths
            sim_binary = sim_dir.joinpath(BINARY_FILENAME).resolve()
            sim_prconf = sim_dir.joinpath("project-conf.h").resolve()
            build_out  = sim_dir.joinpath(BUILD_OUT).resolve()
            sim_diff   = sim_dir.joinpath("diff.txt").resolve()
            simconf    = sim_dir.joinpath(CONFIG_FILENAME).resolve()
            repo = git.Repo(params.app_dir, search_parent_directories=True)

            make_args = get_make_params(simulation)

            current_dir = os.getcwd()
            os.chdir(params.app_dir)
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
            make_process = subprocess.Popen("make " + make_args, shell=True, stderr=subprocess.STDOUT, stdout=subprocess.PIPE)
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

            # save build output
            with open(build_out, "w") as fh:
                fh.write(str.join("", outlines) + "\n")

            # return to previous directory
            os.chdir(current_dir)

            shutil.copy(app_binary,     sim_binary)
            shutil.copy(app_prjconf,    sim_prconf)

            # fill templates with simulation details
            # set relative link to bin file in .json test file
            json_sim["binaries"]["bin_file"] = os.path.relpath(sim_binary, str(sim_dir))
            json_sim["binaries"]["targets"] = list(sorted(params.nodes))

            # include the last commit of the repo in the json file.
            json_sim["commit_id"] = str(repo.head.commit)
            with open(sim_diff, "w") as fh:
                subprocess.check_call(["git", "--no-pager", "diff", "--submodule=diff"], stdout=fh, stderr=subprocess.STDOUT)

            with open(sim_testbed_file, "w") as fh:
                # convert json back to string
                sim_testbed = json.dumps(json_sim, indent=1)
                fh.write(sim_testbed)

            # it's now safe to swap nodes with deployed_nodes,
            # so that we can define nodes not in the testbed infrastructure
            simulation.nodes = simulation.deployed_nodes
            del simulation.deployed_nodes
            with open(simconf, "w") as fh:
                json.dump(simulation.__dict__, fh, indent=2)

        except BaseException as e:
            clean_simgen_temp(app_dir)
            # delete current simulation directory
            shutil.rmtree(str(sim_dir))
            raise e

        # increse sim counter
        nsimulations += 1

    # try to clean the project
    try:
        clean_simgen_temp(app_dir)
    except:
        pass

    print("{} Simulations generated\n".format(nsimulations) + "-"*40)

def clean_simgen_temp(app_dir):
    print("-"*40 + "\nCleaning simgen temporary files...\n" + "-"*40)
    current_dir = os.getcwd()
    os.chdir(str(app_dir))
    subprocess.check_call(["make","clean"], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
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

    parser = ArgumentParser(description="Simgen. Manage simulations (hopefully) without pain!")
    parser.add_argument("-fg" ,"--force-generation", action="store_true", help="REMOVE(!) previous sim folder in case of name clashes")
    parser.add_argument("-ti" ,"--testbed-info", action="store_true", help="Return the testbed file used in generated simulations")
    parser.add_argument("-pi" ,"--params-info", action="store_true", help="Return parameters used in generated simulations")
    parser.add_argument("-d"  ,"--delete-simulations", action="store_true", help="DELETE(!) the main simulation folder and all its contents")

    args = parser.parse_args()

    if args.testbed_info:
        print(TESTBED_TEMPLATE)
        sys.exit(0)
    elif args.params_info:
        from params import PARAMS
        import pprint
        pprint.pprint(PARAMS)
        sys.exit(0)
    if args.delete_simulations:
        delete_simulations()
        sys.exit(0)

    # if True notifies the presence of nodes detached from the testbed
    # infrastructure (this will loose checks on the builder script)
    MOBILE_NODES = True

    params_fp = Path(".").resolve().joinpath("params.py")
    if not params_fp.exists():
        raise ValueError("No params.py file found in current directory")

    # make the params in the current folder takes precedence over
    # any other params
    sys.path.insert(0, str(params_fp.parent.resolve()))
    from params import PARAMS
    params = Params(**PARAMS)
    if not check_params(params):
        raise ValueError("Some parameter was not set correctly!")

    overwrite = False
    if args.force_generation:
        overwrite = True
    generate_simulations(params, overwrite)

