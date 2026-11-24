# Installing the oual build on an FM-1 (not done yet: needs Charlie's "flash" for step C)

What exists: the CI image (a flash page with the oual firmware inside), and `out/oual-pack-uploader.html` (made locally by `web/build_uploader.py`; it holds the 3-bit sounds, so it is not committed).
The oual firmware has its own identity, `FM-1_8990001`; the sampler is `FM-1_8000014`; stock V15 is `FM-1_015`.

1. Take `FM1-Lofi-Workbench.html` from the CI run's artifact (`oual-device-build`, kept 14 days). Open it in Chrome (Cmd+O), click **Update firmware**, tick the backup box, **Connect device**.
2. From **stock V15** (the unit is on stock now): the page calls it a migration, so click **Enable force flash and reconnect**, then **Force flash** and accept the warning (the same route as the sampler).
   From the **sampler** (`FM-1_8000014`): the page recognises the identity as an Open family one, so **Start update** may work without force flash; if the device does not answer the page's capability query it will offer force flash. Not tested.
3. The FM-1 restarts to the oual screen: HOME with NO PACK. Close the workbench tab.
4. Open `oual-pack-uploader.html`, **CONNECT**, pick `fm1 community`, **UPLOAD**. The FM-1 shows the transfer, then keys 1 to 10 switch the ten voices. GLO page: reverb and CPU PEAK.
5. To go back: the workbench page, **Recovery & force update**, **Install original V15** (as in step B).
