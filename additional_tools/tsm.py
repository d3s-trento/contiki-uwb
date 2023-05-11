import subprocess

import os
import pandas as pd

import jobref


def get_tsm_slots(joblog: jobref.JobLog, force=False) -> pd.DataFrame:
    if isinstance(joblog, jobref.JobLogFile):
        return pd.read_csv(joblog.path)
    elif isinstance(joblog, jobref.JobLogId):
        FILENAME = 'tsm_slots.csv'
        path = os.path.join(joblog.get_cache_dir(), FILENAME)

        if force or not os.path.exists(path):
            joblog.create_folder_structure()

            with open(path, 'w') as f:
                subprocess.run(["get_tsm_slots.sh", '{}'.format(joblog.id)],
                                stdout=f)

            return pd.read_csv(path)
        else:
            return pd.read_csv(path)

    raise Exception('Unrecognized type of JobLog')
