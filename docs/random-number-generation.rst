************************************
Random number generation
************************************

This library never creates a random number generator of its own.  Every routine that consumes randomness takes a generator by reference, and any type satisfying the standard `uniform_random_bit_generator <https://en.cppreference.com/w/cpp/numeric/random/uniform_random_bit_generator>`__ requirements may be used, including the engines in the ``<random>`` header.  Seeding, and therefore reproducibility, is the caller's to control.

A single-threaded run is reproducible in the ordinary way: the same generator, seeded the same way, yields the same result.  Parallelism complicates this, because a work item's randomness must not depend on which thread happened to run it.  Rather than drawing from the caller's generator sequentially -- which would serialize the loop and make results depend on scheduling -- a parallelized routine gives each unit of work its own independent random stream, derived from the caller's generator.  How reproducible the result is then depends on what kind of generator was supplied.

At present ``recover_configurations`` is the only routine with a parallel code path, so it is the only one to which the tiers below apply.  The other routines that consume randomness draw from the caller's generator sequentially and are reproducible in the ordinary single-threaded way, whether or not OpenMP is enabled.  The mechanism is written to be reusable, so further routines may adopt it in a later release.

Counter-based engines
---------------------

A counter-based generator computes its output directly from a counter, so it can be positioned at an arbitrary point in its own output rather than having to be advanced there one draw at a time.  A parallelized routine exploits that by assigning each work item a stream determined by its *index*, so item ``i`` draws the same values no matter which thread computes it.  Results are then **identical for any number of threads, and identical between a serial build and an OpenMP build**: both code paths key each work item the same way, from the same base seed.  If you need results that do not vary with the thread count, or across whether OpenMP was enabled, supply a counter-based engine.

Separation between substreams here is a matter of arithmetic rather than of statistics: distinct indices address disjoint regions of the counter space, which holds for any number of items and any number of draws per item.  That matters because a work item's draw count is not bounded in advance.

The C++26 `std::philox_engine <https://en.cppreference.com/w/cpp/numeric/random/philox_engine>`__ is such an engine.  The support is not specific to it: a generator qualifies by naming a counter type -- either a nested ``counter_type``, or a static ``word_count`` from which the standard ``std::array<result_type, word_count>`` shape is derived -- and accepting that type in ``set_counter``, following ``std::philox_engine``'s convention that the counter is supplied in reverse word order.

Other engines
-------------

Any other generator must be seedable, meaning it provides ``seed()``.  Each *thread* then receives its own independently seeded copy, and work items are distributed across those threads by OpenMP.  The results remain statistically valid, but which stream a given work item draws from depends on which thread runs it, so **the output depends on the number of threads**.

This is weaker than the counter-based tier because an ordinary engine cannot be positioned at a chosen point in its output without being advanced there one draw at a time, so a per-item stream would have to come from re-seeding rather than from addressing.  Seeding by index would then rest on distinct seeds producing well-separated streams -- a statistical expectation rather than the arithmetic guarantee above.  Per-thread seeding is used instead, which is why the thread count shows through.

Such an engine does agree between a serial build and an OpenMP build running on **one** thread: the serial path performs the same seeding an OpenMP build would perform for thread 0.  That makes the single-threaded case a usable reference point when comparing a parallel run against a serial one.

A generator that is neither counter-based nor seedable is rejected at compile time when OpenMP is enabled, with a diagnostic naming both requirements.  Such a generator remains usable in a serial build, where only the ``uniform_random_bit_generator`` requirements apply.

See :doc:`compilation-flags` for how OpenMP is enabled.
