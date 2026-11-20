# Media tools

`cutout_chromakey.py` removes a strict `#00FF00` background while protecting
cyan/turquoise details and optionally applying edge despill. Install its pinned
runtime environment with:

```bash
tools/arena/bootstrap.sh --art
/tmp/mediaenv/bin/cutout-chromakey input.png output.png --mode auto --crop
```

The tool was ported from `Creative36711/advanced-randomizer` commit
`cda1b7d9b12beefc75b8f7cd440b8dd239fec8ad`.
