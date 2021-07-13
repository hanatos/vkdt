# parameter optimiser (fit)

as the `cli`, this runs in headless mode. use it to optimise
a set of module parameters for a target output.

```
usage: vkdt-fit -g <graph.cfg>
    [-d verbosity]                set log verbosity (none,mem,perf,pipe,cli,err,all)
    [--param m:i:p]               add a parameter line to optimise. has to be float
    [--target m:i:p]              set the given module:inst:param as target for optimisation
    [--config]                    everything after this will be interpreted as additional cfg lines
```

initial parameters and target will be taken from the graph config file passed on the command line.
