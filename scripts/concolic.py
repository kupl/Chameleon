from multiprocessing import Process

import signal
import subprocess
import os
import sys
import random
import json
import argparse
import datetime
import shutil
import re

start_time = datetime.datetime.now()
date = start_time.strftime('%m%d')
checker = 0
configs = {
	'script_path': os.path.abspath(os.getcwd()),
	'crest_path': os.path.abspath('../bin/run_crest'),
	'n_exec': 4000,
	'date': '0225',
	'top_dir': os.path.abspath('../experiments/')
}

def load_pgm_config(config_file):
	with open(config_file, 'r') as f:
		parsed = json.load(f)

	return parsed

def gen_run_cmd(pgm_config, idx, stgy, trial):
    crest = configs['crest_path']
    exec_cmd = pgm_config['exec_cmd']
    n_exec = str(configs['n_exec'])
    
    # An initial input for each large benchmark
    if (pgm_config['pgm_name']).find('grep') >= 0:
        input = "grep.input"
    if (pgm_config['pgm_name']).find('gawk') >= 0:
        input = "gawk.input"
    if (pgm_config['pgm_name']).find('sed') >= 0:
        input = "sed.input"
    if (pgm_config['pgm_name']).find('vim') >= 0:
        input = "vim.input"
    
    # An initial input for each small benchmark
    if pgm_config['pgm_name'] == 'replace':
        input = "replace.input"
    if pgm_config['pgm_name'] == 'floppy':
        input = "floppy.input"
    if pgm_config['pgm_name'] == 'cdaudio':
        input = "cdaudio.input"
    if pgm_config['pgm_name'] == 'kbfiltr':
        input = "kbfiltr.input"

    stgy_cmd = '-param'
    log = "logs/" + "__".join([pgm_config['pgm_name']+str(trial), stgy, str(idx)]) + ".log"
    weight = configs['script_path']+"/"+str(trial)+"_weights/" + str(idx) + ".w"
    run_cmd = " ".join([crest, exec_cmd, input, log, n_exec, stgy_cmd, weight])
    
    return (run_cmd, log)


def running_function(pgm_config, top_dir, log_dir, group_id, parallel_list, stgy, trial, time, stime):
    group_dir = top_dir + "/" + str(group_id)
    os.system(" ".join(["cp -r", pgm_config['pgm_dir'], group_dir]))
    os.chdir(group_dir)
    os.chdir(pgm_config['exec_dir'])
    os.mkdir("logs")

    dir_name = configs['date']+"__"+pgm_config['pgm_name']+"__"+stgy
    switch_log = open(configs['top_dir']+"/"+dir_name+"/"+stgy+"_total.log", 'a')
    for idx in range(parallel_list[group_id-1], parallel_list[group_id]):
        (run_cmd, log) = gen_run_cmd(pgm_config, idx, stgy, trial)
		
        with open(os.devnull, 'wb') as devnull:
            os.system(run_cmd)

        current_time = datetime.datetime.now()
        elapsed_time = str((current_time - start_time).total_seconds()+int(float(stime)))
        if time < ((current_time - start_time).total_seconds()):
            checker =1
            break
        else:
            grep_command = " ".join(["grep", '"It: 4000"', log])
            grep_line = (subprocess.Popen(grep_command, stdout=subprocess.PIPE,shell=True).communicate())[0]
            log_to_write = ", ".join([elapsed_time.ljust(10), str(trial)+"_"+str(idx).ljust(10), grep_line]).strip() + '\n'
            if log_to_write != "":
                switch_log.write(log_to_write)
                switch_log.flush()
        shutil.move(log, log_dir)
    
    switch_log.close() 
	

def run_all(pgm_config, n_iter, n_parallel, trial, stgy, time, stime):
    top_dir = "/".join([configs['top_dir'], configs['date']+"__"+stgy+str(trial), pgm_config['pgm_name']])
    log_dir = top_dir + "/logs"
    os.makedirs(log_dir)
    pgm_dir = pgm_config["pgm_dir"]
	
    procs = []
    count = n_iter / n_parallel
    rest = n_iter % n_parallel
    parallel_list = [1]
    p=1
    for i in range(1, n_parallel+1):
        if rest !=0:
            p = p + count + 1
            parallel_list.append(p)
            rest = rest -1
        else: 
            p = p + count
            parallel_list.append(p)
    
    for gid in range(1, n_parallel+1):
        procs.append(Process(target=running_function, args=(pgm_config, top_dir, log_dir, gid, parallel_list, stgy, trial, time, stime)))
    
    for p in procs:
        p.start()

	
if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("pgm_config")
    parser.add_argument("n_iter")
    parser.add_argument("n_parallel")
    parser.add_argument("stgy")
    parser.add_argument("trial")
    parser.add_argument("time")
    parser.add_argument("starttime")

    args = parser.parse_args()
    pgm_config = load_pgm_config(args.pgm_config)
    n_iter = int(args.n_iter)
    n_parallel = int(args.n_parallel)
    stgy = args.stgy
    trial = int(args.trial)
    time = int(args.time)
    stime = args.starttime
    run_all(pgm_config, n_iter, n_parallel, trial, stgy, time, stime)
