# Example Simulation Data

This folder contains example data (in the specific subfolders `example_v_xx`) for a 150-second simulation with a time granularity of 0.1 ms.
The folder contains exemplary descriptions of the task set and network parameters, as well as subfolders with the input trajectory references for a given vehicle under different velocity targets.
The considered track is 1000 meters long.

One FMU for each vehicle must be instantiated, each using its own copy of the input traces.

Each vehicle is also associated its own control chain, i.e., one full task set must be added in the scheduling simulation for each vehicle.

**Copyright**: (c) 2026 Robert Bosch GmbH

**License**: AGPL-3.0

**Authors**: Paolo Pazzaglia, Kevin Schmidt, Laura Beermann, Dirk Ziegenbein and Arne Hamann

## Contents

This example includes the following components:

1. Reference traces for three different peak vehicle velocities (10 m/s, 12.5 m/s and 15 m/s) as `.csv` files inside each corresponding subfolder. In detail it contains:
    *    Feedforward references (`feedforward_sequence_0.csv` and `feedforward_sequence_1.csv`).
    *    Velocity profiles corresponding to each feedforward reference (`velocity.csv`).
    *    Traces of the object's position in (x, y) Cartesian coordinates along the track (`x_position_track.csv` and `y_position_track.csv`).
    *    Time base (`time_vector.csv`).
    *    Timestamps for starting of input traces, corresponding to starting positions of the vehicles spaced 100 meters.
    *    Note: higher velocities represent harder scenarios to control, with increased sensitivity to delays and missed updates in the control chain.

2. System Parameters:
    *    A table detailing exemplary tasks parameters (periods, execution times).
    *    A table outlining the network parameters (periodicity of transmission, delays).

Each trace represents a 150-second simulation with a granularity of 0.1 ms (same as the phyisical simulation step of the FMU).
All traces are normalized to start at x(0) = 0, y(0) = 0.

**Note**: The traces can be shifted in time to simulate different starting points in the track for different vehicles. For example, to start a trace at t = 5s, simply offset the data accordingly.

## Usage

For each vehicle considered in the simulation:
* Instantiate its corresponding FMU in your master simulator (e.g., Simulink).
* Create its corresponding task set, using the parameters in the table, to be added to your scheduling simulation.
* Choose one velocity reference (10, 12.5 or 15 m/s), parse the corresponding trajectory and velocity vectors and re-align them with the timestamp of the chosen starting point in the track (presented in multiples of 100 meters).

## Run with CPSSim

On the supported Ubuntu development environment, CPSSim can load and execute
each supplied dataset through its normal scheduling, fixed-delay network,
Bosch-trigger, and FMI functional path. From the repository root, run the
default `example_v_10` dataset with the validated shared-cloud baseline:

```bash
make bosch-example
```

Select another dataset, baseline, or shorter inclusive integer-tick horizon:

```bash
make bosch-example \
  BOSCH_EXAMPLE_DIR=examples/example_v_15 \
  BOSCH_SCENARIO=dedicated \
  BOSCH_STOP_TICK=150000
```

Run all three datasets through their full supplied horizon (ticks `0` through
`1499999`, ending at `149.9999` seconds):

```bash
make bosch-examples
```

The loader requires all six CSV files, checks equal row counts and finite
values, and validates `time_vector.csv` as exact consecutive 0.1 ms integer
ticks. The Bosch v10 FMU consumes both feedforward columns and `velocity.csv`;
the x/y position columns are validated reference data and are not FMU inputs.

The example package intentionally leaves scheduling choices to the user.
CPSSim therefore requires an explicit `dedicated` or `shared_cloud` run plan;
these are the deterministic, single-vehicle plans already validated against
the captured Bosch v10 timing oracle. Successful execution demonstrates that
the supplied trajectory formats work through the implemented simulator path.
It does not by itself validate multi-vehicle/multi-FMU runs, probabilistic PERT
execution times, three-core cloud scaling, or satisfaction of the performance
constraints in `constraints.md`.



