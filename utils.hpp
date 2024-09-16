//
// Created by shibh308 on 2024/09/12.
//

#ifndef PACKED_ADFA_UTILS_HPP
#define PACKED_ADFA_UTILS_HPP

#include <vector>
#include <string>
#include <map>
#include <fstream>
#include <random>
#include <set>
#include <filesystem>
#include "sdsl/bit_vectors.hpp"

using Char = unsigned char;
using String = std::vector<Char>;
using Strings = std::vector<String>;
using Index = std::int32_t;

constexpr Char NULL_CHAR = 0;
constexpr Char EOW = 1;
constexpr Index NOT_FOUND = -1;


String convert_to_String(const std::string& str, bool add_eow){
  String ret(str.begin(), str.end());
  if(add_eow){
    ret.emplace_back(EOW);
  }
  return ret;
}

std::string base_dir_path = "../";
std::string data_dir_path = base_dir_path + "data/";
std::string out_csv_path = base_dir_path + "result.csv";

struct ResultCsvWriter {
  std::ofstream ofs;
  std::string dataset_name;
  std::size_t num_lines, total_length;
public:
  explicit ResultCsvWriter(const std::string& dataset_name, std::size_t num_lines, std::size_t total_length) : dataset_name(dataset_name), num_lines(num_lines), total_length(total_length){
    bool exists = std::filesystem::exists(out_csv_path);
    ofs.open(out_csv_path, std::ios::app);
    if(!exists){
      ofs << "timestamp,dataset,lines,total_length,method,time_nanoseconds,memory_bytes" << std::endl;
    }
  }
  void write(const std::string& method, std::size_t time, std::size_t memory){
    std::time_t now = std::time(nullptr);
    ofs << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S") << ",";
    ofs << dataset_name << ",";
    ofs << num_lines << ",";
    ofs << total_length << ",";
    ofs << method << ",";
    ofs << time << ",";
    ofs << memory << std::endl;
  }
};

constexpr int CHAR_BITS = 8;
constexpr int ALPHA = 8;
inline Index get_lsb_pos(std::uint64_t val){
  if(val == 0){
    return ALPHA;
  }
  unsigned int ctz = __builtin_ctzll(val);
  return ctz / CHAR_BITS;
}

inline Index get_lcp(const String& str1, Index ofs1, const String& str2, Index ofs2, Index max_len){
  unsigned int max_iter = max_len / ALPHA;
  const auto* ptr1 = reinterpret_cast<const std::uint64_t*>(str1.data() + ofs1);
  const auto* ptr2 = reinterpret_cast<const std::uint64_t*>(str2.data() + ofs2);
  int i = 0;
  while(*ptr1 == *ptr2 && i < max_iter){
    ++i;
    ++ptr1;
    ++ptr2;
  }
  return std::min(i * ALPHA + get_lsb_pos(*ptr1 ^ *ptr2), max_len);
}


Strings load_dataset(const std::string& dataset_name, std::size_t length_limit){
  std::string data_path = data_dir_path + dataset_name;
  std::clog << "loading: " << data_path << std::endl;
  std::ifstream file(data_path);
  assert(file.is_open());
  std::size_t total_bytes = 0;
  std::vector<String> lines;
  while(true){
    std::string line;
    std::getline(file, line);
    total_bytes += line.size();
    if(total_bytes >= length_limit){
      break;
    }
    lines.emplace_back(convert_to_String(line, true));
    if(file.eof()){
      break;
    }
  }
  std::vector<bool> occur(256, false);
  for(auto& line : lines){
    for(auto c : line){
      occur[c] = true;
    }
  }
  std::clog << "Loading file \"" << data_path << "\" is finished." << std::endl;
  std::clog << "Number of lines (bef): " << lines.size() << std::endl;
  std::clog << "Total bytes     (bef): " << total_bytes << std::endl;
  std::sort(lines.begin(), lines.end());
  lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
  total_bytes = std::accumulate(lines.begin(), lines.end(), 0, [](std::size_t sum, const String& line){
    return sum + line.size();
  });
  std::clog << "Number of lines      : " << lines.size() << std::endl;
  std::clog << "Total bytes          : " << total_bytes << std::endl;
  std::clog << "Number of characters : " << std::count(occur.begin(), occur.end(), true) << std::endl;
  std::clog << "Average length       : " << 1.0 * total_bytes / lines.size() << std::endl;
  std::clog << std::endl;
  return lines;
}

std::pair<Strings, Strings> split_data(const Strings& data, std::uint64_t seed = 42, double A_ratio = 0.8){

  std::set<String> data_set(data.begin(), data.end());

  Strings data_vec(data_set.begin(), data_set.end());
  std::mt19937 rand(seed);
  std::shuffle(data_vec.begin(), data_vec.end(), rand);
  std::size_t n = data.size();
  std::size_t train_size = n * A_ratio;
  Strings A(data_vec.begin(), data_vec.begin() + train_size);
  Strings B(data_vec.begin() + train_size, data_vec.end());
  return {A, B};
}

class Maps{
public:
  virtual void insert(Index idx, Char key, Index val) = 0;
  virtual Index search(Index idx, Char key) const = 0;
};

class Map{
public:
  virtual void insert(Char key, Index val) = 0;
  virtual Index search(Char key) const = 0;
  virtual std::vector<std::pair<Char, Index>> to_vector() const = 0;
};

class STLMap : public Map{
  std::map<Char, Index> map;
public:
  void insert(Char key, Index val) override{
    assert(map.find(key) == map.end());
    map[key] = val;
  }
  Index search(Char key) const override{
    auto it = map.find(key);
    if(it == map.end()){
      return NOT_FOUND;
    }
    return it->second;
  }
  std::size_t outdegree() const{
    return map.size();
  }
  std::vector<std::pair<Char, Index>> to_vector() const override{
    std::vector<std::pair<Char, Index>> data(map.begin(), map.end());
    return data;
  }
};

template <typename T> requires std::is_base_of_v<Map, T>
class MapVector : public Maps{
  std::vector<T> maps;
public:
  explicit MapVector(int size){
    maps.resize(size);
  }
  void insert(Index idx, Char key, Index val) override{
    maps[idx].insert(key, val);
  }
  Index search(Index idx, Char key) const override{
    return maps[idx].search(key);
  }
  std::size_t outdegree(Index idx) const{
    return maps[idx].outdegree();
  }
  void extend(int size){
    maps.resize(size);
  }
  Index size() const{
    return maps.size();
  }
  std::vector<std::vector<std::pair<Char, Index>>> to_vector() const{
    std::vector<std::vector<std::pair<Char, Index>>> data(maps.size());
    for(Index i = 0; i < maps.size(); ++i){
      data[i] = maps[i].to_vector();
    }
    return data;
  }
};

class DoubleArrayMaps : public Maps{
public:
  std::vector<Index> next;
  std::vector<Char> check;
  explicit DoubleArrayMaps(int size){
    next.resize(size, NOT_FOUND);
    check.resize(size, NULL_CHAR);
  }
  void insert(Index idx, Char key, Index val) override{
    assert(("Dynamic insertion is not supported. Use static_construct.", false));
  }
  Index search(Index idx, Char key) const override{
    idx += key;
    if(check[idx] == key){
      return next[idx];
    }
    return NOT_FOUND;
  }
  void extend(int size){
    next.resize(size, NOT_FOUND);
    check.resize(size, NULL_CHAR);
  }
  static std::pair<DoubleArrayMaps, std::vector<Index>> construct_without_reindexing(std::vector<std::vector<std::pair<Char, Index>>>& data){
    std::vector<Index> curs(data.size(), 0);
    DoubleArrayMaps maps(data.size());
    Index cur = 0;
    for(Index i = 0; i < data.size(); ++i){
      for(; ; ++cur){
        bool ok = true;
        for(auto [key, to] : data[i]){
          if(cur + key >= maps.next.size()){
            maps.extend(cur + key + 1);
          }
          else if(maps.check[cur + key] != NULL_CHAR){
            ok = false;
            break;
          }
        }
        if(ok){
          break;
        }
      }
      for(auto [key, to] : data[i]){
        maps.check[cur + key] = key;
        maps.next[cur + key] = to;
      }
      curs[i] = cur;
      ++cur;
    }
    return {maps, curs};
  }
  static std::pair<DoubleArrayMaps, std::vector<Index>> construct_with_reindexing(std::vector<std::vector<std::pair<Char, Index>>>& data){
    std::vector<std::vector<Index>> inv(data.size());
    DoubleArrayMaps maps(data.size());
    std::vector<Index> curs(data.size());
    Index cur = 0;
    for(Index i = 0; i < data.size(); ++i){
      for(; ; ++cur){
        bool ok = true;
        for(auto [key, to] : data[i]){
          if(cur + key >= maps.next.size()){
            maps.extend(cur + key + 1);
          }
          else if(maps.check[cur + key] != NULL_CHAR){
            ok = false;
            break;
          }
        }
        if(ok){
          break;
        }
      }
      for(auto [key, to] : data[i]){
        maps.check[cur + key] = key;
        if(!(to & (1 << 31))){
          inv[to].emplace_back(cur + key);
        }
      }
      curs[i] = cur;
      for(auto idx : inv[i]){
        maps.next[idx] = cur;
      }
      ++cur;
    }
    return {maps, curs};
  }
  Index size() const{
    return next.size();
  }
};

class BinarySearchMaps : public Maps{
  sdsl::bit_vector bv;
  sdsl::rank_support_v<1> rank;
  sdsl::select_support_mcl<1> select;
  std::vector<std::pair<Char, Index>> elms;
public:
  explicit BinarySearchMaps(){}
  void insert(Index idx, Char key, Index val) override{
    assert(("Dynamic insertion is not supported. Use static_construct.", false));
  }
  static BinarySearchMaps static_construct(const std::vector<std::vector<std::pair<Char, Index>>>& _data){
    std::vector<std::vector<std::pair<Char, Index>>> data = _data;
    Index total_size = std::accumulate(data.begin(), data.end(), 0, [](Index sum, const std::vector<std::pair<Char, Index>>& vec){
      return sum + vec.size();
    });
    sdsl::bit_vector bv(total_size + data.size() + 1);
    std::vector<std::pair<Char, Index>> elms;
    elms.reserve(total_size);
    Index cur = 0;
    for(Index i = 0; i < data.size(); ++i){
      bv[cur++] = true;
      std::sort(data[i].begin(), data[i].end());
      for(auto [key, val] : data[i]){
        elms.emplace_back(key, val);
        bv[cur++] = false;
      }
    }
    bv[cur++] = true;
    assert(bv.size() == cur);
    BinarySearchMaps maps;
    maps.bv = std::move(bv);
    maps.elms = std::move(elms);
    return maps;
  }
  void reset_bv(){
    rank = sdsl::rank_support_v<1>(&bv);
    select = sdsl::select_support_mcl<1>(&bv);
  }
  Index search(Index idx, Char key) const override{
    Index l = select(idx + 1);
    l = l - rank(l);
    Index r = select(idx + 2);
    r = r - rank(r);
    constexpr int linear_search_border = 5;
    while(r - l > linear_search_border){
      Index mid = (r + l) >> 1u;
      if(elms[mid].first == key){
        return elms[mid].second;
      }else if(elms[mid].first < key){
        l = mid;
      }else{
        r = mid;
      }
    }
    for(unsigned int i = l; i < r; ++i){
      if(elms[i].first == key){
        return elms[i].second;
      }
      else if(key < elms[i].first){
        return NOT_FOUND;
      }
    }
    return NOT_FOUND;
  }
  Index size() const{
    return elms.size();
  }
};

template <typename T> requires std::is_base_of_v<Map, T>
T construct_map(std::vector<std::pair<Char, Index>>& data){
  T map;
  for(auto [key, val] : data){
    map.insert(key, val);
  }
  return map;
}

template<typename U>
MapVector<U> construct_maps(std::vector<std::vector<std::pair<Char, Index>>>& data){
  MapVector<U> maps(data.size());
  for(Index i = 0; i < data.size(); ++i){
    maps[i] = construct_map<U>(data[i]);
  }
  return maps;
}

template <typename T> requires std::is_base_of_v<Maps, T>
T construct_maps(std::vector<std::vector<std::pair<Char, Index>>>& data){
  T maps(data.size());
  for(Index i = 0; i < data.size(); ++i){
    for(auto [key, val] : data[i]){
      maps.insert(i, key, val);
    }
  }
  return maps;
}

#endif //PACKED_ADFA_UTILS_HPP
