#ifndef STGEN_BARRIER_MERGE_H
#define STGEN_BARRIER_MERGE_H

#include "STTypes.hpp"

/*****************************************************************************
 * Merge per-barrier statistics across threads.
 *
 * Each thread holds a list of aggregate stats for each barrier it comes across.
 * Because the barriers may not line up nicely between threads,
 * we do some ugly merging
 *****************************************************************************/


namespace STGen
{

struct BarrierMerge
{
    /* Find the first element of 'from', that matches 'to'.
     * Merge the first matching nodes, and insert all other
     * nodes up to that point in 'from'. Continue until no
     * nodes are left to merge
     *
     * For example, the following barriers may need to be merged:
     * Assume barrier attributes (thread count) does not change.
     * Tried to create sufficiently complex example...
     *
     * T1  T2  T3  T4    Merged
     * B1      B1
     * B2  B2  B2  B2
     * B2  B2  B2  B2
     * B3      B3
     * B2  B2  B2  B2
     * B4          B4
     * -----------------------
     * T1  T2  T3  T4    Merged
     * B1      B1
     * B2  B2  B2          B2 <- this is the head
     * B2  B2  B2          B2 <- important to keep these separate
     * B3      B3                |   from previous barrier
     * B2  B2  B2          B2 <--+
     * B4                  B4
     * -----------------------
     * T1  T2  T3  T4    Merged
     * B1                  B1
     * B2  B2              B2 <- B2 found first for T3, insert B1 before
     * B2  B2              B2 <- B2 should merge with same level
     * B3                  B3 <- put after last inserted/merged (B2),
     * B2  B2              B2 <- matched after last inserted/merged (B3)
     * B4                  B4
     * ------------------------
     * T1  T2  T3  T4    Merged
     * B1                  B1
     * B2                  B2 <- B2 easily found, merged
     * B2                  B2 <- should merge with same level
     * B3                  B3    |
     * B2                  B2 <--+
     * B4                  B4
     * ------------------------
     * T1  T2  T3  T4    Merged
     *                     B1
     *                     B2
     *                     B2
     *                     B3
     *                     B2
     *                     B4
     *
     * If someone comes up with counter examples, please fix :)
     * This is non-exhaustive
     */

    using BarrierIt = AllBarriersStats::iterator;
    using ConstBarrierIt = AllBarriersStats::const_iterator;

    struct FromState
    {
        ConstBarrierIt begin;
        ConstBarrierIt current;
        ConstBarrierIt end;
    };

    struct ToState
    {
        BarrierIt current;
        BarrierIt previous;
        AllBarriersStats &mergedStats;
    };


    static auto merge(const AllBarriersStats &from, AllBarriersStats &to) -> void
    {
        if (from.empty())
            return;

        if (to.empty())
            return (void)(to = from);

        merge({from.begin(), from.begin(), from.end()},
              {to.begin(), to.end(), to});
    }

    static auto merge(FromState from, ToState to) -> void
    {
        /* nothing left */
        if (from.begin == from.end)
            return;

        /* no matches found previously */
        if (from.current == from.end)
        {
            to.mergedStats.insert(to.previous, from.begin, from.end);
            return;
        }

        auto match = findMatchTo(from.current, to.current, to.mergedStats.end());
        if (match == to.mergedStats.end())
        {
            ++from.current;
            merge(from, to);
        }
        else /* matched a barrier */
        {
            /* merge current and insert any previous barriers */
            match->second += from.current->second;
            if (from.begin != from.current)
                to.mergedStats.insert(match, from.begin, from.current);

            /* continue with the rest of the barriers */
            assert(match != to.mergedStats.end());
            to.previous = ++match;
            to.current = to.previous;

            ++from.current;
            from.begin = from.current;
            merge(from, to);
        }
    }

    static auto findMatchTo(ConstBarrierIt from, BarrierIt to, BarrierIt toEnd) -> BarrierIt
    {
        if (from->first == to->first || to == toEnd)
            return to;
        else
            return findMatchTo(from, ++to, toEnd);
    }
};

}; //end namespace STGen

#endif
