
#include <cstring>
#include <cstdio>
#include <cassert>

#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <algorithm>
#include "utils.h"

using namespace std;

int str_rfind_any(string str, const char* chars) {
  size_t rval = string::npos;
  assert( chars );
  while( *chars != '\0' ) {
    const size_t pos = str.rfind(*chars, rval);
    if( pos != string::npos ) {
      rval = pos;
    }
    chars++;
  }
  return rval;
}

bool str_starts_with(string str, string prefix) {
  int i;
  for(i = 0; i < str.size() && i < prefix.size(); i++) {
    if( str[i] != prefix[i] ) {
      return false;
    }
  }
  return (i == prefix.size());
}

void test() {
  assert( str_starts_with("FOO abs", "FOO") );
  assert( str_starts_with("FOO", "FOO") );
  assert( str_starts_with("HEAP DUMP BEGIN", "HEAP DUMP BEGIN") );
}

int sum(vector<int> v) {
  assert( !v.empty() );
  int sum = v[0];
  for(int i = 1; i < v.size(); i++) {
    sum += v[i];
  }
  return sum;
}

int max(vector<int> v) {
  assert( !v.empty() );
  int highest = v[0];
  for(int i = 0; i < v.size(); i++) {
    if( v[i] > highest ) {
      highest = v[i];
    }
  }
  return highest;
}

int min(vector<int> v) {
  assert( !v.empty() );
  int lowest = v[0];
  for(int i = 0; i < v.size(); i++) {
    if( v[i] < lowest ) {
      lowest = v[i];
    }
  }
  return lowest;
}
int median(std::vector<int> v) {
  assert( !v.empty() );
  sort(v.begin(), v.end());
  const int mid = v.size()/2 + 1;
  assert( 0 <= mid && mid < v.size() );
  return v[mid];
}
double avg(vector<int> v) {
  assert( !v.empty() );
  double rval = 0;
  for(int i = 0; i < v.size(); i++) {
    rval += v[i];
  }
  rval = rval/v.size();
  return rval;
}

float sum(vector<float> v) {
  assert( !v.empty() );
  float sum = v[0];
  for(int i = 1; i < v.size(); i++) {
    sum += v[i];
  }
  return sum;
}

float max(vector<float> v) {
  assert( !v.empty() );
  float highest = v[0];
  for(int i = 0; i < v.size(); i++) {
    if( v[i] > highest ) {
      highest = v[i];
    }
  }
  return highest;
}

float min(vector<float> v) {
  assert( !v.empty() );
  float lowest = v[0];
  for(int i = 0; i < v.size(); i++) {
    if( v[i] < lowest ) {
      lowest = v[i];
    }
  }
  return lowest;
}



vector<int> slice(vector<int> v, int start, int end) {
  vector<int> rval;
  if( end > v.size() ) {
    end = v.size();
  }
  rval.reserve(end-start);
  for(int i = start; i < end; i++) {
    rval.push_back(v[i]);
  }
  return rval;
}


#include <cstdlib>
#include <map>
#include <string>
namespace {
  std::map<std::string, std::string> g_cached_env_values;

}

const std::string get_environment_variable(const std::string& name,
                                           const std::string& def,
                                           bool force_refresh) {
  if( force_refresh || 0 == g_cached_env_values.count( name ) ) {
    const char* ptr = getenv(name.c_str());
    g_cached_env_values[name] = (ptr ? std::string(ptr) : def);
  }

  return g_cached_env_values[name];
}
