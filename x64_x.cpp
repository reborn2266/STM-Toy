#include <iostream>
#include <vector>
#include <thread>
#include <immintrin.h>

volatile static int i = 0;
volatile static int j = 0;

int safe()
{
    int status;
    while ((status = _xbegin ()) != _XBEGIN_STARTED) ;

    i++;
    //FIXME: anyone can tell me why program will hang if cout here?
    //std::cout << "o" << std::endl;;
    _xend();

    return i;
}

int unsafe()
{
    j++;
    return j;
}


int main()
{
    std::vector<std::thread> v1(10);
    std::vector<std::thread> v2(10);
    for(auto& t: v1)
        t = std::thread([]{ for(int n = 0; n < 10; ++n) safe(); });
    for(auto& t: v2)
        t = std::thread([]{ for(int n = 0; n < 10; ++n) unsafe(); });
    for(auto& t: v1)
        t.join();
    for(auto& t: v2)
        t.join();
    std::cout << "transactional: " << i << std::endl;
    std::cout << "race: " << j << std::endl;
}
