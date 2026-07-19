# Reference Resources

This directory keeps supporting Bosch challenge files out of the repository
root. These files provide research context or tool-generated support; they are
not dependencies of `cpssim_core`.

- `Invited Paper - Physics-Driven Real-Time CPS Challenge.pdf`: challenge
  paper used as contextual documentation and cited by the reference metadata.
- `Bosch Challenge presentation at RTAS26.pdf`: presentation material for the
  challenge.
- `cps_single_vehicle_v10.slxc`: supplied Simulink cache file. The editable
  model remains `simulink/cps_single_vehicle_v10.slx` and is the source of
  truth.
- `BOSCH_CHALLENGE_README.md`: README from the original Bosch
  challenge repository, preserved after CPSSim became a standalone repository.

`LateralMotionControl.fmu` intentionally remains at the repository root. The
supplied Simulink model stores that relative filename internally, so keeping
the FMU there preserves the original MATLAB/Simulink workflow.
