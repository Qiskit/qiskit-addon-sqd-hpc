======================
Configuration recovery
======================

Functions
=========

This library provides a public function for performing configuration recovery, with one overload for systems of spin-up and spin-down electrons and another for systems of a single species of particle (e.g., spinless fermions).

.. doxygenfunction:: Qiskit::addon::sqd::recover_configurations(const BitstringVectorType &, const WeightVectorType &, const std::array<std::vector<double>, 2> &, std::array<std::uint64_t, 2>, RNGType &)
.. doxygenfunction:: Qiskit::addon::sqd::recover_configurations(const BitstringVectorType &, const WeightVectorType &, const std::vector<double> &, std::uint64_t, RNGType &)
