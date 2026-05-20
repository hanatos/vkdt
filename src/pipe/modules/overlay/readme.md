# overlay: render text overlay

## parameters

* `text l` text for the upper left corner
* `text r` text for the upper right corner
* `perf l` latency of this module will be appended to the left text (milliseconds, requires -d perf)
* `perf r` latency of this module will be appended to the right text (milliseconds, requires -d perf)

## connectors

* `output` low dynamic range anti-aliased text with alpha channel
