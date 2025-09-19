Compile example app on the target machine
=========================================

Option 1: Manual compilation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Install dependencies
^^^^^^^^^^^^^^^^^^^^

- C++ compiler (must support C++17 or later)
- cmake
- MPI implementation
- ...

On RHEL systems, some of these can be installed with

.. code:: sh

    export sudo dnf install cmake mpich-devel

Make sure ``mpicxx`` is in ``$PATH``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

For instance, with ``mpich``, this may look like

.. code:: sh

    export PATH=/usr/lib64/mpich/bin:$PATH
    export LD_LIBRARY_PATH=/usr/lib64/mpich/lib:$LD_LIBRARY_PATH

Obtain the addon source code
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code:: sh

    git clone https://github.com/Qiskit/qiskit-addon-sqd-hpc.git
    cd qiskit-addon-sqd-hpc
    git submodule update --init --recursive

Build using ``cmake``
^^^^^^^^^^^^^^^^^^^^^

.. code:: sh

    mkdir -p build
    cd build
    cmake ..
    cmake --build . -j4

Option 2: Compile using Spack
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
