===========
Subsampling
===========

Functions
=========

This library provides functions for subsampling either a single batch or multiple batches from a pool of bitstrings.

.. doxygenfunction:: Qiskit::addon::sqd::subsample(const BitstringVectorType &, const WeightVectorType &, unsigned int, RNGType &)
.. doxygenfunction:: Qiskit::addon::sqd::subsample(BatchVectorType &, const BitstringVectorType &, const WeightVectorType &, unsigned int, RNGType &)

.. doxygenfunction:: Qiskit::addon::sqd::subsample_multiple_batches(const BitstringVectorType &, const WeightVectorType &, unsigned int, unsigned int, RNGType &)
.. doxygenfunction:: Qiskit::addon::sqd::subsample_multiple_batches(BatchesVectorType &, const BitstringVectorType &, const WeightVectorType &, unsigned int, unsigned int, RNGType &)
