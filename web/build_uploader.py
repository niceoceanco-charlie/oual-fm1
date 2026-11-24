#!/usr/bin/env python3
"""Builds a single-file oual pack uploader page with a pack inside.   usage: web/build_uploader.py pack.lfp "Pack name" out.html
The page (proto.js, midi.js from the lo-fi sampler's web editor, GPL-3.0) holds the pack as base64, shows what is in it, and sends it to the FM-1 over Web MIDI."""
import base64, os, sys
here = os.path.dirname(os.path.abspath(__file__))
pack, name, out = sys.argv[1:4]
t = open(os.path.join(here, 'uploader.template.html')).read()
t = t.replace('__PROTO__', open(os.path.join(here, 'proto.js')).read()).replace('__MIDI__', open(os.path.join(here, 'midi.js')).read())
t = t.replace('__PACK__', base64.b64encode(open(pack, 'rb').read()).decode()).replace('__NAME__', name.replace('"', ''))
open(out, 'w').write(t); print('wrote', out, len(t), 'bytes')
