import os, sys, glob, argparse

def print_branches(stgy, path):
    os.chdir(path)

    [learn_set, rr_set, paradyse_set, cfds_set, cgs_set, gen_set, random_set] = [set([]), set([]), set([]), set([]), set([]), set([]), set([])]
    stgys = {"Chameleon":learn_set, "RoundRobin":rr_set, "CFDS":cfds_set, "CGS":cgs_set, "Param":paradyse_set, "Gen":gen_set, "Random":random_set}

    files = glob.glob("*" +stgy+ "*")
    b_set = []
    n =0 
    for f in files:
        with open(f) as fp:
            lines = fp.readlines()
            if len(lines)==5:
                line = lines[-2]
                b_set = line.split(':')
                n = n+1
                learn_set = learn_set.union(set(b_set[1].split()))
    print "# of covered branches by "+stgy+": "  +str(len(learn_set)) + " (# of trials: " + str(n) + ")"


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument("stgy")
    parser.add_argument("path")

    args = parser.parse_args()
    stgy = args.stgy
    path = args.path

    print_branches(stgy, path)

if __name__ == '__main__':
    main()

