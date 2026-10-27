# Bounded image-upload batches on Metal

Large texture uploads could allocate hundreds of 16 MiB staging blocks in one
submission. Existing blocks remained busy under that same tick, so the pool grew
before the GPU could complete work and make their space reusable.

The renderer now submits and waits after completed image uploads accumulate
64 MiB of guest-image sizes. The original upload method finishes first, including
its transfer marker. Pending barriers are flushed and Scheduler::Finish submits
before waiting. Existing staging completion/ownership checks are preserved.
The budget resets on an external tick change and saturates without overflow.

Enabled by default only for the Metal frame path. Set `SOTC_UPLOAD_BATCH=0` for
the previous behavior. `SOTC_UPLOAD_BATCH_TRIAL=1` prints optional batch boundaries;
it is not required to activate the fix. Other backends retain their behavior.
This is an upload-work budget, not a total GPU-memory ceiling. One oversized image
is not split. Waiting can introduce pauses; tune only with further measurements.

In a local native-runtime test, stream-block creation dropped from243 to14
(3888 to224 MiB capacity), all67 batch waits completed, and the user reached
controllable gameplay. The run later reported Metal insufficient memory at about
12.53 GB: total memory pressure and graphical defects remain unresolved.

The gameplay evidence is from local runtime a27f0ca477bd. This commit backports
the same policy to the repository's existing b2ff9cd0 runtime without changing its
pin or publishing private diagnostics. The public adapter wraps its original
UploadImage body in UploadImageUnbatched so markers end before submission.
Gameplay on this older public pin has not been revalidated.

Validation: current local native renderer builds and loads; public adapter passes
C++ syntax checking against the matching runtime headers/dependencies; clean
overlay application and predecessor migration tests; CPU/UBSan budget tests
(threshold, reset, tick change, oversized input, backend selection and disable).

Run policy tests with:

```sh
python3 -m unittest discover -s tests -p 'test_upload_batch.py'
```
