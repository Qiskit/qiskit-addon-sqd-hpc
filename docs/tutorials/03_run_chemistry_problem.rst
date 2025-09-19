Run ``sqd-hdf5-chemistry`` parallel MPI program
===============================================

Run the example app in a terminal session
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

From ``build/`` directory:

.. code:: sh

    mpiexec -n 6 ./sqd-hdf5-chemistry ../docs/tutorials/sqd_input_file.hdf5

will run problem using 6 MPI worker processes.

Run the example app in a Slurm job
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create job submission script
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Create a file ``sqd_submission.sh`` with the following contents.  You will likely need to read your cluster documentation to tweak additional options.

.. code:: sh

    #!/bin/bash

    #SBATCH --job-name=sqd_chemistry
    #SBATCH --nodes=2
    #SBATCH --ntasks-per-node=6
    #SBATCH --cpus-per-task=4

    # Fail immediately upon errors in this shell script
    set -euo pipefail

    # Load module(s) (example, based on OpenHPC)
    module load gnu15 mpich

    # Set OpenMP number of threads (per worker)
    export OMP_NUM_THREADS=$SLURM_CPUS_PER_TASK

    # Run parallel program
    srun /path/to/sqd-hdf5-chemistry /path/to/sqd_input_file.hdf5

Submit job
^^^^^^^^^^

.. code:: sh

    sbatch sqd_submission.sh

Monitor status
^^^^^^^^^^^^^^

.. code:: sh

    squeue

Collect output file
^^^^^^^^^^^^^^^^^^^
