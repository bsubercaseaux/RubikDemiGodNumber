# Repository experiments related to Demi God numbers in the Rubiks cube

We use the solver from Elias Frantar (et al.): https://github.com/efrantar/rob-twophase

To compile simply run `make`.

To solve <N> cubes sampled uniformly at random, using at most <m> milliseconds per cube and <t> threads, run
```
./twophase -n <N> -m <m> -t <t>
```

This will result in a text file `sol.cubes`, containing stastitics and information for verification.

The description of each scramble follows Kociemba's representation, as described in https://github.com/efrantar/rob-twophase/blob/master/src/face.h 

The files `experiments_out_<n>.txt` (<n> in 1..10) contain the information corresponding to the 500.000 random solves used in the paper. The results have been split into 10 files to comply with technical requirements of Github anonimization, that admits files of at most 8mb.
