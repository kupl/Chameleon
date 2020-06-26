import os
import re
import sys
import numpy as np
import operator
import time, datetime, argparse, glob
import json
import random, array
import scipy.stats as stats
from collections import defaultdict
from copy import deepcopy


# Setting.
stgy="Chameleon"
lower, upper = -10.0, 10.0 #feature weight range
feat_num = 40
start_time = datetime.datetime.now()


# Hyper-parameter.
pool_size = 100 # Pool size of search heuristics. (n1)
ratio = 0.03 # Ratio of choosing good and bad heuristics from knowledge K.  
offspring_size = 10 # The number of offspring to be generated from each effective heuristic. (n3)
exploitation_rate = 0.8 # The exploitation rate. (n4)
parent_size = int((pool_size/offspring_size)*exploitation_rate) # The number of parent heuristics to generate offspring.
refine_ratio = 0.5 

#global variable
load_config = {}

def load_pgm_config(config_file):
    with open(config_file, 'r') as f:
        parsed = json.load(f)
    return parsed


configs = {
        'date':'0225',
        'script_path': os.path.abspath(os.getcwd()),
        'ex_dir': os.path.abspath('../experiments/'),
        'bench_dir': os.path.abspath('../benchmarks/'),
}


def Gen_Heuristics(iters):
    global load_config
    os.chdir(configs['script_path'])
    benchmark_dir = configs['bench_dir']+"/"+load_config['pgm_dir']+load_config['exec_dir']
    
    # "H" is a set of heuristics.
    H = iters+"_weights/" 
    if not os.path.exists(H):
        os.mkdir(H)
    
    # Randomly generate a set of heuristics.
    for idx in range(1, pool_size+1):
        weights = [str(random.uniform(-10, 10)) for _ in range(feat_num)] # of features:40
        fname = H + str(idx) + ".w"
        with open(fname, 'w') as f:
            for w in weights:
                f.write(str(w) + "\n")
    
    return H


def Concolic(pgm_config, time_budget, core_num, trial):
    global load_config
    #timeout check.
    current_time = datetime.datetime.now()
    total_time = (current_time-start_time).total_seconds()
    elapsed_time = str(total_time).ljust(10)
    if time_budget < total_time:
        clean()
        print "#############################################"
        print "################Time Out!!!!!################"
        print "#############################################"
        sys.exit()

    # Make main_directory. 
    os.chdir(configs['ex_dir'])
    dir_name = configs['date']+"__"+load_config['pgm_name']+"__"+stgy
    if not os.path.exists(dir_name):
        os.mkdir(dir_name)
    
    bug_dir = dir_name+"/"+"buginputs"
    if not os.path.exists(bug_dir):
        os.mkdir(bug_dir)

    # Run concolic testing.
    os.chdir(configs['script_path'])
    run_concolic = " ".join(["python", "concolic.py", pgm_config, str(pool_size), core_num, stgy, trial, str(time_budget), elapsed_time])
    os.system(run_concolic)
        
    # Store the results in the main_directory. 
    os.chdir(configs['ex_dir'])
    cp_cmd = " ".join(["cp", configs['date']+"__"+stgy+trial+"/"+load_config['pgm_name']+"/logs/*.log", dir_name+"/"])
    os.system(cp_cmd)
    bug_cp_cmd = " ".join(["mv", configs['date']+"__"+stgy+trial+"/"+load_config['pgm_name']+"/*/"+load_config['exec_dir']+"/*buginput*", bug_dir+"/"])
    os.system(bug_cp_cmd)
   
    # Accumulated the current knowledge G(heuristic -> covered_bset).
    G ={}
    for t in range(int(trial), int(trial)+1):
        log_dir = configs['date']+"__"+stgy+str(t)+"/"+load_config['pgm_name']+"/logs/"
        os.chdir(configs['ex_dir']+"/"+log_dir)
        files = glob.glob("*" + "_"+stgy+"_" + "*")
        b_set = []
        for f in files:
            with open(f) as fp:
                heuristic = str(t)+"_weights/"+(f.split(".log")[0]).split("__")[2]+".w"
                lines = fp.readlines()
                if len(lines) == 5:
                    b_set = lines[-2].split(':')
                    covered_bset = set(b_set[1].split())
                    G[heuristic] = covered_bset 
    
    return G


def SELECT(knowledge):
    global load_config
    os.chdir(configs['script_path'])
    
    temp_knowledge = deepcopy(knowledge)
    # The number of selected good(bad) heuristics from knowledge K. (n2) 
    selection_size = max(int(ratio*len(temp_knowledge)), parent_size)
    
    # greedy algorithm for solving the maximum coverage problem(MCP). 
    topk_hlist = []
    intersect_set = set()
    for i in range(1, selection_size+1):
        sorted_list = sorted(temp_knowledge.items(), key=lambda kv:(0.95*len(kv[1]) + 0.05*len(knowledge[kv[0]])), reverse = True)
        topk_h = sorted_list[0][0]
        topk_hset = sorted_list[0][1]

        if len(topk_hset) > 0: 
            topk_hlist.append(topk_h)
            intersect_set = intersect_set | topk_hset 
            for key in temp_knowledge.keys():
                temp_knowledge[key] = temp_knowledge[key] - intersect_set
    
    with open(load_config['pgm_name']+"_ratio", 'a') as rf:
        rf.write("topk("+str(selection_size)+"):"+str(len(intersect_set))+"\n") 

    temp_knowledge = deepcopy(knowledge)
   
    botk_hlist = []
    intersect_set = set()
    if len(temp_knowledge) < selection_size:
        selection_size=len(temp_knowledge)

    for i in range(1, selection_size+1):
        sorted_list = sorted(temp_knowledge.items(), key=lambda kv:(0.95*len(kv[1]) + 0.05*len(knowledge[kv[0]])), reverse = False)
        botk_h = sorted_list[0][0]
        botk_hset = sorted_list[0][1]
        
        botk_hlist.append(botk_h)
        intersect_set = intersect_set | botk_hset 
        del temp_knowledge[botk_h]
        for key in temp_knowledge.keys():
            temp_knowledge[key] = temp_knowledge[key]- intersect_set
    
    with open(load_config['pgm_name']+"_ratio", 'a') as rf:
        rf.write("botk("+str(selection_size)+"):"+str(len(intersect_set))+"\n") 
   
    return topk_hlist, botk_hlist


def Feature_Selection(topk_hlist, botk_hlist, trial):
    os.chdir(configs['script_path'])
    
    gsample_space_list = [[] for l in range(feat_num)]
    bsample_space_list = [[] for l in range(feat_num)]
    
    for bot_h, top_h in zip(botk_hlist, topk_hlist):
        with open(bot_h, 'r') as bf, open(top_h, 'r') as tf:
            itr=0
            botw_list = bf.read().splitlines()
            for w_idx in botw_list:
                bsample_space_list[itr].append(float(w_idx))
                itr = itr+1
            
            itr=0
            topw_list = tf.read().splitlines()
            for w_idx in topw_list:
                gsample_space_list[itr].append(float(w_idx))
                itr = itr+1
 
    rf_dic = {} 
    nth_feat = 1
    for lb, lg in zip(bsample_space_list, gsample_space_list):
        bad_mu = np.mean(lb)
        bad_sigma = np.std(lb)
        good_mu = np.mean(lg)
        good_sigma = np.std(lg)

        similarity = abs(bad_mu - good_mu) + abs(bad_sigma - good_sigma)
        if similarity <= 1.0:
            rf_dic[nth_feat] = similarity
        nth_feat = nth_feat+1        
   
    if rf_dic:
        most_rfeat_list = sorted(rf_dic.items(), key=lambda kv: kv[1], reverse = False)
        redundant_feat= [e[0] for e in most_rfeat_list]
    else:
        redundant_feat = []

   
    return redundant_feat 


def SWITCH(topk_hlist, botk_hlist, trial):
    # Filter Redundant Features
    redundant_features = Feature_Selection(topk_hlist, botk_hlist, trial) 
    
    os.chdir(configs['script_path'])
    
    # Collect each ith feature weight of bottom search heuristics.
    IthFeat_BotHeuristics = [[] for l in range(feat_num)]
    for bot_h in botk_hlist:
        with open(bot_h, 'r') as bf:
            feature_wlist = bf.read().splitlines()
            ith_feat=0
            for feature_w in feature_wlist:
                IthFeat_BotHeuristics[ith_feat].append(float(feature_w))
                ith_feat = ith_feat+1
    
    # Sample ith feature weights from the truncated normal distribution for bottom heuristics.   
    Bottom_Sample = [[] for l in range(feat_num)]
    ith_feat = 0
    for l in IthFeat_BotHeuristics:
        mu = np.mean(l)
        sigma = np.std(l)
        X = stats.truncnorm((lower - mu) / sigma, (upper - mu) / sigma, loc=mu, scale=sigma)
        Bottom_Sample[ith_feat] = [int(e) for e in (X.rvs(pool_size)).tolist()]
        ith_feat = ith_feat+1        
   
    New_Sample = [[] for l in range(feat_num)]

    # Use the most effective heuristics first for switching. (e.g., top-1, top-2, ... ) 
    topk_hlist = topk_hlist[:parent_size] 
    for top_h in topk_hlist:
        with open(top_h, 'r') as tf:
            feature_wlist = tf.read().splitlines()
            ith_feat=0
            for feature_w in feature_wlist:
                # Check whether the feature is redundant.
                if ith_feat+1 not in redundant_features:
                    mu = float(feature_w)
                    sigma = 1.0
                    X = stats.truncnorm((lower - mu) / sigma, (upper - mu) / sigma, loc=mu, scale=sigma)
                    predicate = 1
                    while predicate == 1:
                        Top_Sample = (X.rvs(pool_size)).tolist()
                        T_Sample = deepcopy(Top_Sample)
                        B_Sample = deepcopy(Bottom_Sample[ith_feat])
                        # Generate new heuristics that are similar topk_heuristic but dissimilar bot_heuristics. 
                        for e in Top_Sample:
                            if int(e) in B_Sample:
                                T_Sample.remove(e)
                                B_Sample.remove(int(e))
                        if len(T_Sample) >= offspring_size:
                            random.shuffle(T_Sample)
                            predicate = 0
                else:
                    T_Sample=[str(0.0) for _ in range(offspring_size)]

                New_Sample[ith_feat] = New_Sample[ith_feat]+T_Sample[:offspring_size]
                ith_feat = ith_feat+1        
    
    Next_H = str(trial+1)+"_weights/"
    if not os.path.exists(Next_H):
        os.mkdir(Next_H)
    
    # Exploitation (80%) 
    for ith_featw_list in New_Sample:
        idx = 1 
        for featw in ith_featw_list:
            fname = Next_H + str(idx) + ".w"
            with open(fname, 'a') as f:
                f.write(str(featw)+"\n")
                idx = idx+1
    
    # Exploration (20%)
    for idx in range(len(topk_hlist)*offspring_size, pool_size+1):
        rand_h = [str(random.uniform(-1, 1)) for _ in range(feat_num)]
        fname = Next_H + str(idx) + ".w"
        with open(fname, 'w') as f:
            for ith_featw in rand_h:
                f.write(str(ith_featw) + "\n")


def REFINE(K, G, parent_hlist, trial):
    os.chdir(configs['script_path'])
    # Calculate a set of covered branches in the previous knowledge K. 
    prev_set = set()
    for h in K.keys(): 
        prev_set = prev_set | K[h]
   
    # Evaluate the performance of offspring generated from each parent heuristic.
    Offspring_Performance = {}
    for i in range(0, len(parent_hlist)):
        offspring_set = set()
        uniq_branch_num = 0
        for w_num in range(i*offspring_size+1, (i+1)*offspring_size+1):
            h = str(trial)+"_weights/"+str(w_num)+".w"
            if h in G.keys():
                offspring_set = offspring_set | G[h]
        
        uniq_branch_num = len(offspring_set - prev_set)
        if uniq_branch_num > 0:
            Offspring_Performance[parent_hlist[i]] = uniq_branch_num    
    
    # Find top-k good parent heuristics in terms of unique coverage (k = 50%) 
    refine_size = int(round(refine_ratio*len(parent_hlist)))
    if len(Offspring_Performance) < refine_size:
        refine_size = len(Offspring_Performance) 
    
    GoodParent_Heuristics = sorted(Offspring_Performance.items(), key=lambda kv: kv[1], reverse = True)
    goodparent_hlist = []
    for l in GoodParent_Heuristics[:refine_size]:
        goodparent_hlist.append(l[0])
    
    # Delete the bad parent heuristics, but save good parent heuristics.
    K.update(G) # (K U G)
    
    for parent_h in parent_hlist:
        if parent_h in K.keys() and parent_h not in goodparent_hlist: 
            del K[parent_h] # (K U G) \ Kill
    
    return K
   
    
def clean():
    global load_config
    os.chdir(configs['ex_dir'])
    
    dir_name = configs['date']+"__"+load_config['pgm_name']+"__"+stgy
    bug_dir = dir_name+"/"+"buginputs"
    cp_cmd = " ".join(["mv", configs['date']+"__"+stgy+"*/"+load_config['pgm_name']+"/logs/*.log", dir_name+"/"])
    os.system(cp_cmd)
    bug_cp_cmd = " ".join(["mv", configs['date']+"__"+stgy+"*/"+load_config['pgm_name']+"/*/"+load_config['exec_dir']+"/*buginput*", bug_dir+"/"])
    os.system(bug_cp_cmd)
    
    rm_cmd = " ".join(["rm", "-rf", configs['date']+"__"+stgy+"*"])
    os.system(rm_cmd)
    
    os.chdir(configs['script_path'])
    dir_name = configs['ex_dir']+"/"+configs['date']+"__"+load_config['pgm_name']+"__"+stgy
    mv_cmd = " ".join(["mv", "*_weights", "bad*", load_config['pgm_name']+"*", dir_name])
    os.system(mv_cmd)
    
    rm_cmd = " ".join(["rm", bug_dir+"/*run_crest*"])
    os.system(rm_cmd)


def main():
    global load_config
    
    parser = argparse.ArgumentParser()
    parser.add_argument("pgm_config")
    parser.add_argument("time_budget")
    parser.add_argument("core_num")

    args = parser.parse_args()
    pgm_config = args.pgm_config
    time_budget = int(args.time_budget)
    core_num = args.core_num
    load_config = load_pgm_config(args.pgm_config)
    
    K = {}
    H = Gen_Heuristics("1")
    # Repeat the process until a given time budget is exhausted.
    for trial in range(1, 1000):
        G = Concolic(pgm_config, time_budget, core_num, str(trial))
        
        # Refine the knowledge.
        if not K: # Check whether the knowledge K is empty.
            K = G  
        else:
            K = REFINE(K, G, top_H[:parent_size], trial)
        
        # Select topk_ratio and botk_ratio heuristics.  
        top_H, bot_H = SELECT(K)
            
        # Learn new search heuristics based on the distributions of top and bottom heuristics.
        SWITCH(top_H, bot_H, trial)
    
    # clean_dir 
    clean()


if __name__ == '__main__':
    main()
