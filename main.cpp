#include<sstream>
#include<string>

#include<fstream>
#include<iostream>

#include<cassert>

#include<chrono>
#include<locale>
#include<codecvt>
//just for printing time
#include <stdio.h> 
#include <stdlib.h> 
#include <time.h> 

#include<cstdio>
#include<thread> 
#include<vector>

static char const *exec(char const *const cmd) {
    static char buf[4096];
    auto const pipe = popen(cmd, "r");
    if (!pipe) return nullptr;
    fgets(buf, 4096, pipe);
    pclose(pipe);
    return buf;
}

static constexpr auto pattern = "{\"response\":{\"comment_id\":$,\"parents_stack\":[]}}";

static bool checkPattern(char const *it) {
    auto pat = pattern;
    while(*it != '\0' & *pat != '\0') {
        while(*it == ' ' | *it == '\n') it++;
        while(*pat == ' '| *pat == '\n') pat++;
        if(*pat == '$') {
            while(*it >= '0' & *it <= '9') it++;
            pat++;
        }
        if(*pat != *it) {
            //std::cout << "@" << (int)*it << ' ' << (int)*pat << '\n';
            return false;
        }
        it++;
        pat++;
    }
    //std::cout << "@" << (int)*it << ' ' << (int)*pat << '\n';
    return *it == *pat;
}

static char const *toHex(unsigned char const codePoint) {
    static constexpr char chars[] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
    };

    static char arr[3] = {};

    arr[0] = chars[codePoint / 16];
    arr[1] = chars[codePoint % 16];

    return arr;
}

#define COMMAND1 curl 'https://api.vk.com/method/wall.createComment' \
  -H 'authority: api.vk.com' \
  -H 'accept: */*' \
  -H 'accept-language: en-US,en;q=0.9' \
  -H 'content-type: application/x-www-form-urlencoded' \
  -H 'origin: https://dev.vk.com' \
  -H 'referer: https://dev.vk.com/' \
  -H 'sec-ch-ua: "Google Chrome";v="113", "Chromium";v="113", "Not-A.Brand";v="24"' \
  -H 'sec-ch-ua-mobile: ?0' \
  -H 'sec-ch-ua-platform: "Chrome OS"' \
  -H 'sec-fetch-dest: empty' \
  -H 'sec-fetch-mode: cors' \
  -H 'sec-fetch-site: same-site' \
  -H 'user-agent: Mozilla/5.0 (X11; CrOS x86_64 14541.0.0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/113.0.0.0 Safari/537.36' \
  --compressed \
  --data-raw

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

int main() {
    std::stringstream ss{};

    std::vector<std::string> words{};
    /*read words*/ {
        auto str = std::ifstream{"words.txt"};

        auto tmp = std::string{};
        ss << std::hex;

        while(std::getline(str, tmp)) {
            for(auto const ch : tmp) {
                ss << '%' << toHex(ch);
            }
            words.emplace_back(ss.str());
            ss.str(std::string{});
        }
    }

    using configStr_t = char *;
    configStr_t accessToken, ownerId, postId; 
    /*read config*/ {
        auto i = std::ifstream{"config.txt"};
        i.seekg(0, i.end);
        auto const size = i.tellg();
        i.seekg(0, i.beg);

        auto const configStr = new char[int(size) + 1];
        auto charI = 0;
        unsigned strEnds[3];
        auto curEnd = 0;

        while(true) {
            auto const ch = i.get();
            if(ch == std::char_traits<char>::eof()) {
                configStr[charI] = '\0';
                break;
            }
            else if(ch == '\n') {
                configStr[strEnds[curEnd++] = charI++] = '\0';
            }
            else {
                configStr[charI++] = (char) ch;
            }
        }

        assert((curEnd == 2 | curEnd == 3) & size_t(charI) == size);
        assert(strEnds[0] > 1 & strEnds[1] > 1+strEnds[0] & (curEnd == 2 ? charI > strEnds[1] : strEnds[2] > 1+strEnds[1]));

        accessToken = configStr;
        ownerId = configStr + strEnds[0] + 1u;
        postId = configStr + strEnds[1] + 1u;
    }

    size_t index, count = 0;
    std::ifstream{"index.txt"} >> index;

    auto const start = std::chrono::steady_clock::now();
    auto const startTime = time(NULL);

    auto cur = start;
    auto lastLogPosition = decltype(std::ofstream{""}.tellp()){};
    {
        auto o = std::ofstream{ "log.txt", std::ios_base::app };
        o << "start time: " << ctime(&startTime);
        lastLogPosition = o.tellp();
    }

    while(true) {
        ss << TOSTRING(COMMAND1) << " \'owner_id=" << ownerId << "&post_id=" << postId 
            << "&message=" << words[index] << "&access_token=" << accessToken << "&v=5.131\'";

        auto const v = exec(ss.str().c_str());
        count++;


        auto const curTime = time(NULL); 
        std::system("clear");
        std::cout << (v ? v : "(null)") << "\n(" << count << " requests sent, working for "
            << std::chrono::duration_cast<std::chrono::minutes>(cur-start).count() << "m"
            << (std::chrono::duration_cast<std::chrono::seconds>(cur-start).count() % 60) << "s)\n";
        printf("first   time: %s", ctime(&startTime));
        printf("current time: %s", ctime(&curTime));
        std::cout.flush();


        auto const errorResponse = v == nullptr || !checkPattern(v);
        auto const finish = count == 50 | errorResponse;

        /*log current request*/ {
            auto o = std::fstream{ "log.txt", std::fstream::in | std::fstream::out };
            o.seekp(lastLogPosition);
            o << "end time  : " << ctime(&curTime);
            o << "requests count: ";
            if(errorResponse) o << '-';
            o << count << '\n';
            o << "------------------------------------\n";

        }

        if(errorResponse) {
            std::cout << "ERROR: UNREOCGNIZED RESPONSE\n";
            return 1;
        }

        index = (index + 1) % words.size();
        std::ofstream{"index.txt"} << index;

        if(count == 50) {
            std::cout << "OK: MAX AMOUNT OF REQUESTS REACHED\n";
            return 0;
        }

        ss.str(std::string{});
        auto const next = cur + std::chrono::seconds(10);
        std::this_thread::sleep_until(next);
        cur = next;
    }

    assert(false) /*unreachable*/;
}
