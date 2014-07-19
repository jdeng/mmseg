mmseg
=====

MMSEG simple word segmenter in C++ 11 

  * Based on http://yongsun.me/2013/06/simple-implementation-of-mmseg-with-python/
  * Data files from mmseg4j https://code.google.com/p/mmseg4j/
  * Compile with: g++ -Ofast -march=native -funroll-loops -DMMSEG_MAIN -x c++ -o mmseg -std=c++11 mmseg.h

Usage
===
  	MMSeg mmseg;
    mmseg.load("words.dic", "chars.dic");

  	std::u16string s = MMSeg::from_utf8(MMSeg::trim(line));
    for (auto& w: mmseg.segment(s)) 
	    std::cout << MMSeg::to_utf8(w) << "  "; 
  	std::cout << std::endl;
