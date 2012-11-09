/** @file Generic utility functions. */

#ifndef UTILS_H
#define UTILS_H

#include <climits>
#include <vector>
#include <string>

int str_rfind_any(std::string str, const char* chars);
bool str_starts_with(std::string str, std::string prefix);
void test();

int sum(std::vector<int> v);
int max(std::vector<int> v);
int min(std::vector<int> v);
int median(std::vector<int> v);
double avg(std::vector<int> v);

float sum(std::vector<float> v);
float max(std::vector<float> v);
float min(std::vector<float> v);


std::vector<int> slice(std::vector<int> v, int start, int end = INT_MAX);

const std::string get_environment_variable(const std::string& name,
                                           const std::string& def,
                                           bool force_refresh = false);


#endif //UTILS_H
