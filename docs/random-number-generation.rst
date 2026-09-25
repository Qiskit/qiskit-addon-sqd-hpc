************************************
Random number generation
************************************

This library never creates a random number generator of its own.  Every routine that consumes randomness takes a generator by reference, and any type satisfying the standard `uniform_random_bit_generator <https://en.cppreference.com/w/cpp/numeric/random/uniform_random_bit_generator>`__ requirements may be used, including the engines in the ``<random>`` header.  Seeding, and therefore reproducibility, is the caller's to control.

What "reproducible" means here
------------------------------

A single-threaded run is reproducible in the ordinary way: the same generator, seeded the same way, yields the same result.

Parallelism complicates this, because a work item's randomness must not depend on which thread happened to run it.  Rather than drawing from the caller's generator sequentially -- which would serialize the loop and make results depend on scheduling -- the parallel code paths give each unit of work its own independent random stream, derived from the caller's generator.  How reproducible the result is then depends on what kind of generator was supplied.

Counter-based engines
---------------------

A counter-based generator computes its output directly from a counter, so it can be positioned at an arbitrary point in its own output rather than having to be advanced there one draw at a time.  This library exploits that by assigning each work item a stream determined by its *index*, so item ``i`` draws the same values no matter which thread computes it.  **Results are then identical for any number of threads.**

The C++26 `std::philox_engine <https://en.cppreference.com/w/cpp/numeric/random/philox_engine>`__ is such an engine.  The support is not specific to it: a generator qualifies by naming a counter type -- either a nested ``counter_type``, or a static ``word_count`` from which the standard ``std::array<result_type, word_count>`` shape is derived -- and accepting that type in ``set_counter``, following ``std::philox_engine``'s convention that the counter is supplied in reverse word order.

Because a work item's stream is determined by its index alone, the serial code path keys items the same way, so **a counter-based engine also gives the same answer in a serial build and an OpenMP one**.  If you need results that do not vary with the thread count, or across whether OpenMP was enabled, supply a counter-based engine.

Other engines
-------------

Any other generator must be seedable, meaning it provides ``seed()``.  Each thread then receives its own independently seeded copy.  The results remain statistically valid, but which stream a given work item draws from depends on which thread runs it, so **the output depends on the number of threads**.

The weaker guarantee is inherent rather than an omission.  Positioning an ordinary engine per work item means re-seeding it, which costs roughly two orders of magnitude more than a counter-based engine's ``set_counter`` -- but the more fundamental problem is that re-seeding cannot promise distinct items get distinct streams.  A scalar seed reaches only its own width of the engine's state, so two items eventually collide and then draw *identical* sequences for as long as either keeps drawing; with a 32-bit seed that is a near-certainty well inside the workload sizes this library targets.  A counter-based engine avoids this by construction: distinct indices address disjoint regions of the counter space, which is a matter of arithmetic and holds for any number of items and any number of draws.

A generator that is neither counter-based nor seedable is rejected at compile time when OpenMP is enabled, with a diagnostic naming both requirements.  Such a generator remains usable in a serial build, where only the ``uniform_random_bit_generator`` requirements apply.

Serial and parallel builds
--------------------------

A counter-based engine gives the same results in a serial build and an OpenMP build, at any thread count.  Both paths key each work item by its index, and the serial path pays essentially nothing for doing so: positioning the engine discards its output buffer, which a loop that draws continuously would refill on its next draw in any case.

Any other engine agrees between a serial build and an OpenMP build running on **one** thread.  The serial path performs the same seeding an OpenMP build would perform for thread 0, so the single-threaded case is a usable reference point when comparing a parallel run against a serial one.  Beyond one thread the results differ, necessarily: each thread draws from its own independently seeded copy, so which stream a work item sees depends on which thread runs it.  See :doc:`compilation-flags` for how OpenMP is enabled.
