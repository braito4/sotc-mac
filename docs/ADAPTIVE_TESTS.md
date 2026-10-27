# Adaptive owner-authorized diagnostics

The owner authorized serial native runs with probe changes between observations.
The analyst environment's Metal precheck reports zero accessible devices; the
native Terminal UI is also unavailable to computer control. A GPU-capable owner
Terminal must start the local controller once:

```sh
python3 tools/run_adaptive_diagnostics.py --batch work/analysis/adaptive-tests-20261024
```

The controller accepts one fingerprinted JSON request at a time, with no default
round-count limit and a maximum 500-second game deadline. An optional positive
`--max-runs` bounds the run count; the default `0` leaves it unlimited. Requests select only an
existing comparison and deadline; they cannot supply commands. The fixed launcher
retains its prechecks, native window mode and cooperative child cancellation.
No request means no next run. The analyst reviews and preserves the completed
result, changes only justified probes, checks those changes and supplies the next
request. An incomplete previous game log, mismatched fingerprints, low disk space,
stall, missing child exit or cleanup failure prevents continuation.

The STOP file in the batch directory stops between rounds. Terminal Ctrl-C reaches
the foreground controller/launcher and supervisor; the existing supervisor owns
cooperative game cancellation. No new external process signaling is introduced.
The idle timeout ends the controller if no new request arrives. These bounds do
not guarantee recovery from a kernel/driver hang.

Once evidence is sufficient to prepare and test a correction, preserve the
reproducer and the decisive capture, then remove redundant probes while retaining
only the observations needed to validate that correction. A known failure family
or changed prefix is not sufficient by itself to retire its causal investigation.
Near-colossus startup remains a subsequent goal after current failures are fixed
and stable entry is established; use a compatible engine path or save, with its
identity and behavior checked separately.
