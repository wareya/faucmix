#include <map>
#include <utility>

typedef std::pair<int, int> pair;

int shittygcd(int a, int b)
{
    int swap;
    while(a)
    {
        swap = a;
        a = b%a;
        b = swap;
    }
    return b;
}

std::map<pair, long> cache;
 
int lcm(int a, int b)
{
    if (a == b)
        return b;
    if (b > a)
        std::swap(a, b);
    if(cache.count(pair(a, b)))
        return cache[pair(a, b)];
    else
    {
        long r = b*a/shittygcd(a, b);
        cache[pair(a, b)] = r;
        return r;
    }
}
