#include <set>

#include <iostream>

#include <SDL2/SDL_stdinc.h>

template <typename T>
struct GenericAllocator
{
    std::set<T> Used; // IDs which are currently being used
    std::set<T> Freed; // IDs which were used at some point, but no longer are
    T LowestUnused = 1; // The lowest ID which has never been used yet
    
    T New()
    {
        if (Freed.size() == 0) // No freed IDs, use lowest unused
        {
            auto id = LowestUnused;
            Used.insert(id); //logn
            
            // We need to figure out what the next lowest unused will be
            // There aren't any freed ones to use, so we have to do it hard
            if (LowestUnused >= *Used.rbegin()) // lowest unused is outside of used range, free to increment LowestUnused
                LowestUnused++;
            else // This shouldn't ever happen; no freed IDs, lowest unused being inside Used's bounds -- implies earlier error setting LowestUnused
            {
                std::cout << "Error: Unknown cause with LowestUnused being invalid 1!\n";
                std::cout << LowestUnused << " " << *Used.rbegin() << "\n";
                return -1;
            }
            return id;
        }
        else // use an ID from the freed list
        {
            auto id = *Freed.begin();
            Freed.erase(id); //logn
            Used.insert(id); //logn
            
            if(Freed.size() > 0) // We have an ID to use
            { // (Freed.size == 0 block won't be the next to run if we're here, so...)
                LowestUnused = *Freed.begin();
                return id;
            }
            else // We just exhausted the last previously freed ID
            {
                if (LowestUnused >= *Used.end()) // lowest unused is outside of used range, free to increment LowestUnused
                    LowestUnused++;
                else // This shouldn't ever happen; no freed IDs, lowest unused being inside Used's bounds -- implies earlier error setting LowestUnused
                {
                    std::cout << "Error: Unknown cause with LowestUnused being invalid 2!";
                    return -1;
                }
                std::cout << id << "\n";
                return id;
            }
        }
    }
    // attempt to forcefully allocate a specific ID
    // returns whether successful
    bool ForceNew(T id)
    {
        if(Exists(id))
            return false;
        else
        {
            Used.insert(id); //logn
            if (Freed.size() == 0 and LowestUnused == id)
            {
                LowestUnused = *Used.rbegin();
            }
            else if (Freed.find(id))
            {
                Freed.erase(id);
            }
            else
            {
                puts("Critical error synchronizing internal state in ForceNew!");
            }
            return true;
        }
    }
    // free a given ID -- returns false if id doesn't exist
    bool Free(T id)
    {
        if(Used.find(id) == Used.end())
            return false;
        Freed.insert(id);
        Used.erase(id);
        if(id < LowestUnused)
            LowestUnused = id;
        return true;
    }
    bool Exists(T id)
    {
        return *Used.find(id);
    }
};
