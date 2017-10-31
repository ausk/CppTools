// 2017.01.23
/*
参考：
[C++双缓冲多线程分析大文件词频](http://www.cnblogs.com/lekko/p/3370825.html)

主要技术点：
1. 多线程
2. C++ unordermap/map/labmda表达式/IO操作

*/

#include <algorithm>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <time.h>
#include <unordered_map>
using namespace std;

class mytimer {
    public:
        mytimer(const mytimer &timer) = delete;
        mytimer &operator()(mytimer &timer) = delete;
        mytimer(const char *name = "") : sname(name), tstart(0), tend(0) {
            tstart = clock();
            cout << "=======================================================\n";
            cout << "Start " << sname << "..." << endl;
            cout << "=======================================================\n";
        }
        void showPassedTime() {
            tend = clock();
            cout << "Time passed:" << (tend - tstart) / CLOCKS_PER_SEC << "s" << endl;
            cout << "=======================================================\n";

        }

        ~mytimer() {
            tend = clock();
            cout << "=======================================================\n";
            cout << "Finished " << sname << "!" << endl;
            cout << "Time passed:" << (tend - tstart) / CLOCKS_PER_SEC << "s" << endl;
            cout << "=======================================================\n";
        }

    private:
        string sname;
        double tstart, tend;
};

struct WordsLess {
    bool operator()(const char *str1, const char *str2) const {
        return strcmp(str1, str2) < 0;
    }
};

struct WordsEqual {
    bool operator()(const char *str1, const char *str2) const {
        return strcmp(str1, str2) == 0;
    }
};

struct WordHash {
    // BKDR hash algorithm
    int operator()(char *str) const {
        int seed = 131; // 31 131 1313 131313 etc..
        int hash = 0;
        while (*str) {
            hash = hash * seed + (*str++);
        }
        return hash & (0x7FFFFFFF);
    }
};

// Can you allocate an array with something equivalent to make_shared?
// http://stackoverflow.com/questions/13794447/can-you-allocate-an-array-with-something-equivalent-to-make-shared

template<typename T>
inline std::shared_ptr<T> MakeArray(int size)
{
    return std::shared_ptr<T>( new T[size], []( T *p ) {
        delete [] p;
    } );
}


using WordMap = map<char *, unsigned int, WordsLess>;
using KeyIter = WordMap::iterator;
using WordHashMap = unordered_map<char *, unsigned int, WordHash, WordsEqual>;
using KeyHashIter = WordHashMap::iterator;

int threadCount = 4;
streamsize loadsize = 1024; // 536870912; // 1024*1024*1024  1879048192
// 1610612736 1073741824 536870912 268435456
inline bool isword(char i) {
    static bool bIsWord[128] = {false};
    static bool bInitialed = false;
    if (!bInitialed) {
        // 初始化可识别字符
        memset(bIsWord, false, 128);
        bIsWord['_'] = true;
        for (char c = 'a'; c <= 'z'; ++c)
            bIsWord[c] = true;
        for (char c = 'A'; c <= 'Z'; ++c)
            bIsWord[c] = true;
        for (char c = '0'; c <= '9'; ++c)
            bIsWord[c] = true;
        bInitialed = true;
    }
    return bIsWord[i];
}

extern int readMultiThreadsIODoubleBuffers(const char *filename, int threadCount ,
        streamsize loadsize ) {

    mytimer timer("readMultiThreadsIODoubleBuffers");
    // int threadCount = 4;
    // streamsize loadsize = 1024; // 536870912; // 1024*1024*1024  1879048192
    // 1610612736 1073741824 536870912 268435456

    streamsize maxsize = loadsize + 256;
    //shared_ptr<WordHashMap> wordHashMaps(new WordHashMap[threadCount]); //memory leak without deconstructor
    shared_ptr<WordHashMap> wordHashMaps = MakeArray<WordHashMap>(threadCount);
    char *loadedFile[2]; // 双缓冲
    loadedFile[0] = new char[maxsize];
    loadedFile[1] = new char[maxsize];

    // 文件读入到堆
    auto readLoad = [&](int step, ifstream *file, streamoff start,
    streamsize size) -> void {
        file->seekg(start);
        file->read(loadedFile[step], size);
    };

    // 内存截断检查
    auto getBlockSize = [&](int step, streamoff start,
    streamsize size) -> streamsize {
        char *p = loadedFile[step] + start + size;
        while (isword(*p)) {
            ++size;
            ++p;
        }
        return size;
    };

    // 文件获取临界不截断的真正大小
    auto getRealSize = [&](ifstream *file, streamoff start,
    streamsize size) -> streamsize {
        file->seekg(start + size);
        while (isword(file->get()))
            ++size;
        return size;
    };

    // 分块读取
    auto readBlock = [&](int step, int id, streamoff start,
    streamsize size) -> void {
        char c = '\0';
        char word[128];
        int i = 0;
        WordHashMap *map = wordHashMaps.get() + id;
        KeyHashIter curr, end = map->end();
        char *filebuffer = loadedFile[step];
        streamsize bfSize = start + size;
        for (streamoff index = start; index != bfSize; ++index) {
            c = filebuffer[index];
            if (c > 0 && isword(c))
                word[i++] = c;
            else if (i > 0) {
                word[i++] = '\0';
                // 先判断有没有
                if ((curr = map->find(word)) == end) {
                    char *str = new char[i];
                    memcpy(str, word, i);
                    map->insert(pair<char *, unsigned int>(str, 1));
                } else
                    ++(curr->second);
                i = 0;
            }
        }
        if (i > 0) {
            word[i++] = '\0';
            if ((curr = map->find(word)) == end) {
                char *str = new char[i];
                memcpy(str, word, i);
                map->insert(pair<char *, unsigned int>(str, 1));
            } else
                ++(curr->second);
        }
    };

    // 读取文件
    ifstream file;
    file.open(filename, ios::binary | ios::in);
    if (!file) {
        cout << "Error: file \"" << filename << "\" do not exist!" << endl; // 失败
        exit(1);
    }

    // 确认文件大小
    streamoff start = 0;
    file.seekg(0, ios::end);
    streamoff size, len = file.tellg();
    if (len > 3) {
        // 确认有无BOM
        char bom[3];
        file.seekg(0);
        file.read(bom, 3);
        if (bom[0] == -17 && bom[1] == -69 && bom[2] == -65) {
            start = 3;
            size = len - 3;
        } else {
            size = len;
        }
    } else {
        size = len;
    }
    // 读入文件数据到缓存
    thread *threads = new thread[threadCount];
    streamsize realsize;
    streamoff index, part;
    bool bFrontNotBack = 0, needWait = false;
    while (size) {
        // 缓冲
        realsize = size > maxsize ? getRealSize(&file, start, loadsize) : size;
        readLoad(bFrontNotBack, &file, start, realsize);
        start += realsize;
        size -= realsize;
        // 等待
        if (needWait) {
            for (int i = 0; i < threadCount; ++i) {
                threads[i].join();
            }
        } else {
            needWait = true;
        }
        // 多线程计算
        index = 0, part = realsize / threadCount;
        for (int i = 1; i < threadCount; ++i) {
            len = getBlockSize(bFrontNotBack, index, part);
            // 开算
            threads[i] = thread(readBlock, bFrontNotBack, i, index, len);
            index += len;
        }
        threads[0] = thread(readBlock, bFrontNotBack, 0, index, realsize - index);
        // 转换
        bFrontNotBack = !bFrontNotBack;
    }
    // 清理
    for (int i = 0; i < threadCount; ++i) {
        threads[i].join();
    }
    delete loadedFile[0];
    delete loadedFile[1];
    file.close(); // 关闭

    timer.showPassedTime();
    // 结算累加
    WordMap map;
    for (int i = 0; i < threadCount; ++i) {
        KeyHashIter p = (wordHashMaps.get() + i)->begin(),
                    end = (wordHashMaps.get() + i)->end();
        for (; p != end; ++p) {
            cout << p->first << ":" << p->second << endl;
            char *word = p->first;
            int count = p->second;
            if (map.find(word) != map.end()) {
                map[word] += p->second;
            } else {
                map.insert(pair<char *, int>(word, count));
            }
        }
    }
    // 输出
    cout << "Done.\r\n\nDifferent words: " << map.size() << endl;
    KeyIter p = map.begin(), end = map.end();
    long total = 0;
    for (; p != end; ++p)
        total += p->second;
    cout << "Total words:" << total << endl;
    cout << "\nEach word count:" << endl;
    for (KeyIter i = map.begin(); i != map.end(); ++i)
        cout << i->first << "\t= " << i->second << endl;
    return 0;
}

int main(int argc, char *argv[]) {
    ios::sync_with_stdio(false);
    mytimer timer("Main function");
    // char *filename = argv[1];
    char *filename = "/home/auss/Documents/Zotero/CPlusPlusCoreGuidelines.md";
    int threadCount = 4;
    streamsize loadsize = 102400; // 536870912; // 1024*1024*1024  1879048192
    // 1610612736 1073741824 536870912 268435456
    if (argc > 2)
        threadCount = atoi(argv[2]);
    if (argc > 3)
        loadsize = atol(argv[3]);

    cout << "Starting to calculate with " << threadCount << " threads..." << endl;
    readMultiThreadsIODoubleBuffers(filename, threadCount, loadsize);
    cout<<"GreatJob!\n";
}
