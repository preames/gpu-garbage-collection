#include <cstring>
#include <cstdio>
#include <cassert>

#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <vector>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "common.h"
#include "utils.h"

#include "gpugc.h"
#include "gpugc-internal.h"

using namespace std;

struct statistics {
  int count;
  int depth;
  //aligned per iteration
  vector<int> width;
  vector<int> edges_examined;
  //aligned by node outbound edge count
  struct refdist_t {
    vector<int> v; //0-1000
    map<int, int> m;//1000+ (sparse)
    refdist_t() 
      : v(1000,0) {}

    int& operator[](int cnt) {
      if( cnt < 1000 ) {
        return v[cnt];
      }
      if( m.count(cnt) == 0 ) {
        m[cnt] = 0;
      }
      return m[cnt];
    }
    struct redist_t& operator+=(const struct refdist_t& rhs) {
      for(int i = 0; i < 1000; i++) {
        v[i] += rhs.v[i];
      }
      for(map<int,int>::iterator itr=m.begin(); 
          itr != m.end(); itr++) {
        (*this)[itr->first] += itr->second;
      }
    }

  } refdist;
  statistics() {
    count = 0;
    depth = 0;

    width.reserve(10000);
    edges_examined.reserve(10000);

  }
};

inline int queue_length(uint32_t qs, uint32_t qe) {
  //handle the wrap around case
  return ((qe + QUEUE_SIZE - qs) % QUEUE_SIZE);
}


/** This function calculates statistics about the
    reachable portion of the reference graph.
    
    Note: This operates on a _assumed to be valid_
    reference graph and the queue.  It is the callers
    responsability to perform sanity checking!
    
    Note: This function has the side effect of doing a mark as well!
*/
statistics stats_reachable(struct refgraph_entry **q,
                           uint32_t& qs,
                           uint32_t& qe,
                           int max_width=INT_MAX) {

  statistics stats;
  DEBUGF("%d to %d\n", qs, qe);

  assert( qe >= qs );

  //struct timespec timestart, timeend;
  //clock_gettime(CLOCK_MONOTONIC, &timestart);

  //while not done
  while (qs != qe) {
    int width_this_round = 0;
    int edges_examined_this_round = 0;

    //emulate a fix degree of parallism
    //while not done and within available parallelism
    int num_items = queue_length(qs, qe);
    if( num_items > max_width ) {
      num_items = max_width;
    }
    for(int i = 0; i < num_items && qs != qe; i++) {
      struct refgraph_entry *re = q[qs++];
      DEBUGF("%p\n", re);
      if (qs == QUEUE_SIZE) qs = 0;
      if (re->num_edges & VISITED)
        continue;

      int num_edges = re->num_edges & 0x3fffffff;
      DEBUGF("Processing %p (object %p, %d refs) [Queue size %d]\n", re, re->object, num_edges, queue_length(qs, qe));
      //show_type(re->object);
      for (uint32_t i = 0; i < num_edges; i++) {
        edges_examined_this_round++;
        if (re->edges[i] != NULL) {
          DEBUGF("  Reference %d of %d: %p\n", i, re->num_edges, re->edges[i]);
          q[qe++] = re->edges[i];
          if (qe == qs) { 
            printf("queue full\n"); 
            abort();
          }
          if (qe == QUEUE_SIZE) {
            qe = 0;
          }
        }
      }
      re->num_edges |= VISITED;
      stats.count++;
      stats.refdist[num_edges]++;
      width_this_round++;
    }
    stats.depth++;
    if( stats.width.size() < 10000 ) {
      //we don't really need this data
      stats.width.push_back(width_this_round);
      stats.edges_examined.push_back(edges_examined_this_round);
    } else {
      static bool youre_kidding = true;
      if( youre_kidding) {
        youre_kidding = false;
        cout << "Analyzing heap with a depth > 10000" << endl;
      }
    }
  }

  assert( stats.width.size() == stats.depth);
  assert( stats.edges_examined.size() == stats.depth);


  //  clock_gettime(CLOCK_MONOTONIC, &timeend);


  //uint64_t timeElapsed = timespecDiff(&timeend, &timestart); //nanoseconds

  //printf("CPU Mark Execution time (ms): %f\n", timeElapsed * 1.0e-6f);
  return stats;
}

int print_single_file(ofstream& out, const statistics& stats) {
  out << "Live Nodes:" << stats.count << endl;
  out << "Max Depth:" << stats.depth << endl;
  out << "Avg Width:" << avg(stats.width) << endl;
  out << "Min Width:" << min(stats.width) << endl;
  out << "Max Width:" << max(stats.width) << endl;
  return 0;
}

void print_headers_for_summary(ofstream& stream) {
  vector<string> headers;
  headers.push_back("benchmark");
  headers.push_back("");
  headers.push_back("");
  headers.push_back("");
  headers.push_back("live nodes");
  headers.push_back("max-width = inf");
  headers.push_back("");
  headers.push_back("");
  headers.push_back("");
  headers.push_back("");
  headers.push_back("max-width = 1024*32");
  headers.push_back("");
  headers.push_back("");
  headers.push_back("");
  headers.push_back("");
  headers.push_back("max-width = 1024");
  headers.push_back("");
  headers.push_back("");
  headers.push_back("");
  headers.push_back("");
  headers.push_back("max-width = 512");
  headers.push_back("");
  headers.push_back("");
  headers.push_back("");
  headers.push_back("");
  headers.push_back("max-width = 128");
  headers.push_back("");
  headers.push_back("");
  headers.push_back("");
  headers.push_back("");
  for(int i = 0; i < headers.size(); i++) {
    if( i != 0 ) {
      stream << ", ";
    }
    stream << headers[i];
  }
  stream << endl;
  headers.clear();
  headers.push_back("family");
  headers.push_back("name");
  headers.push_back("size");
  headers.push_back("iteration");
  headers.push_back("");
  for(int i = 0; i < 5; i++) {
    headers.push_back("depth");
    headers.push_back("avg width");
    headers.push_back("med width");
    headers.push_back("min width");
    headers.push_back("max width");
  }
  for(int i = 0; i < headers.size(); i++) {
    if( i != 0 ) {
      stream << ", ";
    }
    stream << headers[i];
  }
  stream << endl;
}





struct on_exit_for_refdist_figure {
  //Sum across all GC in the benchmark
  statistics::refdist_t refdist;
  bool enable;
  std::string filename;
  on_exit_for_refdist_figure(){
    enable = false;
  }
  ~on_exit_for_refdist_figure() {
    if(!enable) return;

    ofstream stream(filename.c_str() );

    //Write the format description as a comment
    stream << 
      "# THIS FILE IS MACHINE GENERATED.  DO NOT EDIT\n"
      "# This is the raw data used by the reference distribution graphs\n"
      "# in the paper. (aka. outbound.plt).\n"
      "# \n"
      "# The format is:\n"
      "# column 1 - number of edges (starting at zero)\n" 
      "# column 2 - percentage of objects in this bucket\n"
      "# (space separated)\n"
           << endl;
    vector<vector<int> > width_cols;
     
    for(int i = 0; i < 1000; i++) {
      if( refdist.v[i] > 0 ) {
        stream << i << " " << refdist.v[i] << endl;
      }
    }
    for(map<int,int>::iterator itr=refdist.m.begin(); 
        itr != refdist.m.end(); itr++) {
      if( itr->first > 0 ) {
        stream << itr->first << " " << itr->second << endl;
      }
    }
  }
} g_on_exit_refdist;


void print_ref_distribution(const statistics& stats) {
  g_on_exit_refdist.enable = true;
  
  g_on_exit_refdist.refdist += stats.refdist;

  g_on_exit_refdist.filename = 
    get_environment_variable("gpugc_refdist_chart_file",
                             "refdist.dat");

}

#if 0
//TODO: Rewrite/reengineer for multiple widths
void print_gc_summary_for_benchmark(ofstream& stream, ..., bool detail) {
  if( !detail ) {
#if 0  
    stream << files[i].benchfam << ", ";
    stream << files[i].bench << ", ";
    stream << files[i].size << ", ";
    stream << files[i].gc << ", ";
#endif
    if( state.dumplines != 0 ) {
      int truecnt = 0;
      {
        const statistics stats = stats_reachable(state);
        truecnt = stats.count;
        stream << stats.count << ", ";
        stream << stats.depth << ", ";
        stream << avg(stats.width) << ", ";
        stream << median(stats.width) << ", ";
        stream << min(stats.width) << ", ";
        stream << max(stats.width) << ", ";
      }
      {
        const statistics stats = stats_reachable(state, 1024*32);
        assert( truecnt == stats.count);
        stream << stats.depth << ", ";
        stream << avg(stats.width) << ", ";
        stream << median(stats.width) << ", ";
        stream << min(stats.width) << ", ";
        stream << max(stats.width) << ", ";
      }
      {
        const statistics stats = stats_reachable(state, 1024);
        assert( truecnt == stats.count);
        stream << stats.depth << ", ";
        stream << avg(stats.width) << ", ";
        stream << median(stats.width) << ", ";
        stream << min(stats.width) << ", ";
        stream << max(stats.width) << ", ";
      }
      {
        const statistics stats = stats_reachable(state, 512);
        assert( truecnt == stats.count);
        stream << stats.depth << ", ";
        stream << avg(stats.width) << ", ";
        stream << median(stats.width) << ", ";
        stream << min(stats.width) << ", ";
        stream << max(stats.width) << ", ";
      }
      {
        const statistics stats = stats_reachable(state, 128);
        assert( truecnt == stats.count);
        stream << stats.depth << ", ";
        stream << avg(stats.width) << ", ";
        stream << median(stats.width) << ", ";
        stream << min(stats.width) << ", ";
        stream << max(stats.width) << endl;
      }
    } else {
      stream << "N/A" << ", ";
      stream << "N/A" << ", ";
      stream << "N/A" << ", ";
      stream << "N/A" << ", ";
      stream << "N/A" << endl;
    }
  } else {
    if( state.dumplines != 0 ) {
      vector<vector<int> > width_cols;
      
      const statistics stats_inf = stats_reachable(state);
      const int truecnt = stats_inf.count;
      width_cols.push_back(stats_inf.width);
      width_cols.push_back(stats_inf.edges_examined);
      
      const statistics stats_1024_32 = stats_reachable(state, 1024*32);
      assert( truecnt == stats_1024_32.count);
      width_cols.push_back(stats_1024_32.width);
      width_cols.push_back(stats_1024_32.edges_examined);
      
      const statistics stats_1024 = stats_reachable(state, 1024);
      assert( truecnt == stats_1024.count);
      width_cols.push_back(stats_1024.width);
      width_cols.push_back(stats_1024.edges_examined);
      
      const statistics stats_512 = stats_reachable(state, 512);
      assert( truecnt == stats_512.count);
      width_cols.push_back(stats_512.width);
      width_cols.push_back(stats_512.edges_examined);
      
      const statistics stats_128 = stats_reachable(state, 128);
      assert( truecnt == stats_128.count);
      width_cols.push_back(stats_128.width);
      width_cols.push_back(stats_128.edges_examined);

      vector<int> v;
      for(int j = 0; j < width_cols.size(); j++) {
        v.push_back( width_cols[j].size() );
      }
      const int maxheight = max(v);

      for(int row = 0; row < maxheight; row++) {
#if 0
        stream << files[i].benchfam << ", ";
        stream << files[i].bench << ", ";
        stream << files[i].size << ", ";
        stream << files[i].gc << ", ";
#endif
        for(int col = 0; col < width_cols.size(); col++) {
          if( width_cols[col].size() > row ) {
            stream << width_cols[col][row];
          } else {
            //stream << ;
          }
          if( col != width_cols.size() -1 ) {
            stream << ",";
          }
          
        }
        stream << endl;
      }
    }      
  }
}
#endif

struct on_exit_for_dropoff_figure {
  //note that "best" is actually the deepest one here
  statistics beststats;
  bool enable;
  std::string filename;
  on_exit_for_dropoff_figure(){
    enable = false;
  }
  ~on_exit_for_dropoff_figure() {
    if(!enable) return;

    ofstream stream( filename.c_str() );

    //write the format as a header
    stream <<
      "# THIS FILE IS MACHINE GENERATED.  DO NOT EDIT\n"
      "# This data is for the graphs which show number of steps vs number\n"
      "# of edges reachable for each benchmark.  We present the deepest\n"
      "# graph out of all garbage collections.\n"
      "# \n"
      "# The format we feed to GNU plot is:\n"
      "# first column - step + 1\n"
      "# second column - edges in queue on that step\n"
      "# (columns are separated by spaces)\n"
           << endl;
                          
    for(int row = 0; row < beststats.edges_examined.size(); row++) {
      stream << row + 1 << " " << beststats.edges_examined[row] << endl;
    }
  }
} g_on_exit;

/** This function (and the handler above) handles the raw
    data for the dropoff graph (i.e. percent complete by
    edges) from the paper.*/
void print_data_for_dropoff_figure(const statistics& stats) {
  //find the deepest one (the actual print happens on process exit)
  if( !g_on_exit.enable || stats.depth > g_on_exit.beststats.depth ) {
    g_on_exit.enable = true;
    g_on_exit.beststats = stats;

    g_on_exit.filename = 
      get_environment_variable("gpugc_dropoff_chart_file",
                               "dropoff.dat");

  }

}

/** This function dumps the figures for the paper based on
    analysis of the reachable portion of the reference graph.
    
    Note: This operates on a _assumed to be valid_
    reference graph and the queue.  It is the callers
    responsability to perform sanity checking!
    
    Note: This function has the side effect of doing a mark as well!
*/
void analyze_heap_structure(struct refgraph_entry **q,
                            uint32_t& qs,
                            uint32_t& qe) {
  test();

  bool dump_detail = false;

  //Dump each type of file that we need for the report
  // and/or analysis purposes

  const statistics stats = stats_reachable(q, qs, qe);

  /*scope*/ {
    ofstream stream("sanity.txt");
    print_single_file(stream, stats);
  }
  /*scope*/ {
    // TODO: Across benchmarks
    print_ref_distribution(stats);
  }
  /*scope*/{
    //recorded for now, actually done at proc exit
    print_data_for_dropoff_figure(stats);
  }
  
  //TODO: To get this working correctly, need to get stats_reachable
  // callable for multiple times in one session.
  // TEST before playing with this though.
  if( 0 ) {


    ofstream stream(dump_detail ? "detail.csv" : "summary.csv");
    stream << "NOT IMPLEMENTED!" << endl;
    cout << "NOT IMPLEMENTED" << endl;
    
    if(!dump_detail == true) {
      print_headers_for_summary(stream);
    }
    //print_gc_summary_for_benchmark(stream, ... , dump_detail);
  }


}

