#!/usr/bin/python3
from __future__ import print_function
from multiprocessing import Process
from builtins import chr
from subprocess import Popen, PIPE
import subprocess
import sys
import argparse
import os
import glob
import shutil
import time

from threading import Timer
buglist = []
count = 0
core_set = set()
time_bd =3 
rc_dic = {6:0, 9:0, 11:0}
bugtype_dic = {6:"Abnormal-Termination", 9:"Performance Bug", 11:"Segmentation-Fault"}
#kill = lambda process : process.kill()

def parse_file(filepath):
    f = open(filepath, 'r')
    s = f.read()
    in_seq = list(map(int, s.split()))
    return in_seq

def null_trim(sequence):
	if 0 in sequence:
		sequence = sequence[:sequence.index(0)]
	return sequence

def seq_to_str(sequence):
    sequence = null_trim(sequence)
    try:
        abs_sequence = list(map(abs, sequence)) #do not use unichr compatiable with python3
        char_sequence = list(map(chr, abs_sequence)) #do not use unichr compatiable with python3
    except:
        return []
    string = ''.join(char_sequence)
    return string

def kill(process, input_vec):
  process.kill()
  buglist.append(input_vec)
  print("timeover!")

def execute(pgm, prog_input,path):
    if 'sed' in pgm:
        cmd = ["./bin/"+pgm, prog_input, "README_Chameleon"]
    elif 'gawk' in pgm:
        cmd = ["./bin/"+pgm, prog_input, "text_inputs/recall.inp"]
    elif 'grep' in pgm:
        cmd = ["./bin/"+pgm, prog_input, "README_Chameleon"]
    elif 'tree' in pgm:
        cmd = ["./bin/"+pgm, prog_input[0:9], prog_input[9:]]
    elif 'ls' in pgm:
        cmd = ["./bin/"+pgm, prog_input, "*.c"]
    elif 'vim' in pgm:
        cmd = ["./bin/"+pgm] 
    run = subprocess.Popen(cmd)    
    print('cmd: ',cmd, '\n')

    my_timer = Timer(time_bd, kill, [run, (prog_input, path)])
    try:
        my_timer.start()
        stdout, stderr = run.communicate()
        rc = run.returncode 
        if -65<rc<0:        
            #buginputcopy(path)
            rc_dic[abs(rc)] = rc_dic[abs(rc)]+1
    finally:
        my_timer.cancel()
    return rc

def buginputcopy(bug_input):
    bad_dir=pgm+"_bug_"+stgy
    if not os.path.exists(bad_dir):
        os.mkdir(bad_dir)
    cp_cmd = " ".join(["cp", bug_input, bad_dir])
    os.system(cp_cmd)
 

def bugcheck(pgm,path):
    in_seq = parse_file(path)
    global count
    count+=1
    prog_in = seq_to_str(in_seq)
    if not prog_in:
        return []
    #print("prog_input:", prog_in)
    print("input_sequence:", in_seq)
    rc = execute(pgm, prog_in, path)
    
    return rc, prog_in


def traverse(pgm, path):
    stgys = ["CFDS", "CGS", "Random", "Gen", "RoundRobin", "Param"]
     
    stgy = "Chameleon"
    pgm = pgm.split('/')[-1]
    folder = path.split('/')
    for subpath in folder:
        for candidate in stgys:
            if subpath.find(candidate) >= 0:
                stgy = candidate 
                print (stgy)
     
    def list_print(lst):
        str_maxlen = [0,0,0]
        for each in lst:
            for i in range(0,3):
                if len(each[i]) > str_maxlen[i]:
                    str_maxlen[i]=len(each[i])
        
        print("BugType".ljust(str_maxlen[0]+2), "File".ljust(str_maxlen[1]+2), "Bug-Triggering Inputs".ljust(str_maxlen[2]+4))
        print ("------------------------------------------------------------")
        for each in lst:
            print(each[0].ljust(str_maxlen[0]+2), each[1].ljust(str_maxlen[1]+2), "\'"+each[2]+"\'")
            print ("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
    
    printables = []
    notprintables = []
    for each in glob.glob(os.path.join(path, '*')):
        input_fname = each.split('/')[-1]
        print (input_fname)
        rc, buginput = bugcheck(pgm, each)
        print ("return code: "+str(rc))
        print ("------------------------------------------------------------")
        if -65<rc<0:        
            printables.append((bugtype_dic[abs(rc)], input_fname, buginput))
         
    if len(printables):
        print ("--------------------------Summary---------------------------")
        print("Tool(Heuristic): "+stgy+",   Binary Version: "+pgm)
        print("# of total bug-triggering inputs: ", len(printables))
        for rc in rc_dic.keys():
            print("  - # of "+bugtype_dic[rc]+": "+str(rc_dic[rc]))
        print ("------------------------------------------------------------")
        list_print(printables)
        print ("------------------------------------------------------------")
    else:
        print("printable bug input not exists")
        pass
    
    rm_cmd = "find . -empty -type d -delete -print"
    os.system(rm_cmd)


if __name__ == "__main__" :
    parser = argparse.ArgumentParser(description='suspicious input folder path')
    parser.add_argument('binary')
    parser.add_argument('path')
    args = parser.parse_args()
    pgm = args.binary
    path = args.path
    traverse(pgm, path)
