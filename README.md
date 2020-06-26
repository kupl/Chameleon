# Chameleon   
<img src= "./img/chameleon_logo.jpg" width="200" height="200">       

**Chameleon** is a tool 
that performs concolic testing with adaptively changing search heuristics.
The tool is implemented on top of [CREST][crest] and [ParaDySE][paradyse].
CREST is a publicly available concolic testing tool for C and 
ParaDySE provides a publicly available implementation of parametric search heuristic for concolic testing.

#### Table of Contents

* [Installation](#Installation)
  * [Install Chameleon.](#Install-Chameleon.)
  * [Compile each benchmark with Chameleon.](#Compile-each-benchmark-with-Chameleon.)
* [Chameleon](#Chameleon)
  * [Run Chameleon.](#Run-Chameleon.)
  * [Calculate branch coverage.](#Calculate-branch-coverage.)
  * [Reproduce bug-triggering inputs.](#Reproduce-bug-triggering-inputs.)

## Installation
### Install Chameleon.
You need to install [Ubuntu 16.04.5(64 bit)][ubuntu] or [Ubuntu 14.04.1(64 bit)][ubuntu14]. 
We use **gcc-4.8** and **g++-4.8**. Then follow the steps:
```sh
$ sudo apt-get install ocaml python-numpy python-scipy #(if not installed) 
$ git clone https://github.com/kupl/Chameleon.git
$ cd Chameleon/cil
$ ./configure
$ make
$ cd ../src
$ make
```

### Compile each benchmark with Chameleon.
Please read **README\_Chameleon** file located in each benchmark directory. 
In **README\_Chameleon** file, we explain how to compile each benchmark.
For instance, we can compile sed-1.17 as follows:
```sh
$ cd Chameleon/benchmarks/sed-1.17 
# #(vi README_Chameleon)
$ make
```

## Chameleon

### Run Chameleon.
Chameleon, a tool for peforming concolic testing with adaptively changing search heuristics online, 
is run on an instrumented program. 
For instance, we can run our tool for **grep-2.2** as follows:
```sh
$ screen 
# Initially, each benchmark should be compiled with Chameleon:
$ cd Chameleon/benchmarks/grep-2.2
$ ./configure
$ cd src
$ make
# Run Chameleon
$ cd Chameleon/scripts
$ python Chameleon.py pgm_config/grep.json 3600 10 
```

Each argument of the last command means:
-	**pgm_config/grep.json** : A json file to describe the benchmark
-	**3600** : Time budget (sec)
-	**10** : The number of cpu cores to use in parallel

If Chameleon successfully ends, you can see the following command:
```sh
#############################################
################Time Out!!!!!################
#############################################
```
### Calculate branch coverage. 
When the testing budget is exhausted, you can calculate the number of covered branches as follows:
```sh
$ cd Chameleon/scripts
$ python print_coverage.py Chameleon ../experiments/0225__grep-2.2__Chameleon/
$ # of covered branches by Chameleon: 2245 (# of trials: 900)
```

Each argument of the last command means:
-	**Chameleon** : Search heuristic (or Chameleon) 
-	**../experiments/0225__grep-2.2__Chameleon/** : The directory where coverage logs are stored.


### Reproduce bug-triggering inputs. 
You can also check whether the found bug-triggering inputs are reproducible as follows:
```sh
$ cd Chameleon/scripts/bug_check/
$ screen 
$ python3 bugchecker.py ./bin/grep-2.2 ../../experiments/0225__grep-2.2__Chameleon/buginputs/ 

--------------------------Summary---------------------------
Tool(Heuristic): Chameleon,   Binary Version: grep-2.2
# of total bug-triggering inputs:  17
  - # of Performance Bug: 4
  - # of Segmentation-Fault: 13
  - # of Abnormal-Termination: 0
------------------------------------------------------------
```

Each argument of the last command means:
-	**./bin/grep-2.2** : The original binary for checking for the bug-triggering inputs.
-	**../../experiments/0225__grep-2.2__Chameleon/buginputs/** : The directory where bug-triggering inputs are stored.


We run **Chameleon** on a linux machine with two Intel Xeon Processor E5-2630 and 192GB RAM.

[crest]: https://github.com/jburnim/crest
[paradyse]: https://github.com/kupl/ParaDySE
[ubuntu]: https://www.ubuntu.com/download/alternative-downloads
[ubuntu14]: http://releases.ubuntu.com/?_ga=2.205149586.387782265.1566189396-1671541918.1565340236
