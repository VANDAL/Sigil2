#ifndef STGEN_EVENT_H
#define STGEN_EVENT_H

#include <memory>
#include <vector>
#include <sstream>
#include <set>
#include <unordered_map>

#include "MemoryPool.h"
#include "spdlog.h"

#include "Sigil2/Primitive.h"
#include "ShadowMemory.hpp"

/*******************************************************************************
 * SynchroTrace Events
 *
 * Defines an application event trace for later replay
 * in the SynchroTraceSim CMP simulator.
 *
 * See Section 2.B in the SynchroTrace paper for further clarification.
 ******************************************************************************/

namespace STGen
{

/* for speed, adapted from http://stackoverflow.com/a/33447587 */
template <typename I>
inline const char *n2hexstr(const I &w)
{
    static constexpr int hex_len = sizeof(I) * 2;
    static const char *digits = "0123456789abcdef";
    thread_local static char rc[hex_len + 3]; //extra 3 slots are "0x" and null char

    bool hex_MSB_found = false;
    int hex_MSB = 2;

    for (size_t i = 2, j = (hex_len - 1) * 4 ; i < hex_len + 2; ++i, j -= 4)
    {
        rc[i] = digits[(w >> j) & 0x0f];

        if (hex_MSB_found == false && rc[i] != '0')
        {
            hex_MSB_found = true;
            hex_MSB = i;
        }
    }

    /* TODO if 'w' is 0, then this returns the full size hex string;
     * instead it should just return "0x0" */

    rc[hex_MSB - 2] = '0';
    rc[hex_MSB - 1] = 'x';
    rc[hex_len + 2] = '\0';

    return rc + (hex_MSB - 2);
}

/* Helper class to track unique ranges of addresses */
struct AddrSet
{
    using AddrRange = std::pair<Addr, Addr>;
    AddrSet() { }
    AddrSet(const AddrRange &range)
    {
        ms.insert(range);
    }
    AddrSet(const AddrSet &other)
    {
        ms = other.ms;
    }
    AddrSet &operator=(const AddrSet &) = delete;

  private:
    std::multiset<AddrRange, std::less<AddrRange>, MemoryPool<AddrRange>> ms;

  public:
    /* A range of addresses is specified by the pair.
     * This call inserts that range and merges existing ranges
     * in order to keep the set of addresses unique */
    void insert(const AddrRange &range);
    void clear();
    const decltype(ms) &get()
    {
        return ms;
    }
};


/**
 * A SynchroTrace Instruction Event
 *
 * SynchroTrace tracks instruction addresses for future work to increase
 * simulation accuracy. The addresses can be used to simulate an I-cache
 * and any associated latencies
 *
 * The addresses are aggregated to avoid needless writes
 */
class STInstrEvent
{
    /* all active addresses */
    std::string instrs;

    bool is_empty;
    const std::shared_ptr<spdlog::logger> &logger;

  public:
    /* running count over all threads */
    static std::atomic<unsigned long long> instr_count;

  public:
    STInstrEvent(const std::shared_ptr<spdlog::logger> &logger);
    void append_instr(Addr addr);

    /* Addresses should be flushed whenever
     * compute or communication events flush */
    void flush();

  private:
    void reset();
};


/**
 * A SynchroTrace Compute Event.
 *
 * SynchroTrace compute events comprise of one or more Sigil primitive events.
 * I.e., a ST compute event can be any number of local thread load, store,
 * and compute primitives. However in practice, a 'compute' event is normally
 * cut off at ~100 event primitives.
 *
 * A SynchroTrace compute event is only valid for a given thread.
 * Usage is to fill up the event, then flush it to storage at a thread swap,
 * a communication edge between threads, or at an arbitrary number
 * of iops/flops/reads/writes.
 */
struct STCompEvent
{
    using StatCounter = unsigned long long;
    StatCounter iop_cnt;
    StatCounter flop_cnt;

    /* Stores and Loads originating from the current thread
     * I.e. non-edge mem events. These are the count for 'events',
     * not a count for bytes stored/loaded */
    StatCounter thread_local_store_cnt;
    StatCounter thread_local_load_cnt;

    /* Holds a range for the addresses touched by local stores/loads */
    AddrSet stores_unique;
    AddrSet loads_unique;

    STInstrEvent &instr_ev;

    StatCounter total_events;
    bool is_empty;

    TID &thread_id;
    EID &event_id;
    std::string logmsg;
    const std::shared_ptr<spdlog::logger> &logger;

  public:
    /* global count over all threads */
    static std::atomic<StatCounter> flop_count_global;
    static std::atomic<StatCounter> iop_count_global;
    static std::mutex map_mutex;
    static std::map<TID,std::pair<StatCounter, StatCounter>> op_map_global;

    /* local to each thread */
    struct _per_thread_t
    {
        TID curr_tid = -1;

        /* (thread id, <iop, flop>) */
        std::map<TID, std::pair<StatCounter, StatCounter>> per_thread_counts;
        StatCounter flop_count{0};
        StatCounter iop_count{0};

        void sync()
        {
            per_thread_counts[curr_tid].first = iop_count;
            per_thread_counts[curr_tid].second = flop_count;
        }

        void setThread(TID tid)
        {
            sync();

            if /*new thread*/(per_thread_counts.find(tid) == per_thread_counts.cend())
            {
                iop_count = 0;
                flop_count = 0;
            }
            else
            {
                iop_count = per_thread_counts[tid].first;
                flop_count = per_thread_counts[tid].second;
            }

            curr_tid = tid;
        }

        StatCounter getThreadIOPS(TID tid)
        {
            sync();

            return per_thread_counts[tid].first;
        }

        StatCounter getThreadFLOPS(TID tid)
        {
            sync();

            return per_thread_counts[tid].second;
        }

    } per_thread_data;

  public:
    STCompEvent(TID &tid, EID &eid, const std::shared_ptr<spdlog::logger> &logger,
                STInstrEvent &instr_ev);
    void flush();
    void updateWrites(const SglMemEv &ev);
    void updateWrites(const Addr begin, const Addr size);
    void updateReads(const SglMemEv &ev);
    void updateReads(const Addr begin, const Addr size);

    /* Compute Event metadata */
    void incWrites();
    void incReads();
    void incIOP();
    void incFLOP();

    /* HACK Hotfix requested by KS for IOP/FLOP counts on a per-thread basis,
     * and additionally on a per-process basis */
    void track_thread(TID tid);

  private:
    void reset();
};


/**
 * A SynchroTrace Communication Event.
 *
 * SynchroTrace communication events comprise of communication edges.
 * I.e., a ST comm event is typically generated from a read to data
 * that was originally created by other threads. Any subsequent reads
 * to that data by the same thread is not considered a communication
 * edge. Another thread that writes to the same address resets this process.
 */
struct STCommEvent
{
    using LoadEdges = std::vector<std::tuple<TID, EID, AddrSet>>;
    /**< vector of:
     *     producer thread id,
     *     producer event id,
     *     addr range
     * for reads to data written by another thread
     */

    LoadEdges comms;
    bool is_empty;

    STInstrEvent &instr_ev;

    TID &thread_id;
    EID &event_id;
    std::string logmsg;
    const std::shared_ptr<spdlog::logger> &logger;

  public:
    STCommEvent(TID &tid, EID &eid, const std::shared_ptr<spdlog::logger> &logger,
                STInstrEvent &instr_ev);
    void flush();

    /**
     * Adds communication edges originated from a single load/read primitive.
     * Use this function when reading data that was written by a different thread.
     *
     * Expected to by called byte-by-byte.
     * That is, successive calls to this function imply a continuity
     * between addresses. If the first call specifies address 0x0000,
     * and the next call specifies address 0x0008, then it implies
     * addresses 0x0000-0x0008 were all read, instead of non-consecutively read.
     *
     * Use STEvent::flush() between different read primitives.
     */
    void addEdge(const TID writer, const EID writer_event, const Addr addr);

  private:
    void reset();
};


/**
 * A SynchroTrace Synchronization Event.
 *
 * Synchronization events mark thread spawns, joins, barriers, locks, et al.
 * Expected use is to flush immediately; these events do not aggregate like
 * SynchroTrace Compute or Communication events.
 *
 * Synchronizatin events are expected to be immediately logged.
 */
using STSyncType = unsigned char;
class STSyncEvent
{
    STSyncType type = 0;
    Addr sync_addr = 0;

    TID &thread_id;
    EID &event_id;
    std::string logmsg;
    const std::shared_ptr<spdlog::logger> &logger;

  public:
    STSyncEvent(TID &tid, EID &eid, const std::shared_ptr<spdlog::logger> &logger);

    /* only behavior is immediate flush */
    void flush(const STSyncType type, const Addr sync_addr, const Addr MUTEX_HOTFIX);
};

}; //end namespace STGen

#endif
