#include<iostream>
#include<fstream>
#include<stdint.h>
#include<utility>
#include<cassert>
#include<codecvt>
#include<random>

template<typename T>
struct BucketArray {
    static constexpr auto bucketCount = size_t(100);
    struct Node {
        Node *next, *prev;
        T arr[bucketCount];
    };

    Node *start;
    Node *end;
    size_t count;

    void add(T value) {
        if(end == nullptr) {
            end = start = new Node;
            end->next = nullptr;
            end->prev = nullptr;
        }
        else if(count % bucketCount == 0) {
            end->next = new Node;
            end->next->prev = end;
            end->next->next = nullptr;
            end = end->next;
        }

        assert(end != nullptr);
        end->arr[count++ % bucketCount] = value;
    }

    T &operator[](size_t const index) {
        auto const indexBucket = index / bucketCount;
        auto const indexInBucket = index % bucketCount;
        auto posBucket = 0;
        auto cur = start;
        while(cur != nullptr & indexBucket != posBucket) {
            posBucket++;
            cur = cur->next;
        }
        if(cur == nullptr) assert(false && "no such element");
        
        return cur->arr[indexInBucket];
    }

    T removeAt(size_t const index) {
        auto const indexBucket = index / bucketCount;
        auto const indexInBucket = index % bucketCount;
        auto posBucket = 0;
        auto cur = start;
        while(cur != nullptr & indexBucket != posBucket) {
            posBucket++;
            cur = cur->next;
        }
        if(cur == nullptr) assert(false && "no such element");
        
        auto const element = cur->arr[indexInBucket];
        cur->arr[indexInBucket] = end->arr[--count % bucketCount];

        if(count == 0) end = start = nullptr;
        else if(count % bucketCount == 0) {
            end = end->prev;
            delete end->next;
            end->next = nullptr;
        }

        return element;
    }
};


int main() {
    static constexpr auto firstLetterCode = 1072;
    static constexpr auto lastLetterCode = 1103;
    static constexpr auto extraLetterCode = 1105;
    static constexpr auto letterCount = lastLetterCode - firstLetterCode + 1/*extra letter*/;

    using code_t = uint8_t;
    assert(letterCount + 1/*0 code*/ <= 255);

    auto const canonLetterCode = [](code_t const code) -> code_t {
        if(code == letterCount-1 + 1) return 5 + 1; //change ё to е
        else if(code == 9 + 1) return 8 + 1; //change й to и
        else return code;
    };

    auto const outputElement = [&](std::wostream &o, code_t const *cur) {
        while(*cur != 0) {
            if(*cur == letterCount-1 + 1) {
                o << (wchar_t) extraLetterCode;
            }
            else o << (wchar_t) (int(*cur) - 1 + firstLetterCode);
            cur++;
        }
        o << '\n';
    };

    BucketArray<code_t const *> wordsStartEndWith[letterCount * letterCount] = {};  

    auto wordCount = 0;
    /*populate words array from file*/ {
        auto str = std::wifstream{"russian_nouns.txt"};
        str.imbue(std::locale(
            str.getloc(),
            new std::codecvt_utf8_utf16<wchar_t, 0x10ffff, std::consume_header> 
        ));

        str.seekg(0, str.end);
        auto const sCount_ = str.tellg();
        str.seekg(0, str.beg);
        auto const sCount = size_t(sCount_);

        auto const storage = new code_t[sCount+1ull] /*
            won't be deleted, words will reference this array at different positions
        */;

        auto wordStartPos = 0ull;
        auto curPos = wordStartPos;
        while(true) {
            wchar_t ch;
            str.get(ch);
            auto const end = str.fail() | str.eof();

            if(end || ch == '\n') {
                assert(wordStartPos < curPos-1);
                auto const startOffset = canonLetterCode(storage[wordStartPos]) - 1;
                auto const endOffset = canonLetterCode(storage[curPos-1]) - 1;
                assert(startOffset >= 0 & startOffset < letterCount);
                assert(endOffset >= 0 & endOffset < letterCount);

                if(startOffset == letterCount-1 | endOffset == letterCount-1) /*starts or ends in ё*/ {
					//these words are weird, ignore them
                    curPos = wordStartPos;
                }
                else {
                    wordsStartEndWith[startOffset*letterCount + endOffset].add(storage + wordStartPos);

                    storage[curPos++] = 0;
                    wordStartPos = curPos;
                    wordCount++;
                }

                if(end) break;
            }
            else if(ch >= firstLetterCode & ch <= lastLetterCode) {
                storage[curPos++] = ch - firstLetterCode + 1;
            }
            else if(ch == extraLetterCode) {
                storage[curPos++] = letterCount-1 + 1;
            }
        }
    }

    /*std::cout << sCount << ' ' << wordCount << '\n';
    for(int i = 0; i < letterCount; i++) {
        std::cout << i << ' ' << wordsStartWith[i].count << ' ' << wordsEndWith[i].count << '\n';
    }*/

    //list of words in the chain
    auto const list = new code_t const *[wordCount];
    auto listCount = 0;

    std::random_device dev{};
    auto const seed = dev();
    std::mt19937 rng(seed);
    auto const rand = [&](size_t const to) {
        return std::uniform_int_distribution<size_t>(0, to-1)(rng);
    };

    //current/initial word count for starting letters
    int     curWordCForSL[letterCount]{};
    int initialWordCForSL[letterCount]{};
    for(int i = 0; i < letterCount * letterCount; i++) {
        curWordCForSL[i/letterCount] = 
            initialWordCForSL[i/letterCount] += wordsStartEndWith[i].count;
    }

    int startEndWithLetterCount[letterCount*letterCount]{};

    auto const findEndLetterCode = [&](code_t const *const start) -> code_t {
        auto cur = start;
        assert(*cur != 0);
        while(*(cur+1) != 0) cur++; 
        while(true) {
            auto const pos = canonLetterCode(*cur);
            if(initialWordCForSL[pos - 1] == 0) {
                if(cur == start) return 0;
                cur--;
            }
            else return pos;
        }
    };
    
    auto startLetter = 17; // rand(letterCount);

    //go to a prev word if there are no next words for current word
    auto backUp = 0;
    auto const tryBackUp = [&]() -> bool {
        if(backUp > 5) return false;
        else if(listCount == 0) return false;
        else if(listCount == 1) {
            startLetter = rand(letterCount);
            backUp++;
            listCount--;
            return true;
        }
        else {
            auto const i = *list[listCount-1];
            if(i == 0) return false;
            backUp++;
            listCount--;
            startLetter = i-1;
            return true;
        }
    };

    //generate chain of words
    while(true) {
        int wordGroupI;
        /*pick a word with specified starting letter*/ {
            if(curWordCForSL[startLetter] == 0) {
                if(tryBackUp()) continue;
                else break;
            }

            auto const adj = [](int const n, int const m) -> int {
                return std::pow(n, 1.0f) * std::pow(m, 1.2f); 
            };

            int letterWeightsTotal = 0;
            int letterWeightsAccum[letterCount];
            for(int i = 0; i < letterCount; i++) {
                letterWeightsAccum[i] = letterWeightsTotal += adj( 
                    wordsStartEndWith[startLetter*letterCount + i].count,
                    std::max(curWordCForSL[i], 1)
                );
            }

            auto endLetter = 0;
            for(
                auto const randEndLetterS = rand(letterWeightsTotal);
                endLetter < letterCount-1 && letterWeightsAccum[endLetter] <= randEndLetterS; 
                endLetter++
            );
            wordGroupI = startLetter * letterCount + endLetter;

            if(wordsStartEndWith[wordGroupI].count == 0) {
                assert(false) /*never happened in practice*/;

                uint8_t indices[letterCount];
                for(int i = 0; i < letterCount; i++) indices[i] = i;

                for(int i = 0; i < letterCount; i++) {
                    auto const indexI = rand(letterCount-1 - i  + 1);
                    endLetter = indices[indexI];
                    std::swap(indices[indexI], indices[letterCount-1 - i]);
                    if(wordsStartEndWith[startLetter*letterCount + endLetter].count != 0) {
                        wordGroupI = startLetter*letterCount + endLetter;
                        goto succ;
                    }
                }

                if(tryBackUp()) continue;
                else break;
            }

            succ:;
            backUp = 0;
        }
        auto &array = wordsStartEndWith[wordGroupI];
        auto const curWord = array.removeAt(rand(array.count));
        curWordCForSL[startLetter]--;
        list[listCount++] = curWord;
        startEndWithLetterCount[wordGroupI]++;

        startLetter = findEndLetterCode(curWord);
        if(startLetter == 0) break;
        startLetter--;
    }

    listCount += backUp /*all the words from the last back up are still there*/;

    /*check if we didn't loose or add any words*/ {
        auto sum = 0;
        auto sum2 = 0;
        for(int i = 0; i < letterCount; i++) {
            auto wordsSum = 0;
            auto remWordsSum = 0;
            for(int j = 0; j < letterCount; j++) {
                wordsSum += wordsStartEndWith[i*letterCount+j].count + startEndWithLetterCount[i*letterCount+j];
                remWordsSum += wordsStartEndWith[i*letterCount+j].count;
            }
            assert(initialWordCForSL[i] == wordsSum);
            assert(curWordCForSL[i] == remWordsSum);
            sum += wordsSum;
            sum2 += initialWordCForSL[i];
        }

        assert(sum == wordCount);
        assert(sum == wordCount);
    }

    //check is the chain is correct
    //auto last_ = 0;
    for(int i = 0; i < listCount-1; i++) {
        auto endLetter = findEndLetterCode(list[i]);
        assert(endLetter != 0);
        assert(endLetter == canonLetterCode(*list[i + 1]));
        //if(*list[i+1] == 1) { std::cout << (i+1 - last_) << ' '; last_ = i+1; }
    }
    //std::cout << "last: " << last_ << '\n';

    //write the chain and the seed
    auto output = std::wofstream{ "chain.txt" };
    output.imbue(std::locale(
        output.getloc(),
        new std::codecvt_utf8_utf16<wchar_t, 0x10ffff, std::consume_header> 
    ));

    for(int i = 0; i < listCount; i++) {
        outputElement(output, list[i]);
    }

    auto seedO = std::ofstream{ "seed.txt", std::ios_base::app };
    seedO << seed << '\n';
    seedO.close();


    std::cout << "Done!\n";
    return 0;
}
