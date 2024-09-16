//
// Created by shibh308 on 2024/09/12.
//

#ifndef PACKED_ADFA_TRIE_HPP
#define PACKED_ADFA_TRIE_HPP

#include "utils.hpp"
#include <unordered_map>
#include <map>
#include "sdsl/bit_vectors.hpp"


class PatternMatcingIndex{
  virtual bool search(const String& line) const = 0;
};

// a simple trie that supports dynamic insertion
class BaseTrie : public PatternMatcingIndex{
  int node_count = 1;
  MapVector<STLMap> maps;
public:
  explicit BaseTrie(const Strings& data) : maps(1){
    for(auto line : data){
      insert(line);
    }
  }
  void insert(const String& line){
    Index node = 0;
    for(auto ch: line){
      Index child = maps.search(node, ch);
      if(child == NOT_FOUND){
        child = node_count;
        maps.extend(++node_count);
        maps.insert(node, ch, child);
      }
      node = child;
    }
  }
  bool search(const String& line) const override{
    Index node = 0;
    for(auto ch : line){
      node = maps.search(node, ch);
      if(node == NOT_FOUND){
        return false;
      }
    }
    return maps.outdegree(node) == 0;
  }
  std::vector<std::vector<std::pair<Char, Index>>> to_vector() const{
    return maps.to_vector();
  }
  void print_stats() const{
    std::clog << "--------------------------------" << std::endl;
    std::clog << "node count: " << node_count << std::endl;
    std::size_t edge_count = 0;
    std::vector<std::vector<std::pair<Char, Index>>> data = to_vector();
    for(auto& edges : data){
      edge_count += edges.size();
    }
    std::clog << "edge count: " << edge_count << std::endl;
    std::clog << "--------------------------------" << std::endl;
  }
};

// a static trie that uses binary search
class BinarySearchTrie : public PatternMatcingIndex {
  sdsl::bit_vector is_leaf;
  BinarySearchMaps maps;
public:
  explicit BinarySearchTrie(const BaseTrie& base) : maps(){
    auto data = base.to_vector();
    is_leaf.resize(data.size());
    for(Index i = 0; i < data.size(); ++i){
      if(data[i].empty()){
        is_leaf[i] = true;
      }
    }
    maps = BinarySearchMaps::static_construct(data);
    maps.reset_bv();
  }
  bool search(const String& line) const override{
    Index node = 0;
    for(auto ch : line){
      node = maps.search(node, ch);
      if(node == NOT_FOUND){
        return false;
      }
    }
    return is_leaf[node];
  }
  std::size_t memory_usage() const{
    std::size_t memory = is_leaf.size() / 8
                         + (sizeof(Char) + sizeof(Index) + 1) * maps.size();
    return memory;
  }
};

// a static trie that uses double array
class DoubleArrayTrie : public PatternMatcingIndex{
  sdsl::bit_vector is_leaf;
  DoubleArrayMaps maps;
public:
  explicit DoubleArrayTrie(const BaseTrie& base) : maps(0){
    std::vector<std::vector<std::pair<Char, Index>>> data = base.to_vector();
    auto [da, cor] = DoubleArrayMaps::construct_with_reindexing(data);
    is_leaf.resize(da.next.size());
    assert(cor[0] == 0);
    for(Index i = 0; i < data.size(); ++i){
      if(data[i].empty()){
        is_leaf[cor[i]] = true;
      }
    }
    maps = std::move(da);
  }
  bool search(const String& line) const override{
    Index node = 0;
    for(auto ch : line){
      node = maps.search(node, ch);
      if(node == NOT_FOUND){
        return false;
      }
    }
    return is_leaf[node];
  }
  std::size_t memory_usage() const{
    std::size_t memory = is_leaf.size() / 8
                         + (sizeof(Char) + sizeof(Index)) * maps.size();
    return memory;
  }
};

class TailTrie : public PatternMatcingIndex{
public:
  String tail_str;
  std::vector<Index> next;
  MapVector<STLMap> maps;
  explicit TailTrie(const BaseTrie& base) : maps(0){
    std::vector<std::vector<std::pair<Char, Index>>> data = base.to_vector();
    std::vector<int> number_of_paths_leaf(data.size(), 0);
    std::vector<Index> mapping(data.size(), NOT_FOUND);
    std::vector<std::vector<std::pair<Char, std::pair<bool, Index>>>> new_edges;
    std::vector<std::vector<std::pair<Char, Index>>> new_data;
    for(Index i = data.size() - 1; i >= 0; --i){
      if(data[i].empty()){
        number_of_paths_leaf[i] = 1;
      }
      for(Index j = 0; j < data[i].size(); ++j){
        auto [ch, to] = data[i][j];
        number_of_paths_leaf[i] += number_of_paths_leaf[to];
      }
    }
    for(Index i = 0; i < data.size(); ++i){
      if(number_of_paths_leaf[i] == 1){
        continue;
      }
      mapping[i] = new_edges.size();
      new_edges.emplace_back();
      for(auto [ch, to] : data[i]){
        if(number_of_paths_leaf[to] > 1){
          new_edges.back().emplace_back(ch, std::make_pair(false, to));
        }
        else{
          new_edges.back().emplace_back(ch, std::make_pair(true, tail_str.size()));
          tail_str.emplace_back(ch);
          Index cur = to;
          while(true){
            if(data[cur].empty()){
              break;
            }
            tail_str.emplace_back(data[cur].front().first);
            cur = data[cur].front().second;
          }
        }
      }
    }
    new_data.resize(new_edges.size());
    for(Index i = 0; i < new_edges.size(); ++i){
      for(auto& [ch, to] : new_edges[i]){
        if(!to.first){
          to.second = mapping[to.second];
        }
        new_data[i].emplace_back(ch, to.second | (to.first << 31));
      }
    }
    assert(number_of_paths_leaf[0] > 1);
    maps = construct_maps<MapVector<STLMap>>(new_data);
  }
  bool search(const String& line) const override{
    Index node = 0;
    for(Index i = 0; i < line.size(); ++i){
      node = maps.search(node, line[i]);
      if(node == NOT_FOUND){
        return false;
      }
      if(node & (1 << 31)){
        Index next = node & ~(1 << 31);
        return get_lcp(tail_str, next, line, i, line.size() - i) == line.size() - i;
      }
    }
    return true;
  }
};

class TailDoubleArrayTrie : public PatternMatcingIndex{
  String tail_str;
  std::vector<Index> next;
  DoubleArrayMaps maps;
public:
  explicit TailDoubleArrayTrie(const TailTrie &base) : maps(0){
    tail_str = base.tail_str;
    std::vector<std::vector<std::pair<Char, Index>>> light_edges = base.maps.to_vector();
    auto [da, cor] = DoubleArrayMaps::construct_without_reindexing(light_edges);
    maps = std::move(da);
    next = std::move(cor);
  }
  bool search(const String &line) const override{
    Index node = 0;
    for(Index i = 0; i < line.size(); ++i){
      node = maps.search(next[node], line[i]);
      if(node == NOT_FOUND){
        return false;
      }
      if(node & (1 << 31)){
        Index nex = node & ~(1 << 31);
        return get_lcp(tail_str, nex, line, i, line.size() - i) == line.size() - i;
      }
    }
    return true;
  }
  std::size_t memory_usage() const{
    std::size_t memory = sizeof(Index) * 1
                         + sizeof(Char) * tail_str.size()
                         + sizeof(Index) * next.size()
                         + (sizeof(Char) + sizeof(Index)) * maps.size();
    return memory;
  }
};

class TailBinarySearchTrie : public PatternMatcingIndex{
  String tail_str;
  std::vector<Index> next;
  BinarySearchMaps maps;
public:
  explicit TailBinarySearchTrie(const TailTrie& base) : maps(){
    tail_str = base.tail_str;
    std::vector<std::vector<std::pair<Char, Index>>> light_edges = base.maps.to_vector();
    maps = BinarySearchMaps::static_construct(light_edges);
    maps.reset_bv();
  }
  bool search(const String& line) const override{
    Index node = 0;
    for(Index i = 0; i < line.size(); ++i){
      node = maps.search(node, line[i]);
      if(node == NOT_FOUND){
        return false;
      }
      if(node & (1 << 31)){
        Index next = node & ~(1 << 31);
        return get_lcp(tail_str, next, line, i, line.size() - i) == line.size() - i;
      }
    }
    return true;
  }
  std::size_t memory_usage() const{
    std::size_t memory = sizeof(Index) * 1
                         + sizeof(Char) * tail_str.size()
                         + (sizeof(Char) + sizeof(Index) + 1) * maps.size();
    return memory;
  }
};

class PathDecomposedTrie : public PatternMatcingIndex {
public:
  sdsl::bit_vector is_leaf;
  String heavy_str;
  MapVector<STLMap> maps;
  explicit PathDecomposedTrie(const BaseTrie& base) : maps(0){
    std::vector<std::vector<std::pair<Char, Index>>> data = base.to_vector();
    is_leaf.resize(data.size());
    std::vector<std::vector<std::pair<Char, Index>>> light_edges(data.size());
    std::vector<Index> heavy_edges(data.size(), NOT_FOUND);
    std::vector<int> number_of_paths_leaf(data.size(), 0);
    for(Index i = data.size() - 1; i >= 0; --i){
      if(data[i].empty()){
        number_of_paths_leaf[i] = 1;
      }
      for(Index j = 0; j < data[i].size(); ++j){
        auto [ch, to] = data[i][j];
        int siz = number_of_paths_leaf[to];
        if(heavy_edges[i] == NOT_FOUND){
          heavy_edges[i] = j;
        }
        else if(siz > number_of_paths_leaf[data[i][heavy_edges[i]].second]){
          light_edges[i].emplace_back(data[i][heavy_edges[i]]);
          heavy_edges[i] = j;
        }
        else{
          light_edges[i].emplace_back(ch, to);
        }
        number_of_paths_leaf[i] += number_of_paths_leaf[to];
      }
    }
    std::vector<Index> heavy_path;
    std::vector<bool> heavy_edges_flag(data.size(), false);
    heavy_str.reserve(data.size());
    for(Index i = 0; i < data.size(); ++i){
      if(heavy_edges_flag[i]){
        continue;
      }
      Index cur = i;
      while(true){
        heavy_path.emplace_back(cur);
        heavy_edges_flag[cur] = true;
        if(heavy_edges[cur] == NOT_FOUND){
          heavy_str.emplace_back(NULL_CHAR);
          break;
        }
        heavy_str.emplace_back(data[cur][heavy_edges[cur]].first);
        cur = data[cur][heavy_edges[cur]].second;
      }
    }
    std::vector<Index> heavy_path_inv(data.size());
    for(Index i = 0; i < heavy_path.size(); ++i){
      heavy_path_inv[heavy_path[i]] = i;
    }
    for(Index i = 0; i < data.size(); ++i){
      is_leaf[heavy_path_inv[i]] = data[i].empty();
    }
    std::vector<std::vector<std::pair<Char, Index>>> light_edges_inv(data.size());
    for(Index i = 0; i < data.size(); ++i){
      for(auto [ch, to] : light_edges[i]){
        light_edges_inv[heavy_path_inv[i]].emplace_back(ch, heavy_path_inv[to]);
      }
    }
    maps = construct_maps<MapVector<STLMap>>(light_edges_inv);
  }
  bool search(const String& line) const override{
    Index node = 0;
    for(Index i = 0; i < line.size(); ++i){
      Index lcp = get_lcp(heavy_str, node, line, i, line.size() - i);
      node += lcp;
      i += lcp;
      if(i == line.size()){
        break;
      }
      node = maps.search(node, line[i]);
      if(node == NOT_FOUND){
        return false;
      }
    }
    return is_leaf[node];
  }
};

class PathDecomposedDoubleArrayTrie : public PatternMatcingIndex{
  sdsl::bit_vector is_leaf;
  String heavy_str;
  std::vector<Index> next;
  DoubleArrayMaps maps;
public:
  explicit PathDecomposedDoubleArrayTrie(const PathDecomposedTrie &base) : maps(0){
    PathDecomposedTrie pdtrie(base);
    heavy_str = pdtrie.heavy_str;
    is_leaf = pdtrie.is_leaf;
    std::vector<std::vector<std::pair<Char, Index>>> light_edges = pdtrie.maps.to_vector();
    auto [da, cor] = DoubleArrayMaps::construct_without_reindexing(light_edges);
    maps = std::move(da);
    next = std::move(cor);
  }
  bool search(const String& line) const override{
    Index node = 0;
    for(Index i = 0; i < line.size(); ++i){
      Index lcp = get_lcp(heavy_str, node, line, i, line.size() - i);
      node += lcp;
      i += lcp;
      if(i == line.size()){
        break;
      }
      node = maps.search(next[node], line[i]);
      if(node == NOT_FOUND){
        return false;
      }
    }
    return is_leaf[node];
  }
  std::size_t memory_usage() const{
    std::size_t memory = sizeof(Index) * 2
                         + sizeof(Char) * heavy_str.size()
                         + sizeof(Index) * next.size()
                         + (sizeof(Char) + sizeof(Index)) * maps.size();
    return memory;
  }
};

class PathDecomposedBinarySearchTrie : public PatternMatcingIndex{
  sdsl::bit_vector is_leaf;
  String heavy_str;
  BinarySearchMaps maps;
public:
  explicit PathDecomposedBinarySearchTrie(const PathDecomposedTrie &padfa) : maps(){
    heavy_str = padfa.heavy_str;
    is_leaf = padfa.is_leaf;
    std::vector<std::vector<std::pair<Char, Index>>> light_edges = padfa.maps.to_vector();
    maps = BinarySearchMaps::static_construct(light_edges);
    maps.reset_bv();
  }
  bool search(const String& line) const override{
    Index node = 0;
    for(Index i = 0; i < line.size(); ++i){
      Index lcp = get_lcp(heavy_str, node, line, i, line.size() - i);
      node += lcp;
      i += lcp;
      if(i == line.size()){
        break;
      }
      node = maps.search(node, line[i]);
      if(node == NOT_FOUND){
        return false;
      }
    }
    return is_leaf[node];
  }
  std::size_t memory_usage() const{
    std::size_t memory = sizeof(Index) * 2
                         + sizeof(Char) * heavy_str.size()
                         + (sizeof(Char) + sizeof(Index) + 1) * maps.size();
    return memory;
  }
};

// a static ADFA
class BaseADFA : public PatternMatcingIndex {
  MapVector<STLMap> maps;
public:
  explicit BaseADFA(const BaseTrie& base) : maps(0){
    std::vector<std::vector<std::pair<Char, Index>>> data = base.to_vector();
    std::map<std::vector<std::pair<Char, Index>>, Index> id_map;
    std::vector<Index> ids(data.size(), NOT_FOUND);
    for(Index i = data.size() - 1; i >= 0; --i){
      std::vector<std::pair<Char, Index>> children;
      for(auto [ch, to] : data[i]){
        children.emplace_back(ch, ids[to]);
      }
      if(!id_map.contains(children)){
        id_map[children] = id_map.size();
      }
      ids[i] = id_map[children];
    }
    std::vector<std::pair<Index, std::vector<std::pair<Char, Index>>>> sorted_id_map(id_map.size());
    for(const auto& [children, id] : id_map){
      sorted_id_map[id] = {id, children};
    }
    maps.extend(sorted_id_map.size());
    for(const auto& [id, children] : sorted_id_map){
      Index after_id = sorted_id_map.size() - 1 - id;
      for(auto [ch, to] : children){
        Index after_to = sorted_id_map.size() - 1 - to;
        assert(after_id < after_to);
        maps.insert(after_id, ch, after_to);
      }
    }
  }
  bool search(const String& line) const override{
    Index node = 0;
    for(auto ch : line){
      node = maps.search(node, ch);
      if(node == NOT_FOUND){
        return false;
      }
    }
    return node == maps.size() - 1;
  }
  std::vector<std::vector<std::pair<Char, Index>>> to_vector() const{
    return maps.to_vector();
  }
  void print_stats() const{
    std::clog << "--------------------------------" << std::endl;
    std::vector<std::vector<std::pair<Char, Index>>> data = to_vector();
    std::clog << "node count: " << data.size() << std::endl;
    std::size_t edge_count = 0;
    for(auto& edges : data){
      edge_count += edges.size();
    }
    std::clog << "edge count: " << edge_count << std::endl;
    std::clog << "--------------------------------" << std::endl;
  }
};

// a static ADFA that uses binary search
class BinarySearchADFA : public PatternMatcingIndex {
  Index sink;
  BinarySearchMaps maps;
public:
  explicit BinarySearchADFA(const BaseADFA& base) : maps(){
    auto data = base.to_vector();
    sink = data.size() - 1;
    maps = BinarySearchMaps::static_construct(data);
    maps.reset_bv();
  }
  bool search(const String& line) const override{
    Index node = 0;
    for(auto ch : line){
      node = maps.search(node, ch);
      if(node == NOT_FOUND){
        return false;
      }
    }
    return node == sink;
  }
  std::size_t memory_usage() const{
    std::size_t memory = sizeof(Index) * 1
                         + (sizeof(Char) + sizeof(Index) + 1) * maps.size();
    return memory;
  }
};

// a static ADFA that uses double array
class DoubleArrayADFA : public PatternMatcingIndex {
  Index sink;
  DoubleArrayMaps maps;
public:
  explicit DoubleArrayADFA(const BaseADFA& base) : maps(0){
    std::vector<std::vector<std::pair<Char, Index>>> data = base.to_vector();
    auto [da, cor] = DoubleArrayMaps::construct_with_reindexing(data);
    assert(cor[0] == 0);
    sink = cor.back();
    maps = std::move(da);
  }
  bool search(const String& line) const override{
    Index node = 0;
    for(auto ch : line){
      node = maps.search(node, ch);
      if(node == NOT_FOUND){
        return false;
      }
    }
    return node == sink;
  }
  std::size_t memory_usage() const{
    std::size_t memory = sizeof(Index)
                      + (sizeof(Char) + sizeof(Index)) * maps.size();
    return memory;
  }
};

class PathDecomposedADFA : public PatternMatcingIndex {
public:
  Index root, sink;
  String heavy_str;
  MapVector<STLMap> maps;
  explicit PathDecomposedADFA(const BaseADFA& base) : maps(0){
    std::vector<std::vector<std::pair<Char, Index>>> data = base.to_vector();
    std::vector<std::vector<std::tuple<Char, Index, bool>>> data_with_heavy_flag(data.size());
    for(Index i = 0; i < data.size(); ++i){
      data_with_heavy_flag[i].resize(data[i].size());
      for(Index j = 0; j < data[i].size(); ++j){
        data_with_heavy_flag[i][j] = {data[i][j].first, data[i][j].second, true};
      }
    }
    // first heavy path decomposition
    std::vector<int> number_of_paths_sink(data.size(), 0);
    number_of_paths_sink[data.size() - 1] = 1;
    for(Index i = data.size() - 1; i >= 0; --i){
      for(auto& [ch, to] : data[i]){
        number_of_paths_sink[i] += number_of_paths_sink[to];
      }
    }
    for(Index i = 0; i < data.size(); ++i){
      std::pair<int, int> max = {0, 0};
      for(Index j = 0; j < data[i].size(); ++j){
        auto [ch, to] = data[i][j];
        if(number_of_paths_sink[to] > max.second){
          max = {j, number_of_paths_sink[to]};
        }
      }
      for(Index j = 0; j < data[i].size(); ++j){
        if(j != max.first){
          std::get<2>(data_with_heavy_flag[i][j]) = false;
        }
      }
    }
    // second heavy path decomposition
    std::vector<int> number_of_paths_root(data.size(), 0);
    number_of_paths_root[0] = 1;
    for(Index i = 0; i < data.size(); ++i){
      for(auto& [ch, to] : data[i]){
        number_of_paths_root[to] += number_of_paths_root[i];
      }
    }
    std::vector<std::pair<Index, Index>> heavy_edges(data.size(), {NOT_FOUND, NOT_FOUND});
    for(Index i = data.size() - 1; i >= 0; --i){
      for(Index j = 0; j < data[i].size(); ++j){
        auto [ch, to, is_heavy] = data_with_heavy_flag[i][j];
        if(!is_heavy){
          continue;
        }
        if(heavy_edges[to].first == NOT_FOUND){
          heavy_edges[to] = {i, j};
        }
        else if(number_of_paths_root[i] > number_of_paths_root[heavy_edges[to].first]){
          std::get<2>(data_with_heavy_flag[heavy_edges[to].first][heavy_edges[to].second]) = false;
          heavy_edges[to] = {i, j};
        }
        else{
          std::get<2>(data_with_heavy_flag[i][j]) = false;
        }
      }
    }
    // obtain heavy paths
    std::vector<bool> heavy_edges_flag(data.size(), false);
    std::vector<Index> heavy_path;
    for(Index i = 0; i < data.size(); ++i){
      if(heavy_edges_flag[i]){
        continue;
      }
      heavy_edges_flag[i] = true;
      heavy_path.emplace_back(i);
      Index cur = i;
      while(true){
        bool has_heavy = false;
        for(auto& [ch, to, is_heavy] : data_with_heavy_flag[cur]){
          if(is_heavy){
            assert(!heavy_edges_flag[to]);
            heavy_path.emplace_back(to);
            heavy_edges_flag[to] = true;
            heavy_str.emplace_back(ch);
            cur = to;
            has_heavy = true;
            break;
          }
        }
        if(!has_heavy){
          break;
        }
      }
      heavy_str.emplace_back(NULL_CHAR);
    }
    assert(heavy_path.size() == data.size());
    std::vector<Index> heavy_path_inv(data.size());
    for(Index i = 0; i < heavy_path.size(); ++i){
      heavy_path_inv[heavy_path[i]] = i;
    }
    // obtain light edges
    std::vector<std::vector<std::pair<Char, Index>>> light_edges(data.size());
    for(Index i = 0; i < data.size(); ++i){
      for(auto& [ch, to, is_heavy] : data_with_heavy_flag[i]){
        if(!is_heavy){
          light_edges[heavy_path_inv[i]].emplace_back(ch, heavy_path_inv[to]);
        }
      }
    }
    maps = construct_maps<MapVector<STLMap>>(light_edges);
    root = heavy_path_inv.front();
    sink = heavy_path_inv.back();
  }
  bool search(const String& line) const override{
    Index node = root;
    for(Index i = 0; i < line.size(); ++i){
      Index lcp = get_lcp(heavy_str, node, line, i, line.size() - i);
      node += lcp;
      i += lcp;
      if(i == line.size()){
        break;
      }
      node = maps.search(node, line[i]);
      if(node == NOT_FOUND){
        return false;
      }
    }
    return node == sink;
  }
};

class PathDecomposedDoubleArrayADFA : public PatternMatcingIndex {
  Index root, sink;
  String heavy_str;
  std::vector<Index> next;
  DoubleArrayMaps maps;
public:
  explicit PathDecomposedDoubleArrayADFA(const PathDecomposedADFA& pdadfa) : maps(0){
    heavy_str = pdadfa.heavy_str;
    root = pdadfa.root;
    sink = pdadfa.sink;
    std::vector<std::vector<std::pair<Char, Index>>> light_edges = pdadfa.maps.to_vector();
    auto [da, cor] = DoubleArrayMaps::construct_without_reindexing(light_edges);
    maps = std::move(da);
    next = std::move(cor);
  }
  bool search(const String& line) const override{
    Index node = root;
    for(Index i = 0; i < line.size(); ++i){
      Index lcp = get_lcp(heavy_str, node, line, i, line.size() - i);
      node += lcp;
      i += lcp;
      if(i == line.size()){
        break;
      }
      node = maps.search(next[node], line[i]);
      if(node == NOT_FOUND){
        return false;
      }
    }
    return node == sink;
  }
  std::size_t memory_usage() const{
    std::size_t memory = sizeof(Index) * 2
                         + sizeof(Char) * heavy_str.size()
                         + sizeof(Index) * next.size()
                         + (sizeof(Char) + sizeof(Index)) * maps.size();
    return memory;
  }
};

class PathDecomposedBinarySearchADFA : public PatternMatcingIndex {
  Index root, sink;
  String heavy_str;
  BinarySearchMaps maps;
public:
  explicit PathDecomposedBinarySearchADFA(const PathDecomposedADFA& padfa) : maps(){
    heavy_str = padfa.heavy_str;
    root = padfa.root;
    sink = padfa.sink;
    std::vector<std::vector<std::pair<Char, Index>>> light_edges = padfa.maps.to_vector();
    maps = BinarySearchMaps::static_construct(light_edges);
    maps.reset_bv();
  }
  bool search(const String& line) const override{
    Index node = root;
    for(Index i = 0; i < line.size(); ++i){
      Index lcp = get_lcp(heavy_str, node, line, i, line.size() - i);
      node += lcp;
      i += lcp;
      if(i == line.size() || node == sink){
        break;
      }
      node = maps.search(node, line[i]);
      if(node == NOT_FOUND){
        return false;
      }
    }
    return node == sink;
  }
  std::size_t memory_usage() const{
    std::size_t memory = sizeof(Index) * 2
                         + sizeof(Char) * heavy_str.size()
                         + (sizeof(Char) + sizeof(Index) + 1) * maps.size();
    return memory;
  }
};

#endif //PACKED_ADFA_TRIE_HPP
