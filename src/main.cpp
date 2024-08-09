#include <algorithm>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <vector>
#include <numeric>
#include <csignal>
#include <cstdlib>
#include <cassert>

#include "cubie.h"
#include "coord.h"
#include "face.h"
#include "move.h"
#include "prun.h"
#include "solve.h"
#include "sym.h"

const std::string BENCH_FILE = "bench.cubes";
const std::string SOL_FILE = "sol.cubes";

auto initial_tick = std::chrono::high_resolution_clock::now();
std::vector<cubie::cube> cubes;

std::vector<std::vector<int>> sols;
std::vector<int> times;

int freq[100];

int partial_sum = 0;
int solve_cnt = 0;

double mean(const std::vector<std::vector<int>> &sols, int (*len)(const std::vector<int> &))
{
  double total = 0;
  for (auto &sol : sols)
    total += len(sol);
  return total / sols.size();
}

void signal_handler(int signal_num)
{

  std::cout << std::endl
            << "Interrupted! Saving progress ..." << std::endl;

  // Save progress


  auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::high_resolution_clock::now() - initial_tick)
                        .count() /
                    1000000.;

  std::ofstream outFile(SOL_FILE);

  outFile << "Total time: " << total_time << "s" << std::endl;

  outFile << "Avg. Time: " << std::accumulate(times.begin(), times.end(), 0.) / times.size() << " ms" << std::endl;
  outFile << "Avg. Moves: " << mean(sols, move::len_ht) << " (HT) " << std::endl;

  int min = 100;
  int max = 0;
 

  for (auto &sol : sols)
  {
    freq[sol.size()]++;
    min = std::min(min, (int)sol.size()); // errors without casting ...
    max = std::max(max, (int)sol.size());
  }

  int total = 0;
  outFile << "Distribution:" << std::endl;
  for (int len = 1; len <= 30; len++) {
    outFile << len << ": " << freq[len] << std::endl;
    total += freq[len];
  }

  assert(total == sols.size());
  outFile << "Total solved: " << sols.size() << std::endl << std::endl;

  for (int i = 0; i < cubes.size(); i++)
  {
    outFile << "Cube " << i << ": ";
    outFile << face::from_cubie(cubes[i]) << std::endl;
    outFile << "Solution: ";
    for (int m : sols[i])
    {
      outFile << move::names[m] << " ";
    }
    outFile << std::endl
            << std::endl;
  }
  outFile.close();

  std::cout << "Progress saved!" << std::endl;
 

  // Terminate program
  exit(signal_num);
}

void usage()
{
  std::cout << "Usage: ./twophase "
            << "[-c] [-l MAX_LEN = 1] [-m MILLIS = 10] [-n N_SOLS = 1] [-s N_SPLITS = 1] [-t N_THREADS = 1] [-w N_WARMUPS = 0]"
            << std::endl;
  exit(1);
}

void init()
{
  auto tick = std::chrono::high_resolution_clock::now();
  std::cout << "Loading tables ..." << std::endl;

  face::init();
  move::init();
  coord::init();
  sym::init();
  if (prun::init(true))
  {
    std::cout << "Error." << std::endl;
    exit(1);
  }

  std::cout << "Done. " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - tick).count() / 1000. << "s" << std::endl
            << std::endl;
}

void warmup(solve::Engine &solver, int count)
{
  if (count == 0)
    return;

  std::cout << "Warming up ..." << std::endl;
  cubie::cube c;
  std::vector<std::vector<int>> sols;
  for (int i = 0; i < count; i++)
  {
    cubie::shuffle(c);
    solver.prepare();
    solver.solve(c, sols);
    solver.finish();
    std::cout << i << std::endl;
  }
  std::cout << "Done." << std::endl
            << std::endl;
}

bool check(const cubie::cube &c, const std::vector<int> &sol)
{
  cubie::cube c1;
  cubie::cube c2;

  c1 = c;
  for (int m : sol)
  {
    cubie::mul(c1, move::cubes[m], c2);
    std::swap(c1, c2);
  }

  return c1 == cubie::SOLVED_CUBE;
}



int main(int argc, char *argv[])
{

   // Register signal handlers
  signal(SIGINT, signal_handler);  // For Ctrl+C
  signal(SIGABRT, signal_handler); // For abort()
  signal(SIGSEGV, signal_handler); // For segmentation fault


  int n_threads = 1;
  int tlim = 10;
  int n_sols = 1;
  int max_len = -1;
  int n_splits = 1;
  bool compress = false;
  int n_warmups = 0;

  for(int i = 0; i < 100; ++i) {
    freq[i] = 0;
  }

  try
  {
    int opt;
    while ((opt = getopt(argc, argv, "cl:m:n:s:t:w:")) != -1)
    {
      switch (opt)
      {
      case 'c':
        compress = true;
        break;
      case 'l':
        max_len = std::stoi(optarg);
        break;
      case 'm':
        tlim = std::stoi(optarg);
        break;
      case 'n':
        if ((n_sols = std::stoi(optarg)) <= 0)
        {
          std::cout << "Error: Number of solutions (-n) must be >= 1." << std::endl;
          return 1;
        }
        break;
      case 's':
        if ((n_splits = std::stoi(optarg)) <= 0)
        {
          std::cout << "Error: Number of job splits (-s) must be >= 1." << std::endl;
          return 1;
        }
        break;
      case 't':
        if ((n_threads = std::stoi(optarg)) <= 0)
        {
          std::cout << "Error: Number of solver threads (-t) must be >= 1." << std::endl;
          return 1;
        }
        break;
      case 'w':
        if ((n_warmups = std::stoi(optarg)) <= 0)
        {
          std::cout << "Error: Number of warmup solves (-w) must be >= 0." << std::endl;
          return 1;
        }
        break;
      default:
        usage();
      }
    }
  }
  catch (...)
  { // catch any integer conversion errors
    usage();
  }

  std::cout << "This is rob-twophase v2.0; copyright Elias Frantar 2020." << std::endl
            << std::endl;
  init();
  solve::Engine solver(n_threads, tlim, 1, max_len, n_splits);
  warmup(solver, n_warmups);

  solver.prepare();
  std::cout << "Ready!" << std::endl;

  try
  {

    int failed = 0;

    for (int i = 0; i < 2*n_sols; i++)
    {
      cubie::cube c;
      cubie::shuffle(c);
    

      solver.prepare();
      auto tick = std::chrono::high_resolution_clock::now();
      std::vector<std::vector<int>> tmp;
      solver.solve(c, tmp);
      times.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
                     std::chrono::high_resolution_clock::now() - tick)
                     .count() /
                 1000.);
      solver.finish();

      if (tmp.size() == 0 || !check(c, tmp[0]))
      {
        std::cout << "FAILURE: " << i << std::endl;
        std::cout << face::from_cubie(c) << std::endl;
        failed++;
      }
      else
      {
        cubes.push_back(c);
        sols.push_back(tmp[0]);
        partial_sum += tmp[0].size();
        solve_cnt++;
       
        std::cout << i+1 << ",  partial avg: " << double(partial_sum) / double(solve_cnt) << std::endl;
        if (solve_cnt == n_sols)
        {
          break;
        }
      }
    }
    signal_handler(0);
  }
  catch (...)
  { // any file reading errors
    std::cout << "Error." << std::endl;
  }

  solver.finish(); // clean exit

  return 0;
}
