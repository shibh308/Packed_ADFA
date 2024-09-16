#include <iostream>
#include <cassert>
#include <cxxabi.h>
#include "utils.hpp"
#include "trie.hpp"


template <typename, typename = std::void_t<>>
struct has_memory_usage : std::false_type {};
template <typename T>
struct has_memory_usage<T, std::void_t<decltype(std::declval<T>().memory_usage())>> : std::true_type {};

template <typename T, std::enable_if_t<has_memory_usage<T>::value, int> = 0>
std::size_t call_memory_usage(T& obj) {
  std::clog << "memory usage: " << obj.memory_usage() / 1024.0 << "[KiB]" << std::endl;
  return obj.memory_usage();
}

template <typename T, std::enable_if_t<!has_memory_usage<T>::value, int> = 0>
std::size_t call_memory_usage(T&) {
  return 0;
}

template<typename Index> requires std::is_base_of_v<PatternMatcingIndex, Index>
void benchmark_search(const Index& index, const Strings& positive, const Strings& negative, ResultCsvWriter& writer){
  // compute time
  std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
  for(auto& pattern : positive){
    bool res = index.search(pattern);
    assert(res);
  }
  for(auto& pattern : negative){
    bool res = index.search(pattern);
    assert(!res);
  }
  std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
  // nanoseconds
  std::size_t nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  // output type of Index
  std::string method = abi::__cxa_demangle(typeid(index).name(), 0, 0, nullptr);
  std::clog << "Type: " << method << std::endl;
  std::clog << "Time: " << nanoseconds / 1e9 << " seconds." << std::endl;
  std::size_t memory_usage = call_memory_usage(index);
  std::clog << std::endl;
  writer.write(method, nanoseconds, memory_usage);
}


int main(int argc, char** argv){

  if(argc == 1){
    std::clog << "Usage: " << argv[0] << " [dataset_name] [dataset_size (optional)]" << std::endl;
    return 0;
  }
  std::string dataset_name = argv[1];
  int dataset_size = 1e9;
  if(argc >= 3){
    dataset_size = std::stoi(argv[2]);
  }
  auto data = load_dataset(dataset_name, dataset_size);
  auto [positive, negative] = split_data(data, 0.0);

  std::size_t total_length = 0;
  for(auto& pattern : positive){
    total_length += pattern.size();
  }
  ResultCsvWriter writer(dataset_name, positive.size(), total_length);

  BaseTrie trie(positive);
  trie.print_stats();
  benchmark_search(trie, positive, negative, writer);

  [&](){
    DoubleArrayTrie datrie(trie);
    benchmark_search(datrie, positive, negative, writer);
  }();

  [&](){
    BinarySearchTrie bstrie(trie);
    benchmark_search(bstrie, positive, negative, writer);
  }();

  [&](){
    TailTrie ttrie(trie);
    benchmark_search(ttrie, positive, negative, writer);
    [&](){
      TailDoubleArrayTrie tdatrie(ttrie);
      benchmark_search(tdatrie, positive, negative, writer);
    }();
    [&](){
      TailBinarySearchTrie tbstrie(ttrie);
      benchmark_search(tbstrie, positive, negative, writer);
    }();
  }();

  [&](){
    PathDecomposedTrie pdtrie(trie);
    benchmark_search(pdtrie, positive, negative, writer);
    [&](){
      PathDecomposedDoubleArrayTrie pddatrie(pdtrie);
      benchmark_search(pddatrie, positive, negative, writer);
    }();
    [&](){
      PathDecomposedBinarySearchTrie pdbstrie(pdtrie);
      benchmark_search(pdbstrie, positive, negative, writer);
    }();
  }();

  [&](){
    BaseADFA adfa(trie);
    adfa.print_stats();
    benchmark_search(adfa, positive, negative, writer);
    [&]() {
      DoubleArrayADFA daadfa(adfa);
      benchmark_search(daadfa, positive, negative, writer);
    }();
    [&]() {
      BinarySearchADFA bsadfa(adfa);
      benchmark_search(bsadfa, positive, negative, writer);
    }();
    [&]() {
      PathDecomposedADFA pdadfa(adfa);
      benchmark_search(pdadfa, positive, negative, writer);
      [&]() {
        PathDecomposedDoubleArrayADFA pddaadfa(pdadfa);
        benchmark_search(pddaadfa, positive, negative, writer);
      }();
      [&]() {
        PathDecomposedBinarySearchADFA pdbsadfa(pdadfa);
        benchmark_search(pdbsadfa, positive, negative, writer);
      }();
    }();
  }();

  return 0;
}
