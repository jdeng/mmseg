// MMSEG C++ 11
// Copyright (C) 2014 Jack Deng <jackdeng@gmail.com>
// MIT LICENSE
//
// Based on http://yongsun.me/2013/06/simple-implementation-of-mmseg-with-python/
// Data files from mmseg4j https://code.google.com/p/mmseg4j/
// Compile with: g++ -Ofast -march=native -funroll-loops -DMMSEG_MAIN -x c++ -o mmseg -std=c++11 mmseg.h
// UTF-8 input only
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <numeric>
#include <algorithm>
#include <functional>
#include <fstream>
#include <sstream>
#include <iostream>

#if defined(_LIBCPP_BEGIN_NAMESPACE_STD)
#include <codecvt>
#else // gcc has no <codecvt>
#include "utf8cpp/utf8.h"
#endif

class MMSeg {
public:
  using Char = char16_t;
  using String = std::u16string;
  using StringIt = String::const_iterator;
  using StringP = std::pair<StringIt, StringIt>;

  inline static std::string trim(const std::string& s, const std::string& whitespace = "\r\n \t")
  {
      const auto first = s.find_first_not_of(whitespace);
      if (first == std::string::npos) return "";

      const auto last = s.find_last_not_of(whitespace);
      const auto range = last - first + 1;
      return s.substr(first, range);
  }

#if defined(_LIBCPP_BEGIN_NAMESPACE_STD)
  inline static std::string to_utf8(const std::u16string& in) {
      std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> cv;
      return cv.to_bytes(in.data());
  }

  inline static std::u16string from_utf8(const std::string& in) {
      std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> cv;
      return cv.from_bytes(in.data());
  }
#else // gcc has no <codecvt>
  inline static std::string to_utf8(const std::u16string& in) {
    std::string out;
    utf8::utf16to8(in.begin(), in.end(), std::back_inserter(out));
    return out;
  }

  inline static std::u16string from_utf8(const std::string& in) {
    std::u16string out;
    utf8::utf8to16(in.begin(), in.end(), std::back_inserter(out));
    return out;
  }
#endif

private:
  struct Trie {
    struct Node {
      std::unordered_map<Char, Node*> trans_;
      bool val_ = false;

      ~Node() {
        for (auto& it: trans_) delete it.second;
        trans_.clear();
      }
    };

    Node root_;
    Trie() {}

    void add(const String& word) {
      Node *current = &root_;
      for (auto ch: word) {
        auto it = current->trans_.find(ch);
        if (it == current->trans_.end()) {
          auto ret = current->trans_.emplace(ch, new Node{});
          current = ret.first->second;
        }
        else {
          current = it->second;
        }
      }
      current->val_ = true;
    }

    std::vector<StringP> match_all(StringIt start, StringIt end) {
      std::vector<StringP> ret;
      Node *current = &root_;
      StringIt _start = start;
      for (; start != end; ++start) {
        auto ch = *start;
        auto it = current->trans_.find(ch);
        if (it == current->trans_.end())
          break;
        current = it->second;
        if (current->val_) ret.emplace_back(_start, start + 1);
      }

      return ret;
    }

    size_t size() const { return root_.trans_.size(); }
  };

  std::unordered_map<Char, int> char_freqs_;
  Trie dict_;

  static size_t length(const StringP& w) { return std::distance(w.first, w.second); }

  struct Chunk {
    std::vector<StringP> words_;
    size_t length_ = 0;
    float mean_ = 0, var_ = 0, degree_ = 0;

    Chunk(std::vector<StringP> words, const std::unordered_map<Char, int>& chars) : words_(std::move(words)) {
      length_ = std::accumulate(words_.begin(), words_.end(), size_t(0), [&](size_t n, const StringP& w) { return n + length(w); });
      mean_ = float(length_) / words_.size();
      var_ = - std::accumulate(words_.begin(), words_.end(), float(0), [&](size_t n, const StringP& w) { return  n + (length(w) - mean_) * (length(w) - mean_); }) / words_.size();

      for (auto& w: words_) {
        if (length(w) != 1) continue;
        auto it = chars.find(*w.first);
        if (it != chars.end())
          degree_ += log(float(it->second));
      }
    }

    std::string to_string() const {
      std::string s;
      for (auto& w: words_) { s += to_utf8(String(w.first, w.second)) + " "; }
      s += "(" + std::to_string(length_) + " " + std::to_string(mean_) + " " + std::to_string(var_) + " " + std::to_string(degree_) + ")";
      return s;
    }
  };

  std::vector<Chunk> get_chunks(StringIt _start, StringIt _end, int depth) {
    std::vector<Chunk> ret;
    std::function<void(StringIt, StringIt, int, std::vector<StringP>)> get_chunks_it = [&] (StringIt start, StringIt end, int n, std::vector<StringP> segs) {
      if (n == 0 || start == end) {
        ret.emplace_back(std::move(segs), char_freqs_);
      }
      else {
        auto m = dict_.match_all(start, end);
        for (auto& w: m) {
          auto nsegs = segs;
          auto len = length(w);
          nsegs.emplace_back(std::move(w));
          get_chunks_it(start + len, end, n - 1, std::move(nsegs));
        }
        segs.emplace_back(start, start +1 );
        get_chunks_it(start + 1, end, n - 1, std::move(segs));
      }
    };

    get_chunks_it(_start, _end, depth, std::vector<StringP>{});
    return ret;
  }

public:
  std::vector<String> segment(const String& s, int depth = 3) {
    std::vector<String> ret;
    auto start = s.begin(), end = s.end();
    while (start != end) {
      auto chunks = get_chunks(start, end, depth);
      auto best = std::max_element(chunks.begin(), chunks.end(), [&](const Chunk& x, const Chunk& y) {
        return std::tie(x.length_, x.mean_, x.var_, x.degree_) < std::tie(y.length_, y.mean_, y.var_, y.degree_);
      });

//      for (auto& c: chunks) std::cout << c.to_string() << std::endl; std::cout << std::endl;
      auto& word = best->words_.front();
      start += length(word);
      ret.emplace_back(String(word.first, word.second));
    }

    return ret;
  }

  int load(const std::string& dict, const std::string& char_freqs = "") {
    std::ifstream din(dict);
    if (!din.is_open()) return -1;
    while (din.good()) {
      std::string line;
      if (! std::getline(din, line)) break;
      auto word = from_utf8(trim(line));
      dict_.add(word);
    }
    din.close();

    if (char_freqs.empty()) return 0;

    std::ifstream fin(char_freqs);
    if (!fin.is_open()) return -1;
    while (fin.good()) {
      std::string line;
      if (! std::getline(fin, line)) break;
      auto s = trim(line);
      size_t pos = s.find(' ');
      if (pos != std::string::npos) {
        auto word = from_utf8(s.substr(0, pos));
        int freq = std::stoi(s.substr(pos+1));
        char_freqs_.emplace(word[0], freq);
      }
    }
    fin.close();

    std::cout << "Loaded Dict: " << dict_.size() << ", Freq: " << char_freqs_.size() << std::endl;
    return 0;
  }
};

#if defined(MMSEG_MAIN)
int main(int argc, const char *argv[]) {
  MMSeg mmseg;
  mmseg.load("words.dic", "chars.dic");

  if (argc >= 2) {
    std::ifstream ifs(argv[1]);
    if (! ifs.good()) {
      std::cerr << "Failed to open " << argv[1] << std::endl;
      return -1;
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    auto s = MMSeg::from_utf8(ss.str());
    time_t now = time(0);
    auto w = mmseg.segment(s);
    std::cout << "Done in " << time(0)  - now << " seconds, " << w.size() << " words from " << s.size() << " chars" << std::endl;
    return 0;
  }

  while (true) {
    std::cout << "Input String: ";
    std::string line;
    if (! std::getline(std::cin, line)) break;
    std::u16string s = MMSeg::from_utf8(MMSeg::trim(line));
    for (auto& w: mmseg.segment(s)) std::cout << MMSeg::to_utf8(w) << "  "; std::cout << std::endl;
  }

  return 0;
}
#endif

